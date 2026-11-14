#version 330 core
#define VARIANT 1
#define KEEP_ASPECT 1   // 1 = sprite keeps its own aspect ratio, 0 = stretch to square

uniform vec2  RENDERSIZE;
uniform float motion;
uniform float response;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_OnBeat;
uniform sampler2D syn_Media;
uniform float syn_MediaType;
out vec4 fragColor;

#define uBass (syn_BassLevel * response)
#define uMid  (syn_MidLevel * response)
#define uHigh (syn_HighLevel * response)
#define uBeat (syn_OnBeat * response)
#define uTime motion

const float PI  = 3.14159265;
const float TAU = 6.28318531;

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

float hash21(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * .1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3. - 2. * f);
  return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
             mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
  float v = 0., a = .5;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p);
    p = rot(.5) * p * 2.02;
    a *= .5;
  }
  return v;
}

vec3 pal(float t) { return .5 + .5 * cos(TAU * (t + vec3(0., .33, .67))); }

// Sprite lookup. Tiles with fract(); transparent when no sprite is loaded.
vec4 img(vec2 uv) {
  if (syn_MediaType <= 0.0) return vec4(0.0);
  vec2 f = fract(uv);
#if KEEP_ASPECT
  vec2 ts = vec2(textureSize(syn_Media, 0));
  float ar = ts.x / max(ts.y, 1.0);
  if (ar >= 1.0) f.y = (f.y - 0.5) * ar + 0.5;
  else           f.x = (f.x - 0.5) / ar + 0.5;
  if (f.x < 0.0 || f.x > 1.0 || f.y < 0.0 || f.y > 1.0) return vec4(0.0);
#endif
  return textureLod(syn_Media, f, 0.0);
}

vec3 imgOver(vec2 uv, vec3 bg) {
  vec4 t = img(uv);
  return mix(bg, t.rgb, t.a);
}


float lum(vec3 c) { return dot(c, vec3(.299, .587, .114)); }

float hash11(float p) { p = fract(p * .1031); p *= p + 33.33; p *= p + p; return fract(p); }

vec2 hash22(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.xx + p3.yz) * p3.zy);
}

float sdSeg(vec2 p, vec2 a, vec2 b) {
  vec2 pa = p - a, ba = b - a;
  float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
  return length(pa - ba * h);
}

// No FFT texture is declared, so a spectrum is synthesised from the three bands.
float spec(float x) {
  float b = mix(mix(uBass, uMid, smoothstep(.1, .5, x)), uHigh, smoothstep(.5, .95, x));
  return clamp(b * (.55 + .9 * noise(vec2(x * 18., uTime * 4.))), 0., 1.);
}

mat3 rotX(float a) { float c = cos(a), s = sin(a); return mat3(1., 0., 0., 0., c, -s, 0., s, c); }
mat3 rotY(float a) { float c = cos(a), s = sin(a); return mat3(c, 0., s, 0., 1., 0., -s, 0., c); }
mat3 rotZ(float a) { float c = cos(a), s = sin(a); return mat3(c, -s, 0., s, c, 0., 0., 0., 1.); }

float sdBox(vec3 p, vec3 b) {
  vec3 q = abs(p) - b;
  return length(max(q, 0.)) + min(max(q.x, max(q.y, q.z)), 0.);
}


float bolt(vec2 p, float seed, float amp) {
  float tk = floor(uTime * 12. + seed * 3.);
  float on = step(.25, hash21(vec2(tk, seed)));
  p.x += (fbm(vec2(p.y * 2.5 + seed * 7., tk * .7)) - .5) * amp;
  float d = abs(p.x);
  float core = .004 / (d + .004);
  float glow = .03 / (d + .03);
  return (core * .8 + glow * .25) * on;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float I = 0.;
  if (VARIANT == 0) I = bolt(p, 1., 1.3);
  if (VARIANT == 1) {
    I = bolt(p, 1., 1.2);
    I += .6 * bolt(p - vec2(.25, 0.) + vec2(0., .2), 2., 1.) * step(p.y, .5);
    I += .5 * bolt(p + vec2(.45, .2), 3., .9) * step(p.y, .3);
    I += .5 * bolt(p - vec2(.7, -.1), 4., .8) * step(p.y, .2);
  }
  if (VARIANT == 2) {
    for (int i = 0; i < 4; i++) {
      float fi = float(i);
      I += .6 * bolt(p - vec2(-1. + fi * .55, 0.), 5. + fi, .5);
      I += .6 * bolt(p.yx - vec2(-.6 + fi * .4, 0.), 9. + fi, .5);
    }
  }
  if (VARIANT == 3) {
    I += .8 * .06 / (length(p) + .06);
    for (int i = 0; i < 9; i++) {
      float fi = float(i);
      vec2 r = rot(fi * TAU / 9. + uTime * .3) * p;
      I += .7 * bolt(vec2(r.y, r.x), 11. + fi, .35) * step(0., r.x) * smoothstep(1., .25, r.x);
    }
  }
  float flash = .35 + uBass * .9 + uBeat * 2.;
  float sky = fbm(p * 2. + vec2(0., uTime * .1));
  vec3 col = pal(.6 + v * .1) * sky * .12 * (1. + I * 3.);
  vec4 t = img(p * .5 + .5);
  float lit = .1 + uBeat * 1.1 + uBass * .4 + clamp(I, 0., 2.) * .3;
  col += mix(vec3(0.), t.rgb, t.a) * lit;
  col += mix(pal(.55 + v * .1), vec3(1.), clamp(I, 0., 1.)) * I * flash * .6;
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

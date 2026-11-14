#version 330 core
#define VARIANT 2
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


vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float tb = floor(uTime * 8.);
  float hit = uBeat;
  vec2 off = vec2(0.);
  if (VARIANT == 0) {
    vec2 id = floor(p * vec2(6., 4.));
    float r = hash21(id + tb);
    off = (r > .85 - hit * .6) ? (hash22(id + tb + 1.3) - .5) * .8 : vec2(0.);
  }
  if (VARIANT == 1) {
    float row = floor(p.y * 30.);
    float r = hash21(vec2(row, tb));
    off.x = (r > .85 - hit * .4) ? (hash21(vec2(row, tb + 3.)) - .5) * (.4 + uBass) : 0.;
  }
  if (VARIANT == 3) {
    float melt = noise(vec2(p.x * 14., tb * .25)) * (.15 + hit * .6) * pow(1. - (p.y * .5 + .5), 2.);
    off.y = melt * 1.5;
    off.x = (noise(vec2(p.y * 6., uTime)) - .5) * .05 * (1. + hit * 4.);
  }
  vec2 uv = p * .55 + .5 + off;
  float ca = .004 + uHigh * .03 + hit * .03;
  vec2 cell = floor(p * vec2(20., 12.));
  float junk = step(.7, hash21(cell + tb * 1.7));
  vec3 bg = pal(hash21(cell + tb) + v * .2) * .28 * junk + pal(.6 + v * .1) * .05;
  vec3 col;
  if (VARIANT == 2) {
    vec2 oR = vec2(sin(tb * 1.3) * .06, cos(tb * .7) * .04) * (1. + hit * 3.);
    vec2 oB = -vec2(cos(tb * 1.9) * .06, sin(tb * 1.1) * .05) * (1. + hit * 3.);
    col = vec3(imgOver(uv + oR, bg).r, imgOver(uv, bg).g, imgOver(uv + oB, bg).b);
  } else {
    col = vec3(imgOver(uv + vec2(ca, 0.), bg).r, imgOver(uv, bg).g, imgOver(uv - vec2(ca, 0.), bg).b);
  }
  col *= .88 + .12 * sin(gl_FragCoord.y * 3.14159);
  if (hit > .8 && hash21(floor(p * vec2(6., 4.)) + tb) > .9) col = 1. - col;
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

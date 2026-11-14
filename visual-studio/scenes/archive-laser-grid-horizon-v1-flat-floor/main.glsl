#version 330 core
#define VARIANT 0
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
  vec2 q = p;
  if (VARIANT == 3) q.x = abs(q.x);
  float h = (VARIANT == 1) ? 0. : -.12;
  if (VARIANT == 2) h = -.05 - q.x * q.x * .2;
  float dy = (VARIANT == 1) ? abs(q.y) : (h - q.y);
  vec3 col;
  if (dy > .002) {
    float depth = 1. / dy;
    vec2 g = vec2(q.x * depth, depth + uTime * (1.5 + uBass * 3.));
    vec2 gd = .5 - abs(fract(g) - .5);
    float lw = clamp(.02 + .012 * depth, .02, .5);
    float ln = 1. - smoothstep(0., lw, min(gd.x, gd.y));
    float fade = 1. / (1. + depth * .06);
    vec4 t = img(g * .25);
    col = mix(pal(.7 + v * .12) * .06, t.rgb, t.a * .9);
    col += pal(g.y * .03 + v * .2) * ln * (.7 + uBeat * 1.5);
    col *= fade;
    col += pal(.85 + v * .1) * exp(-dy * 6.) * (.3 + uBass);
  } else {
    vec2 s = q - vec2(0., .12);
    vec3 sky = mix(pal(.95 + v * .1) * .45, pal(.6 + v * .1) * .05, clamp((q.y - h) * 1.2, 0., 1.));
    float sun = smoothstep(.37, .35, length(s)) * smoothstep(-.1, .3, sin(q.y * 80.) + q.y * 6. + .6);
    sky = mix(sky, mix(pal(.1 + v * .1), pal(.95 + v * .1), clamp(-s.y * 2. + .5, 0., 1.)) * 1.3, sun);
    float ang = atan(q.x, q.y - h + 1e-3);
    float beams = pow(.5 + .5 * cos(ang * 9. + uTime * .7), 20.) * exp(-(q.y - h) * .8);
    sky += pal(uTime * .1 + v * .25) * beams * (.25 + uBeat * 1.4);
    vec4 t = img(s * 1.2 + .5);
    col = mix(sky, t.rgb, t.a);
  }
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

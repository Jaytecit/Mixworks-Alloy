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


float F0(vec2 c, float tk) { return step(.5, hash21(c + tk * 13.7)); }
float F1(vec2 c, float tk) {
  float s = 0.;
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) s += F0(c + vec2(float(i), float(j)), tk);
  if (VARIANT == 2) return step(2.5, s) * step(s, 6.5);
  return step(4.5, s);
}
float F2(vec2 c, float tk) {
  float s = 0.;
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) s += F1(c + vec2(float(i), float(j)), tk);
  if (VARIANT == 2) return step(3.5, s) * step(s, 5.5);
  return step(4.5, s);
}
// Rule 30 grown from a single seed cell: x = column offset from the seed, n = generations
float rule30(float x, int n) {
  float a[64];
  for (int k = 0; k < 64; k++) {
    if (k > 2 * n) break;
    a[k] = (abs(x - float(n) + float(k)) < .5) ? 1. : 0.;
  }
  for (int s = 0; s < 24; s++) {
    if (s >= n) break;
    int w = 2 * n + 1 - 2 * (s + 1);
    for (int k = 0; k < 64; k++) {
      if (k >= w) break;
      a[k] = abs(a[k] - max(a[k + 1], a[k + 2]));
    }
  }
  return a[0];
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float N = (VARIANT == 1) ? 16. : (VARIANT == 3) ? 12. : 22.;
  vec2 q = p * N;
  vec2 id = floor(q), f = fract(q) - .5;
  float alive = 0.;
  float tk = floor(uTime * 2.5);
  if (VARIANT == 0 || VARIANT == 2) {
    alive = F2(id, tk);
  } else if (VARIANT == 1) {
    float scroll = floor(uTime * 3. + uBeat * 2.);
    float yy = id.y + scroll;
    float blk = floor(yy / 18.);
    float row = yy - blk * 18.;
    float offc = floor(hash21(vec2(blk, 1.)) * 8. - 4.);
    alive = rule30(id.x - offc, int(row));
  } else {
    int ax = int(abs(id.x)), ay = int(abs(id.y));
    int kk = int(floor(uTime * 1.5)) & 15;
    alive = (((ax + kk) & (ay + (kk >> 1))) == 0) ? 1. : 0.;
  }
  float h = hash21(id);
  vec4 t = img(f + .5);
  vec3 dead = pal(h + v * .2) * .06;
  vec3 live = mix(pal(h + uTime * .1 + v * .2) * .6, t.rgb, t.a) * (1. + uBeat * .8);
  vec3 col = mix(dead, live, alive);
  vec2 e = .5 - abs(f);
  col += pal(h) * smoothstep(.05, 0., min(e.x, e.y)) * (.1 + uHigh * .3);
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

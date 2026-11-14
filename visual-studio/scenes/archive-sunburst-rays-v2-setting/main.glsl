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


float godray(vec2 p, vec2 c, float sz) {
  vec2 d = p - c;
  float acc = 0., w = 1.;
  for (int i = 0; i < 28; i++) {
    vec2 s = p - d * (float(i) / 28. * .95);
    vec2 l = (s - c) / sz;
    float m = smoothstep(.12, .1, length(s - c)) * .7;
    if (syn_MediaType > 0.) {
      vec4 t = img(l + .5);
      m = max(m, lum(t.rgb) * t.a * step(max(abs(l.x), abs(l.y)), .5));
    }
    acc += m * w;
    w *= .94;
  }
  return acc / 12.;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec2 c = vec2(0., -.1);
  float sz = .9 + uBass * .1;
  if (VARIANT == 0) c = vec2(0., -.9 + .9 * (.5 + .5 * sin(uTime * .3)) + uBass * .1);
  if (VARIANT == 1) c = vec2(0., .1 - .5 * (.5 + .5 * sin(uTime * .2)));
  float acc;
  vec2 c1 = vec2(-.9, 0.), c2 = vec2(.9, 0.);
  if (VARIANT == 3) { sz = .7; acc = godray(p, c1, sz) + godray(p, c2, sz); }
  else acc = godray(p, c, sz);
  vec2 d = p - c;
  float a = atan(d.y, d.x);
  float rays = .55 + .45 * pow(.5 + .5 * cos(a * 14. + uTime * .6 * ((VARIANT == 1) ? -1. : 1.)), 3.);
  if (VARIANT == 2) rays *= .3 + uBeat * 1.5 + step(.5, fract(uTime * 8.)) * .5;
  vec3 col = pal(.7 + v * .1) * .05;
  col += pal(a / TAU * .5 + uTime * .05 + v * .2) * acc * rays * (.9 + uBass * 1.2);
  if (VARIANT == 1) col *= smoothstep(-.55, -.4, p.y) * .8 + .2;
  vec2 l1 = (p - ((VARIANT == 3) ? c1 : c)) / sz;
  vec4 t1 = img(l1 + .5);
  col = mix(col, t1.rgb * 1.1, t1.a * step(max(abs(l1.x), abs(l1.y)), .5));
  if (VARIANT == 3) {
    vec2 l2 = (p - c2) / sz;
    vec4 t2 = img(l2 + .5);
    col = mix(col, t2.rgb * 1.1, t2.a * step(max(abs(l2.x), abs(l2.y)), .5));
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

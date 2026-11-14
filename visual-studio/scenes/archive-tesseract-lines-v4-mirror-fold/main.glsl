#version 330 core
#define VARIANT 3
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


vec2 P4[16];
float Z4[16];
void buildTess(float scl, float ph) {
  float a1 = uTime * .4 + ph + uBass * .5, a2 = uTime * .27 + ph * 1.3, a3 = uTime * .33;
  for (int i = 0; i < 16; i++) {
    vec4 q = vec4(float(i & 1), float((i >> 1) & 1), float((i >> 2) & 1), float((i >> 3) & 1)) * 2. - 1.;
    float c = cos(a1), s = sin(a1);
    q.xw = vec2(c * q.x - s * q.w, s * q.x + c * q.w);
    c = cos(a2); s = sin(a2);
    q.yz = vec2(c * q.y - s * q.z, s * q.y + c * q.z);
    c = cos(a3); s = sin(a3);
    q.zw = vec2(c * q.z - s * q.w, s * q.z + c * q.w);
    float k = 1. / (2.6 - q.w);
    vec3 r3 = q.xyz * k;
    float k2 = 1. / (3.2 - r3.z);
    P4[i] = r3.xy * k2 * 1.9 * scl;
    Z4[i] = r3.z;
  }
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec2 pp = p;
  if (VARIANT == 3) pp = abs(pp) - vec2(.35, 0.);
  vec3 bg = imgOver(p * .5 + .5, pal(.6 + v * .1) * .04) * .3;
  vec3 acc = vec3(0.);
  int passes = (VARIANT == 1) ? 2 : 1;
  for (int ps = 0; ps < 2; ps++) {
    if (ps >= passes) break;
    if (ps == 0) buildTess(1. + uBeat * .06, 0.);
    else buildTess(.5, 2.6);
    for (int i = 0; i < 16; i++) {
      for (int b = 0; b < 4; b++) {
        if (((i >> b) & 1) == 0) {
          int j = i | (1 << b);
          float d = sdSeg(pp, P4[i], P4[j]);
          float w = exp(-d * 55.);
          float gl = .012 / (d + .012);
          vec3 tint = pal(float(i + j) * .04 + float(b) * .15 + uTime * .1 + v * .2);
          vec3 imc = imgOver(pp * .5 + .5, tint);
          acc += mix(tint, imc, .6) * (w * 1.4 + gl * .12) * (.6 + uBeat * 1.6 + uBass);
        }
      }
    }
  }
  vec3 col = bg + acc;
  if (VARIANT == 2) {
    for (int i = 0; i < 16; i++) {
      vec2 c = (pp - P4[i]) / (.16 * (1. + Z4[i] * .3));
      vec4 t = img(c + .5);
      col = mix(col, t.rgb, t.a * step(max(abs(c.x), abs(c.y)), .5));
    }
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

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


vec2 curveP(float t) {
  float s = .9 + uBass * .3;
  if (VARIANT == 0) return vec2(4. * cos(t) - cos(4. * t), 4. * sin(t) - sin(4. * t)) * .16 * s;
  if (VARIANT == 1) return vec2(4. * cos(t) + 1.5 * cos(4. * t), 4. * sin(t) - 1.5 * sin(4. * t)) * .15 * s;
  if (VARIANT == 2) return cos(7. * t) * vec2(cos(t), sin(t)) * .8 * s;
  return vec2(cos(t), sin(t)) * (.55 + .22 * cos(8. * t + uTime)) * 1.3 * s;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float N = (VARIANT == 1) ? 6. : 8.;
  float r = length(p), a = atan(p.y, p.x);
  float seg = TAU / N;
  a = mod(a + uTime * .05, seg);
  a = abs(a - seg * .5);
  vec2 q = r * vec2(cos(a), sin(a));
  vec3 col = imgOver(p * .4 + .5, pal(.6 + v * .1) * .05) * .3;
  float bestD = 1e9, bestT = 0.;
  for (int L = 0; L < 2; L++) {
    float sc = (L == 0) ? 1. : .55;
    mat2 R = rot(uTime * .2 * ((L == 0) ? 1. : -1.));
    vec2 pa = R * curveP(0.) * sc;
    for (int i = 1; i <= 90; i++) {
      float t = float(i) / 90. * TAU;
      vec2 pb = R * curveP(t) * sc;
      float d = sdSeg(q, pa, pb);
      if (d < bestD) { bestD = d; bestT = t + float(L) * 7.; }
      pa = pb;
    }
  }
  float w = .012 + .01 * uBass;
  float line = smoothstep(w, w * .4, bestD);
  float glow = exp(-bestD * 25.);
  col += pal(bestT * .15 + r + uTime * .1 + v * .2) * (line * 1.2 + glow * .3) * (.7 + uBeat * 1.3);
  vec4 t = img(p / .44 + .5);
  col = mix(col, t.rgb, t.a * smoothstep(.22, .2, r));
  col += pal(uTime * .2) * smoothstep(.02, 0., abs(r - .22)) * (.4 + uBeat * 1.5);
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

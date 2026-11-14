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


vec3 knot(float t, float pp, float qq, float k) {
  float r = k * cos(qq * t) + 2.;
  return vec3(r * cos(pp * t), r * sin(pp * t), -k * sin(qq * t));
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float pp = 2., qq = 3., k = 1.;
  if (VARIANT == 1) qq = 5.;
  if (VARIANT == 3) k = .5 + .5 * sin(uTime * .5);
  mat3 R = rotY(uTime * .4) * rotX(uTime * .27 + .6);
  int nc = (VARIANT == 2) ? 2 : 1;
  vec3 col = imgOver(p * .4 + .5, pal(.6 + v * .1) * .04) * .3 + pal(.6) * .03 / (length(p) + .3);
  float bestZ = 1e9, bc = 0., bt = 0., gmin = 1e9;
  for (int c = 0; c < 2; c++) {
    if (c >= nc) break;
    float phase = float(c) * PI / pp;
    vec3 pa = R * knot(phase, pp, qq, k) * .32;
    for (int i = 1; i <= 128; i++) {
      float t = float(i) / 128. * TAU + phase;
      vec3 pb = R * knot(t, pp, qq, k) * .32;
      vec2 sa = pa.xy / (1. + pa.z * .4) * 1.2;
      vec2 sb = pb.xy / (1. + pb.z * .4) * 1.2;
      float d = sdSeg(p, sa, sb);
      float z = (pa.z + pb.z) * .5;
      float thick = .05 / (1. + z * .4) * (1. + uBass * .4);
      gmin = min(gmin, d);
      if (d < thick && z < bestZ) {
        vec2 ab = sb - sa;
        float side = sign(ab.x * (p.y - sa.y) - ab.y * (p.x - sa.x));
        bestZ = z;
        bc = side * d / thick;
        bt = t;
      }
      pa = pb;
    }
  }
  col += pal(.55 + v * .1) * exp(-gmin * 18.) * .25;
  if (bestZ < 1e8) {
    float sh = sqrt(max(1. - bc * bc, 0.));
    vec4 tx = img(vec2(bt / TAU * 10., bc * .5 + .5));
    vec3 tc = pal(bt / TAU * 3. + uTime * .1 + v * .2) * (.4 + .6 * sh);
    tc = mix(tc, tx.rgb * (.5 + .6 * sh), tx.a * .9);
    tc += pal(bt / TAU * 3.) * pow(1. - abs(bc), 6.) * .5 * (.4 + uBeat);
    col = tc;
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

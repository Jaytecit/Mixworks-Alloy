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


mat3 RR[7];
float RAD[7];
float THK[7];
int NR;
float sdTor(vec3 p, float R, float r) {
  vec2 q = vec2(length(p.xz) - R, p.y);
  return length(q) - r;
}
float mapR(vec3 p) {
  float d = 1e9;
  for (int i = 0; i < 7; i++) {
    if (i >= NR) break;
    d = min(d, sdTor(RR[i] * p, RAD[i], THK[i]));
  }
  return d;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  NR = (VARIANT == 1) ? 7 : 3;
  for (int i = 0; i < 7; i++) {
    if (i >= NR) break;
    float fi = float(i);
    float spd = .3 + .13 * fi;
    RR[i] = rotX(uTime * spd + fi * 1.1) * rotZ(uTime * spd * .7 + fi * .8);
    if (VARIANT == 2) RR[i] = rotX(.9 + .15 * sin(uTime * .4 + fi)) * rotZ(fi * .55 + uTime * .2);
    float base = (VARIANT == 1) ? .3 + fi * .17 : .55 + fi * .35;
    RAD[i] = base * (1. + uBass * .1) * ((VARIANT == 3) ? 1. + uBeat * (.5 + .2 * fi) : 1.);
    THK[i] = (VARIANT == 1) ? .04 : .07 + .01 * fi;
  }
  vec3 ro = vec3(0., 0., -4.2);
  vec3 rd = normalize(vec3(p, 1.9));
  float t = 0.;
  bool hit = false;
  for (int i = 0; i < 64; i++) {
    float d = mapR(ro + rd * t);
    if (d < .002) { hit = true; break; }
    t += d * .85;
    if (t > 10.) break;
  }
  float rr = length(p);
  vec3 col = imgOver(p * .35 + .5, pal(.6 + v * .1) * .05) * .3;
  col += pal(.7 + v * .1) * .05 / (rr + .3);
  if (hit) {
    vec3 pos = ro + rd * t;
    float bd = 1e9;
    int wi = 0;
    for (int i = 0; i < 7; i++) {
      if (i >= NR) break;
      float dd = sdTor(RR[i] * pos, RAD[i], THK[i]);
      if (dd < bd) { bd = dd; wi = i; }
    }
    vec2 e = vec2(.003, 0.);
    vec3 n = normalize(vec3(mapR(pos + e.xyy) - mapR(pos - e.xyy),
                            mapR(pos + e.yxy) - mapR(pos - e.yxy),
                            mapR(pos + e.yyx) - mapR(pos - e.yyx)));
    vec3 lp = RR[wi] * pos;
    float maj = atan(lp.z, lp.x);
    vec2 qq = vec2(length(lp.xz) - RAD[wi], lp.y);
    float mnr = atan(qq.y, qq.x);
    vec4 tx = img(vec2(maj / TAU * (RAD[wi] / THK[wi]) - uTime * .1, mnr / TAU));
    vec3 tint = pal(float(wi) * .15 + v * .2 + uTime * .05);
    float diff = .3 + .7 * max(dot(n, normalize(vec3(.4, .7, -.5))), 0.);
    vec3 base = mix(tint * .5, tx.rgb * 1.1, tx.a) * diff;
    base += tint * pow(1. - max(dot(n, -rd), 0.), 2.) * (.5 + uBeat * 1.5 + uBass);
    col = base;
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

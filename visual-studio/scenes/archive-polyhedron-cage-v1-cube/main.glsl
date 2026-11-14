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


mat3 RM;
float gR;

// largest and second-largest plane distances for the solid (kind 0 cube, 1 octahedron, 2 icosahedron)
vec2 top2(vec3 p, int kind) {
  const float PHI = 1.618034, IP = .618034, K = .57735027;
  float c[10];
  int n = 3;
  if (kind == 0) {
    c[0] = abs(p.x); c[1] = abs(p.y); c[2] = abs(p.z);
  } else {
    c[0] = abs(p.x + p.y + p.z) * K;
    c[1] = abs(p.x + p.y - p.z) * K;
    c[2] = abs(p.x - p.y + p.z) * K;
    c[3] = abs(-p.x + p.y + p.z) * K;
    c[4] = abs(IP * p.y + PHI * p.z) * K;
    c[5] = abs(IP * p.y - PHI * p.z) * K;
    c[6] = abs(IP * p.x + PHI * p.y) * K;
    c[7] = abs(IP * p.x - PHI * p.y) * K;
    c[8] = abs(PHI * p.x + IP * p.z) * K;
    c[9] = abs(PHI * p.x - IP * p.z) * K;
    n = (kind == 1) ? 4 : 10;
  }
  float a = -1e9, b = -1e9;
  for (int i = 0; i < 10; i++) {
    if (i >= n) break;
    float x = c[i];
    if (x > a) { b = a; a = x; }
    else if (x > b) b = x;
  }
  return vec2(a, b);
}
float sdKind(vec3 q, int kind, float r) { return top2(q, kind).x - r; }

float mapObj(vec3 q) {
  if (VARIANT == 0) return sdKind(q, 0, gR);
  if (VARIANT == 1) return sdKind(q, 1, gR * 1.1);
  if (VARIANT == 2) return sdKind(q, 2, gR);
  return min(sdKind(q, 0, gR * .62), sdKind(rotY(.7) * q, 1, gR * 1.05));
}

vec3 scene(vec2 p) {
  float v = float(VARIANT);
  RM = rotY(uTime * .5 + uBass * .3) * rotX(uTime * .37);
  gR = .9 * (1. + uBass * .1 + uBeat * .08);
  vec3 ro = vec3(0., 0., -3.2);
  vec3 rd = normalize(vec3(p, 1.8));
  float t = 0.;
  bool hit = false;
  for (int i = 0; i < 56; i++) {
    float d = mapObj(RM * (ro + rd * t));
    if (d < .002) { hit = true; break; }
    t += d * .9;
    if (t > 8.) break;
  }
  float rr = length(p);
  vec3 col = imgOver(p * .35 + .5, pal(.6 + v * .1) * .05) * .35;
  col += pal(.7 + v * .1) * .05 / (rr + .3);
  if (hit) {
    vec3 q = RM * (ro + rd * t);
    int kind = (VARIANT == 3) ? 0 : VARIANT;
    vec3 qq = q;
    float rad = gR;
    if (VARIANT == 1) rad = gR * 1.1;
    if (VARIANT == 3) {
      float dC = sdKind(q, 0, gR * .62);
      float dO = sdKind(rotY(.7) * q, 1, gR * 1.05);
      if (dC < dO) { kind = 0; qq = q; rad = gR * .62; }
      else { kind = 1; qq = rotY(.7) * q; rad = gR * 1.05; }
    }
    vec2 e = vec2(.003, 0.);
    vec3 n = normalize(vec3(sdKind(qq + e.xyy, kind, rad) - sdKind(qq - e.xyy, kind, rad),
                            sdKind(qq + e.yxy, kind, rad) - sdKind(qq - e.yxy, kind, rad),
                            sdKind(qq + e.yyx, kind, rad) - sdKind(qq - e.yyx, kind, rad)));
    vec3 t1 = normalize(cross(n, (abs(n.y) < .9) ? vec3(0., 1., 0.) : vec3(1., 0., 0.)));
    vec3 t2 = cross(n, t1);
    vec2 uv = vec2(dot(qq, t1), dot(qq, t2)) / (rad * 1.3) + .5;
    vec4 tx = img(uv);
    vec2 tt = top2(qq, kind);
    float edge = 1. - smoothstep(0., .06, tt.x - tt.y);
    float h = hash21(floor(n.xy * 7.) + floor(n.z * 7.) * 3.7 + float(kind) * 3.);
    vec3 face = pal(h + v * .2 + uTime * .05) * .35;
    float diff = .3 + .7 * max(dot(n, normalize(vec3(.5, .7, -.5))), 0.);
    vec3 base = mix(face, tx.rgb * 1.1, tx.a) * diff;
    base += pal(h + uTime * .15) * edge * (.7 + uBeat * 2. + uHigh);
    base += pal(.6 + h) * pow(1. - max(dot(n, -rd), 0.), 3.) * .25;
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

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


vec3 envMap(vec3 rd) {
  vec3 c = pal(rd.y * .5 + rd.x * .3 + uTime * .05) * .35 + .12;
  c += pow(max(0., sin(rd.y * 9. + uTime)), 10.) * .8;
  vec4 t = img(rd.xy * .6 + .5);
  c = mix(c, t.rgb * 1.2, t.a * step(rd.z, 0.));
  return c;
}
float mapCh(vec3 p) {
  float t = uTime;
  float amp = .5 + uBass * 1.5;
  if (VARIANT == 0) {
    return length(p) - .9 - .12 * amp * sin(5. * p.x + t) * sin(5. * p.y + t * 1.3) * sin(5. * p.z - t);
  }
  if (VARIANT == 1) {
    float hgt = sin(p.x * 3. + t) * cos(p.z * 3. - t * .8) * .15 * (1. + uBass * 2.);
    return p.y + .5 - hgt;
  }
  if (VARIANT == 2) {
    p = rotY(t * .5) * rotX(t * .37) * p;
    vec2 q = vec2(length(p.xz) - .85, p.y);
    float a = atan(p.z, p.x);
    return length(q) - .3 - .06 * amp * sin(a * 6. + t * 2.);
  }
  float d = p.y + .8;
  float k = .4;
  for (int i = 0; i < 4; i++) {
    float fi = float(i);
    vec3 c = vec3(sin(t * .7 + fi * 1.9) * .8, -.3 + abs(sin(t * 1.3 + fi * 2.3)) * (.6 + uBass), cos(t * .6 + fi * 2.7) * .6);
    float s = length(p - c) - (.3 + uBeat * .1);
    float hh = clamp(.5 + .5 * (d - s) / k, 0., 1.);
    d = mix(d, s, hh) - k * hh * (1. - hh);
  }
  return d;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec3 ro = vec3(0., (VARIANT == 1) ? .9 : ((VARIANT == 3) ? .4 : 0.), -3.2);
  vec3 rd = normalize(vec3(p, 1.7));
  if (VARIANT == 1) rd = normalize(vec3(p.x, p.y - .45, 1.7));
  float t = 0.;
  bool hit = false;
  for (int i = 0; i < 64; i++) {
    float d = mapCh(ro + rd * t);
    if (d < .002) { hit = true; break; }
    t += d * .7;
    if (t > 12.) break;
  }
  vec3 col = imgOver(p * .4 + .5, pal(.6 + v * .1) * .05) * .35;
  if (hit) {
    vec3 pos = ro + rd * t;
    vec2 e = vec2(.003, 0.);
    vec3 n = normalize(vec3(mapCh(pos + e.xyy) - mapCh(pos - e.xyy),
                            mapCh(pos + e.yxy) - mapCh(pos - e.yxy),
                            mapCh(pos + e.yyx) - mapCh(pos - e.yyx)));
    vec3 rr = reflect(rd, n);
    float fres = pow(1. - max(dot(-rd, n), 0.), 3.);
    col = envMap(rr) * (.55 + .6 * fres) + pow(max(dot(rr, normalize(vec3(.4, .7, -.6))), 0.), 30.);
    col *= 1. + uBeat * .5;
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

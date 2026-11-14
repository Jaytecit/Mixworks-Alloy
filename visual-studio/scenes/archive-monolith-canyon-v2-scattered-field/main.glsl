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


float pillarH(vec2 id) {
  float r = hash21(id);
  return .8 + 2.6 * r * (.35 + 1.1 * mix(uBass, uHigh, r));
}
bool pillarOn(vec2 id) {
  float r = hash21(id + 7.7);
  if (VARIANT == 1) return r > .45;
  return (id.x > 0. || id.x < -1.) && r > .2;
}
vec3 foldP(vec3 p) { if (VARIANT == 3) p.x = abs(p.x); return p; }
float map(vec3 p) {
  p = foldP(p);
  float d = p.y;
  vec2 id = floor(p.xz / 3.);
  if (pillarOn(id)) {
    vec2 l = p.xz - 3. * (id + .5);
    float h = pillarH(id);
    d = min(d, sdBox(vec3(l.x, p.y - h, l.y), vec3(.45, h, .45)));
  }
  return min(d, 1.);
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec3 ro = vec3(0., 1.5 + ((VARIANT == 1) ? 2.2 : 0.), uTime * 4. + uBass * 1.5);
  if (VARIANT == 2) ro.x = sin(uTime * .4) * 1.6;
  vec3 rd = normalize(vec3(p.x, p.y - ((VARIANT == 1) ? .4 : 0.), 1.5));
  if (VARIANT == 2) rd.xy = rot(uTime * .25) * rd.xy;
  ro.y += uBeat * .06 * sin(uTime * 40.);
  float t = .1;
  bool hit = false;
  for (int i = 0; i < 60; i++) {
    float d = map(ro + rd * t);
    if (d < .003) { hit = true; break; }
    t += d * .9;
    if (t > 70.) break;
  }
  vec3 sky = pal(.75 + v * .1) * (.12 + .25 * exp(-abs(rd.y) * 5.));
  vec3 col = sky;
  if (hit) {
    vec3 pos = ro + rd * t;
    vec2 e = vec2(.005, 0.);
    vec3 n = normalize(vec3(map(pos + e.xyy) - map(pos - e.xyy),
                            map(pos + e.yxy) - map(pos - e.yxy),
                            map(pos + e.yyx) - map(pos - e.yyx)));
    vec3 pp = foldP(pos);
    vec2 id = floor(pp.xz / 3.);
    vec2 l = pp.xz - 3. * (id + .5);
    float hh = hash21(id);
    vec3 base;
    if (pp.y < .02) {
      vec2 gd = .5 - abs(fract(pp.xz / 1.5) - .5);
      float ln = 1. - smoothstep(0., .03 + t * .004, min(gd.x, gd.y));
      base = pal(.6 + v * .1) * .04 + pal(pp.z * .02 + v * .2) * ln * (.5 + uBeat * 1.5);
    } else {
      vec2 fuv = (abs(n.x) > abs(n.z)) ? vec2(l.y, pp.y) : vec2(l.x, pp.y);
      vec4 tx = img(vec2(fuv.x / .9 + .5, fuv.y / .9));
      vec3 side = pal(hh * .5 + v * .2 + uTime * .04) * .35;
      float top = step(.9, n.y);
      base = mix(side, tx.rgb * 1.1, tx.a * (1. - top));
      float diff = .35 + .65 * max(dot(n, normalize(vec3(.3, .8, -.5))), 0.);
      base *= diff;
      float e2 = max(abs(l.x), abs(l.y));
      base += pal(hh + uTime * .1) * smoothstep(.41, .45, e2) * (.4 + uBeat * 1.5 + uHigh);
      base += pal(hh + uTime * .1 + .3) * top * .5 * (uBass + .2);
    }
    col = mix(base, sky, 1. - exp(-t * .03));
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

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


float terH(vec2 xz) {
  float t = uTime * .7;
  float h = sin(xz.x * .7 + t) * cos(xz.y * .5 - t * .8) * (.35 + uBass * 1.2);
  h += sin(xz.x * 1.9 - xz.y * 1.3 + t * 1.7) * (.12 + uMid * .35);
  h += sin(xz.x * 4.1 + xz.y * 3.7 - t * 3.1) * (.04 + uHigh * .15);
  if (VARIANT == 2) h = h * .6 + smoothstep(1.5, 4., abs(xz.x)) * 4.;
  if (VARIANT == 3) h *= .5;
  return h;
}
vec3 terSky(vec3 rd) {
  float v = float(VARIANT);
  vec2 s = rd.xy / max(rd.z, .1) * 1.7;
  vec3 c = mix(pal(.95 + v * .05) * .45, pal(.6) * .05, clamp(rd.y * 2.2, 0., 1.));
  vec2 sp = s - vec2(0., .42);
  float fw = step(0., rd.z) * step(0., rd.y);
  float sun = smoothstep(.45, .43, length(sp)) * fw;
  c = mix(c, pal(.08) * 1.3, sun * .4);
  vec4 t = img(sp / .9 + .5);
  c = mix(c, t.rgb, t.a * step(max(abs(sp.x), abs(sp.y)), .45) * fw);
  return c;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec3 ro = vec3(0., 1.4 + ((VARIANT == 2) ? .6 : 0.), uTime * 3. + uBass);
  vec3 rd = normalize(vec3(p.x, p.y - .12, 1.7));
  float t = .2;
  bool hit = false;
  for (int i = 0; i < 90; i++) {
    vec3 pos = ro + rd * t;
    float d = pos.y - terH(pos.xz);
    if (d < .003 * t) { hit = true; break; }
    t += max(d * .5, .01 * t);
    if (t > 60.) break;
  }
  vec3 col = terSky(rd);
  if (hit) {
    vec3 pos = ro + rd * t;
    vec2 e = vec2(.02, 0.);
    vec3 n = normalize(vec3(terH(pos.xz - e.xy) - terH(pos.xz + e.xy), .04, terH(pos.xz - e.yx) - terH(pos.xz + e.yx)));
    vec2 gd = .5 - abs(fract(pos.xz) - .5);
    float ln = 1. - smoothstep(0., .02 + t * .003, min(gd.x, gd.y));
    float hh = pos.y;
    vec3 body;
    if (VARIANT == 0) {
      body = pal(hh * .3 + pos.z * .02 + v * .2) * .03 + pal(hh * .4 + pos.z * .03) * ln * (.7 + uBeat * 1.5) * (.6 + hh);
    } else if (VARIANT == 3) {
      vec3 rr = reflect(rd, n);
      float fres = pow(1. - max(dot(-rd, n), 0.), 3.);
      body = pal(.58) * .05 + terSky(rr) * (.25 + .75 * fres) + pal(hh * .3 + .5) * ln * .12 * (.4 + uBeat);
    } else {
      float diff = max(dot(n, normalize(vec3(.3, .8, .5))), 0.);
      vec4 tx = img(pos.xz * .18 + .5);
      body = pal(hh * .35 + pos.z * .02 + v * .15) * (.15 + .75 * diff);
      body = mix(body, tx.rgb * (.3 + .8 * diff), tx.a * .5);
      body += pal(hh * .4 + .2) * ln * .6 * (.5 + uBeat * 1.2);
    }
    col = mix(body, terSky(rd) * .5, 1. - exp(-t * .03));
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

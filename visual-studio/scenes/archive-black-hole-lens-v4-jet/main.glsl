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


vec4 holeFX(vec2 p, vec2 c, float m) {
  vec2 d = p - c;
  float r = length(d);
  float rs = .1 + m * 2.;
  float mask = smoothstep(rs, rs * .88, r);
  vec2 e = d * vec2(1., 3.2);
  float er = length(e);
  float disc = smoothstep(.2, .3, er) * smoothstep(.7, .5, er);
  float dop = .5 + 1.1 * smoothstep(-.4, .4, d.x);
  vec3 dc = pal(.04 + er * .35 + uTime * .02) * disc * dop * (1.2 + uBeat * 1.2);
  float arc = exp(-pow((r - rs * 1.35) * 12., 2.)) * (.4 + .8 * smoothstep(-.3, .3, d.x));
  dc += pal(.08) * arc * 1.3;
  float vis = (d.y < 0.) ? 1. : 1. - mask;
  if (VARIANT == 3) {
    float jet = exp(-pow(d.x * (8. + abs(d.y) * 10.), 2.)) * smoothstep(rs, rs * 2., abs(d.y)) * exp(-abs(d.y) * 1.2);
    jet *= .6 + .4 * sin(abs(d.y) * 30. - uTime * 10.);
    dc += pal(.6 + uTime * .05) * jet * (1. + uBeat * 1.5 + uBass);
    vis = 1.;
  }
  return vec4(dc * vis, mask);
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec2 c1 = vec2(0.), c2 = vec2(0.);
  float m1 = .05 + uBass * .03, m2 = 0.;
  if (VARIANT == 1) c1 = .5 * vec2(cos(uTime * .5), sin(uTime * .7));
  if (VARIANT == 2) {
    float k = .5 + .5 * sin(uTime * .35);
    float sep = .75 * (1. - k) + .12;
    c1 = rot(uTime * .6) * vec2(-sep, 0.);
    c2 = -c1;
    m2 = m1;
  }
  vec2 d1 = p - c1;
  vec2 uvp = p - d1 * (m1 / (dot(d1, d1) + .003));
  if (VARIANT == 2) {
    vec2 d2 = p - c2;
    uvp -= d2 * (m2 / (dot(d2, d2) + .003));
  }
  vec3 col = imgOver(uvp * .5 + .5, pal(.6 + v * .1) * .05) * .8;
  float st = step(.985, hash21(floor(uvp * 60.))) * smoothstep(.3, 0., length(fract(uvp * 60.) - .5));
  col += st * (.6 + uHigh);
  vec4 h1 = holeFX(p, c1, m1);
  col = mix(col, vec3(0.), h1.a) + h1.rgb;
  if (VARIANT == 2) {
    vec4 h2 = holeFX(p, c2, m2);
    col = mix(col, vec3(0.), h2.a) + h2.rgb;
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

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


vec3 srcCRT(vec2 p) {
  vec3 bars = pal(floor((p.x + uTime * .1) * 8.) / 8. + float(VARIANT) * .15) * .35 * (.6 + .4 * sin(p.y * 3. + uTime));
  vec3 c = imgOver(p * .55 + .5, bars);
  c += vec3(.35) * uBeat;
  return c;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float tk = floor(uTime * 24.);
  vec2 q = p * (1. + .08 * dot(p, p));
  float tr = 0.;
  if (VARIANT == 1) {
    float row = floor(p.y * 140.);
    q.x += (hash21(vec2(row, tk)) - .5) * .012 * (1. + uHigh * 3.);
    tr = exp(-pow((fract(p.y * .2 - uTime * .15) - .5) * 14., 2.));
    q.x += tr * .05 * sin(p.y * 120. + tk);
  }
  if (VARIANT == 2) {
    float roll = (hash21(vec2(floor(uTime * 3.), 1.)) > .7) ? fract(uTime * 2.) : 0.;
    q.y = fract(q.y * .5 + .5 + roll) * 2. - 1.;
    q.x += uBeat * .1 * sin(q.y * 8. + uTime * 20.);
    q.x += step(.9, hash21(vec2(floor(p.y * 40.), tk))) * .08 * uBeat;
  }
  float ca = .003 + uHigh * .006 + ((VARIANT == 1) ? .004 : 0.);
  vec3 c = vec3(srcCRT(q + vec2(ca, 0.)).r, srcCRT(q).g, srcCRT(q - vec2(ca, 0.)).b);
  if (VARIANT == 3) {
    for (int i = 1; i < 7; i++) {
      vec3 s = srcCRT(q - vec2(.025 * float(i), 0.)) * pow(.78, float(i));
      c = max(c, s);
    }
  }
  vec3 bl = (srcCRT(q + vec2(.01, 0.)) + srcCRT(q - vec2(.01, 0.)) + srcCRT(q + vec2(0., .01)) + srcCRT(q - vec2(0., .01))) * .25;
  c = c * .8 + bl * .4 * (1. + uBass);
  c += tr * .25 * hash21(p * 300. + tk);
  if (VARIANT == 1) c += (hash21(gl_FragCoord.xy + tk) - .5) * .12;
  float sl = .72 + .28 * sin(gl_FragCoord.y * 3.14159);
  float m = mod(gl_FragCoord.x, 3.);
  vec3 mk = vec3((m < 1.) ? 1. : .55, (m >= 1. && m < 2.) ? 1. : .55, (m >= 2.) ? 1. : .55);
  c *= sl * mk * 1.25;
  c *= .95 + .05 * sin(uTime * 60.);
  vec2 a = abs(p) / vec2(RENDERSIZE.x / RENDERSIZE.y, 1.);
  c *= smoothstep(1., .94, max(a.x, a.y));
  return c;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

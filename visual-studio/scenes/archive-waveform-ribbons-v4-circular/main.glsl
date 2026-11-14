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


float wv(float x) {
  float t = uTime;
  return uBass * sin(x * 3. + t * 2.) * .5 + uMid * sin(x * 9. + t * 5.) * .22 + uHigh * sin(x * 24. + t * 11.) * .09 + .05 * sin(x * 2. + t);
}
// returns premultiplied colour in rgb, coverage in a
vec4 ribbon(vec2 p, float yoff, float ph, float thick, float hue) {
  float xx = p.x + ph;
  float w = wv(xx) + yoff;
  float e = .01;
  float sl = (wv(xx + e) - wv(xx - e)) / (2. * e);
  float d = (p.y - w) / sqrt(1. + sl * sl);
  float c = d / thick;
  float a = smoothstep(1., .85, abs(c));
  float sh = sqrt(max(1. - c * c, 0.));
  vec4 t = img(vec2(p.x * .8 + uTime * .1 + ph, c * .5 + .5));
  vec3 col = pal(hue + p.x * .15 + uTime * .1) * (.5 + .5 * sh);
  col = mix(col, t.rgb * (.6 + .6 * sh), t.a * .9);
  vec3 glow = pal(hue) * exp(-abs(d) * 14.) * .35 * (.5 + uBeat);
  return vec4(col * a + glow, a);
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec3 col = imgOver(p * .4 + .5, pal(.6 + v * .1) * .04) * .25;
  col += pal(.6) * .02 * smoothstep(.02, 0., abs(fract(p.y * 5.) - .5) - .47);
  if (VARIANT == 0) {
    vec4 rc = ribbon(p, 0., 0., .1 + uBass * .12, 0.);
    col = col * (1. - rc.a) + rc.rgb;
  } else if (VARIANT == 1) {
    for (int k = 0; k < 3; k++) {
      float fk = float(k);
      vec4 rc = ribbon(p, sin(p.x * 2. + uTime + fk * 2.09) * .25, fk * 2.09, .08 + uBass * .05, fk * .33);
      col = col * (1. - rc.a) + rc.rgb;
    }
  } else if (VARIANT == 2) {
    float best = 1e9, bs = 0.;
    vec2 bp = vec2(0.);
    vec2 amp = vec2(1.1, .7) * (1. + uBass * .2);
    vec2 pa = vec2(sin(uTime * .5), sin(uTime * .3)) * amp;
    for (int i = 1; i <= 96; i++) {
      float s = float(i) / 96. * TAU;
      vec2 pb = vec2(sin(3. * s + uTime * .5), sin(2. * s + uTime * .3)) * amp;
      float d = sdSeg(p, pa, pb);
      if (d < best) { best = d; bs = s; bp = pb; }
      pa = pb;
    }
    vec3 tube = pal(bs / TAU + uTime * .1) * (exp(-best * 40.) * 1.4 + exp(-best * 10.) * .25) * (.6 + uBeat * 1.2);
    col += tube;
    vec2 c = (p - bp) / .2;
    vec4 t = img(c + .5);
    float inside = step(max(abs(c.x), abs(c.y)), .5) * step(best, .12);
    col = mix(col, t.rgb, t.a * inside);
  } else {
    float a = atan(p.y, p.x);
    float r = length(p);
    float R = .55 + uBeat * .03;
    float w = wv(a) * .7;
    float thick = .09 + uBass * .05;
    float d = r - (R + w);
    float c = d / thick;
    float cov = smoothstep(1., .85, abs(c));
    float sh = sqrt(max(1. - c * c, 0.));
    vec4 t = img(vec2(a / TAU * 5. - uTime * .1, c * .5 + .5));
    vec3 rc = pal(a / TAU + uTime * .1) * (.5 + .5 * sh);
    rc = mix(rc, t.rgb * (.6 + .6 * sh), t.a * .9);
    vec3 glow = pal(a / TAU) * exp(-abs(d) * 14.) * .35 * (.5 + uBeat);
    col = col * (1. - cov) + rc * cov + glow;
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

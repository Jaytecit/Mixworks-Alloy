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


vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float f = 0.;
  vec2 g = vec2(0.);
  for (int i = 0; i < 7; i++) {
    float fi = float(i);
    float a = uTime * (.35 + .11 * fi) + fi * 2.4;
    float orb = (VARIANT == 3) ? .25 + .75 * uBeat + .2 * sin(uTime + fi)
                               : .42 * (.6 + .5 * sin(fi * 1.7 + uTime * .3)) + uBass * .18;
    float R = (VARIANT == 3) ? .13 + .02 * fi + uBeat * .1 : .17 + .02 * sin(fi * 2.1) + uBass * .05;
    vec2 c = vec2(cos(a), sin(a * 1.3 + fi)) * orb;
    vec2 d = p - c;
    float d2 = dot(d, d) + 1e-3;
    f += R * R / d2;
    g += -2. * R * R * d / (d2 * d2);
  }
  vec3 bg = imgOver(p * .45 + .5, pal(.55 + v * .1) * .07) * .45;
  float m = smoothstep(.97, 1.03, f);
  vec3 n = normalize(vec3(-g * .09, 1.));
  vec3 inner;
  if (VARIANT == 0) {
    vec3 rr = reflect(vec3(0., 0., -1.), n);
    vec3 env = pal(rr.x * .7 + rr.y * .5 + uTime * .1) * (.25 + .75 * pow(max(sin(rr.y * 7. + uTime * .8), 0.), 4.));
    vec4 t = img(rr.xy * .6 + .5);
    env = mix(env, t.rgb * 1.2, t.a);
    inner = env + pow(max(dot(n, normalize(vec3(.4, .6, .7))), 0.), 40.);
  } else if (VARIANT == 1) {
    float ca = .02 + uHigh * .05;
    vec2 uv = p * .45 + .5 - n.xy * (.25 + uBass * .2);
    vec3 gb = pal(f * .15 + v * .1) * .3;
    inner = vec3(imgOver(uv + n.xy * ca, gb).r, imgOver(uv, gb).g, imgOver(uv - n.xy * ca, gb).b) * 1.2;
    inner += pow(1. - n.z, 3.) * pal(uTime * .2);
  } else if (VARIANT == 2) {
    vec4 t = img(p * (.55 - .1 * min(f, 3.)) + .5);
    inner = pal(f * .25 + uTime * .1 + v * .1) * (.3 + .5 * clamp(f - 1., 0., 2.));
    inner = mix(inner, t.rgb, t.a * .85) + pow(max(n.z, 0.), 3.) * .15;
  } else {
    vec3 b = pal(f * .4 - uTime * .2) * (.4 + .6 * smoothstep(.2, 1., sin(f * 12. - uTime * 5.)));
    vec4 t = img(p * .5 + .5 + n.xy * .1);
    inner = mix(b, t.rgb, t.a * .7);
  }
  float edge = exp(-abs(f - 1.) * 10.);
  vec3 col = mix(bg, inner, m);
  col += pal(uTime * .1 + v * .2) * edge * (.4 + uBeat * 1.5);
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

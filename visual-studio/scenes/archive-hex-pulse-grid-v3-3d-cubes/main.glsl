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


vec4 hexCoords(vec2 p) {
  const vec2 s = vec2(1., 1.7320508);
  vec4 hC = floor(vec4(p, p - vec2(.5, 1.)) / s.xyxy) + .5;
  vec4 h = vec4(p - hC.xy * s, p - (hC.zw + .5) * s);
  return dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hC.xy) : vec4(h.zw, hC.zw + .5);
}
float hexDist(vec2 p) { p = abs(p); return max(dot(p, vec2(.5, .8660254)), p.x); }

vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec2 q = p * 3.2;
  if (VARIANT == 3) q = rot(length(p) * 1.5 * sin(uTime * .3) + uBass) * q;
  vec4 hc = hexCoords(q);
  vec2 l = hc.xy, id = hc.zw;
  float hd = hexDist(l);
  float d0 = length(id * vec2(1., 1.732));
  float pulse = pow(.5 + .5 * sin(d0 * .9 - uTime * 3.), 4.) * (.3 + uBass);
  float ring = exp(-abs(d0 * .3 - (1. - uBeat) * 5.) * 3.) * uBeat;
  float h = hash21(id);
  float edge = smoothstep(.40, .47, hd);
  vec3 col;
  if (VARIANT == 1) {
    vec3 bg = imgOver(p * .5 + .5, pal(.6 + v * .1) * .05) * .35;
    col = bg + pal(h + uTime * .1) * edge * (.3 + pulse * 1.5 + ring * 2.);
  } else if (VARIANT == 2) {
    float s = 1. + pulse * .25;
    vec2 ll = l / s;
    float ang = atan(ll.y, ll.x);
    float face = floor((ang + PI) / (TAU / 3.));
    float shade = (face < .5) ? 1. : (face < 1.5) ? .55 : .8;
    vec4 t = img(rot(face * 2.094) * ll * 1.8 + .5);
    vec3 side = pal(h + v * .2 + face * .1) * shade * .6;
    col = mix(side, t.rgb * shade * 1.2, t.a * .85);
    col *= step(hexDist(ll), .47);
    col += pal(h + uTime * .1) * smoothstep(.4, .47, hexDist(ll)) * .8 * (.4 + ring * 2.);
  } else {
    vec4 t = img(l * 1.7 * (1. + pulse * .3) + .5);
    vec3 b = pal(h + uTime * .1 + v * .2) * (.1 + .4 * pulse);
    col = mix(b, t.rgb, t.a);
    col *= 1. - edge * .8;
    col += pal(h + .4) * edge * (.4 + pulse + ring * 2.);
  }
  col += pal(uTime * .15) * ring * .35;
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

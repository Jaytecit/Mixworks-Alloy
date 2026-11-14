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


vec3 scene(vec2 p) {
  float v = float(VARIANT);
  float n = (VARIANT == 0) ? 4. : (VARIANT == 1) ? 6. : (VARIANT == 2) ? 8. : 5.;
  vec2 q = p * n * (1. - uBeat * .05);

  if (VARIANT == 1) q.x += floor(q.y) * .5;
  if (VARIANT == 2) q = rot(PI * .25) * q;
  if (VARIANT == 3) {
    float r = length(p) * n;
    float a = atan(p.y, p.x) / TAU;
    float cells = floor(r) * 3. + 3.;
    q = vec2(a * cells, r);
  }

  vec2 id = floor(q), f = fract(q);
  float h = hash21(id);
  float fire = step(h, uBeat * .9 + uBass * .3);

  vec2 c = f - .5;
  c = rot(fire * (h - .5) * PI) * c;
  c *= 1. - fire * .25;
  float inside = step(max(abs(c.x), abs(c.y)), .5);

  vec4 t = img(c + .5);
  t.a *= inside;

  vec3 bg = pal(h + uTime * .1 + v * .2) * (.15 + .5 * fire);
  vec3 col = mix(bg, t.rgb, t.a);
  col += pal(h * 2. + uTime * .3) * fire * .5;

  vec2 e = min(f, 1. - f);
  float line = smoothstep(.04, 0., min(e.x, e.y));
  col += line * pal(h) * (.3 + uHigh);
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

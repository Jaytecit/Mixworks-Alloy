#version 330 core
#define VARIANT 3
#define KEEP_ASPECT 1   // 1 = sprite keeps its own aspect ratio, 0 = stretch to square

uniform vec2  RENDERSIZE;
uniform float motion;
uniform float response;
uniform float artworkImprint;
uniform float refractionDepth;
uniform float spiralTwist;
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
  vec4 artwork = textureLod(syn_Media, f, 0.0);
  artwork.a *= artworkImprint;
  return artwork;
}

vec3 imgOver(vec2 uv, vec3 bg) {
  vec4 t = img(uv);
  return mix(bg, t.rgb, t.a);
}


vec3 scene(vec2 p) {
  float v = float(VARIANT);
  int cnt = (VARIANT == 1) ? 2 : 1;
  float w = 0.;
  vec2 dir = vec2(0.);

  for (int i = 0; i < 2; i++) {
    if (i >= cnt) break;
    float fi = float(i);
    vec2 o = (VARIANT == 1)
      ? .55 * vec2(cos(uTime * .6 + fi * PI), sin(uTime * .6 + fi * PI))
      : vec2(0.);
    vec2 d0 = p - o;
    float r = (VARIANT == 2) ? max(abs(d0.x), abs(d0.y)) : length(d0);
    if (VARIANT == 3) r += atan(d0.y, d0.x) / TAU * (.4 * spiralTwist);
    float ph = r * 14. - uTime * 5.;
    float env = exp(-r * 2.2);
    float s = sin(ph) * env;
    w += s;
    dir += normalize(d0 + 1e-4) * s;
  }

  float A = (.03 + uBass * .08 + uBeat * .07) * refractionDepth;
  vec2 uv = p * .7 + .5 + dir * A * 1.5;
  float ca = (.01 + uHigh * .05) * refractionDepth;

  vec3 bg = pal(w * .3 + uTime * .1 + v * .2) * .3;
  vec3 col = vec3(imgOver(uv + dir * ca, bg).r,
                  imgOver(uv, bg).g,
                  imgOver(uv - dir * ca, bg).b);

  col += pal(w * .5 + uTime * .2) * abs(w) * (.1 + uMid * .3);

  float rr = length(p);
  float ring = exp(-abs(rr - (1. - uBeat) * 1.8) * 14.) * uBeat;
  col += pal(uTime * .3 + v * .25) * ring * 1.5;
  return col;
}

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y * 2.;
  vec3 col = scene(p);
  col *= 1. + uBeat * .5;
  col *= 1. - .35 * dot(p * .5, p * .5);
  fragColor = vec4(clamp(col, 0., 1.), 1.);
}

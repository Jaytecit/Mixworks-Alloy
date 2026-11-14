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


float oceanH(vec2 xz) {
  float t = uTime;
  float amp = (VARIANT == 0) ? .12 : (VARIANT == 1) ? .45 : (VARIANT == 2) ? .22 : .3;
  amp *= .6 + uBass * 1.6;
  float h = sin(xz.x * .6 + t * .9) * cos(xz.y * .4 + t * .6) * amp;
  h += sin(dot(xz, vec2(1.1, .8)) + t * 1.4) * amp * .5;
  h += sin(dot(xz, vec2(-2.3, 1.9)) + t * 2.1) * amp * .25 * (1. + uHigh * 2.);
  h += sin(dot(xz, vec2(4.7, 3.1)) - t * 3.3) * amp * .1 * (1. + uHigh * 3.);
  if (VARIANT == 3) {
    float zc = 8. + 3. * sin(uTime * .5);
    h += 1.8 * (1. + uBass) * exp(-pow((xz.y - zc) * .45, 2.));
  }
  return h;
}
vec3 oSky(vec3 rd) {
  float v = float(VARIANT);
  vec2 s = rd.xy / max(rd.z, .1) * 1.7;
  vec3 c;
  if (VARIANT == 2) c = mix(pal(.8) * .08, vec3(0.), clamp(rd.y * 2.5, 0., 1.)) + pal(.85) * exp(-abs(rd.y) * 14.) * .35;
  else c = mix(pal(.95) * .55, pal(.62) * .08, clamp(rd.y * 2., 0., 1.));
  vec2 sp = s - vec2(0., .5);
  float up = step(0., rd.z) * step(-.02, rd.y);
  float disc = smoothstep(.34, .32, length(sp)) * up;
  c = mix(c, pal(.08) * 1.4, disc * ((VARIANT == 2) ? 0. : .5));
  vec4 t = img(sp / .8 + .5);
  c = mix(c, t.rgb * 1.1, t.a * step(max(abs(sp.x), abs(sp.y)), .4) * up);
  return c;
}
vec3 scene(vec2 p) {
  float v = float(VARIANT);
  vec3 ro = vec3(0., 1.7 + ((VARIANT == 3) ? .6 : 0.), 0.);
  vec3 rd = normalize(vec3(p.x, p.y - .15, 1.7));
  vec3 col = oSky(rd);
  if (rd.y < .02) {
    float t = 0.;
    bool hit = false;
    for (int i = 0; i < 80; i++) {
      vec3 pos = ro + rd * t;
      float d = pos.y - oceanH(pos.xz);
      if (d < .002 * t) { hit = true; break; }
      t += max(d * .6, .004 * t);
      if (t > 80.) break;
    }
    if (hit) {
      vec3 pos = ro + rd * t;
      vec2 e = vec2(.03, 0.);
      vec3 n = normalize(vec3(oceanH(pos.xz - e.xy) - oceanH(pos.xz + e.xy), .06, oceanH(pos.xz - e.yx) - oceanH(pos.xz + e.yx)));
      float fres = pow(1. - max(dot(-rd, n), 0.), 4.) * .9 + .05;
      vec3 refl = oSky(reflect(rd, n));
      vec3 body = pal(.55 + v * .02) * .12 + pal(.5) * pow(max(pos.y + .3, 0.), 2.) * .15;
      vec3 c2 = body * (1. - fres) + refl * fres;
      float foam = smoothstep(.55, .9, 1. - n.y) * ((VARIANT == 1 || VARIANT == 3) ? 1. : .0);
      c2 = mix(c2, vec3(.9), foam * .6);
      if (VARIANT == 2) {
        vec2 gd = .5 - abs(fract(pos.xz * .8) - .5);
        float ln = 1. - smoothstep(0., .03 + t * .004, min(gd.x, gd.y));
        c2 += pal(pos.z * .05 + uTime * .1) * ln * .5 * (.4 + uBeat * 1.5);
      }
      col = mix(c2, oSky(vec3(rd.x, 0., rd.z)) * .6, 1. - exp(-t * .02));
    }
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

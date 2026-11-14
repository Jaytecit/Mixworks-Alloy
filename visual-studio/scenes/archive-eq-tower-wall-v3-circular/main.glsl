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


vec3 gHit;

float eqH(float xi) { return .12 + 2.4 * spec(clamp(abs(xi) / 12., 0., 1.)); }

float barCart(vec3 p, vec2 id) {
  vec2 l = p.xz - (id + .5) * .5;
  float h = eqH(id.x) * (.55 + .45 * hash21(vec2(id.x, id.y + floor(uTime * 3.))));
  return sdBox(vec3(l.x, p.y - h, l.y), vec3(.17, h, .17));
}
float barPolar(vec3 p, float sa, float ri, float r) {
  float sec = TAU / 56.;
  float ang = atan(p.z, p.x);
  float la = ang - (sa + .5) * sec;
  vec2 lp = vec2(r * cos(la) - (1.2 + (ri + .5) * .9), r * sin(la));
  float hgt = .1 + 2.2 * spec(fract((sa + 28.) / 56.)) * (1. - ri * .2);
  return sdBox(vec3(lp.x, p.y - hgt, lp.y), vec3(.28, hgt, .06));
}
float eqMap(vec3 p) {
  float d = p.y;
  if (VARIANT == 2) {
    float r = length(p.xz);
    float rr = r - 1.2;
    float ri = floor(rr / .9);
    if (rr > 0. && ri < 4.) {
      float sec = TAU / 56.;
      float sa = floor(atan(p.z, p.x) / sec);
      for (int i = -1; i <= 1; i++) d = min(d, barPolar(p, sa + float(i), ri, r));
    }
    return min(d, .3);
  }
  vec2 id = floor(p.xz / .5);
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      d = min(d, barCart(p, id + vec2(float(i), float(j))));
    }
  }
  return min(d, .12);
}
vec4 eqRay(vec3 ro, vec3 rd) {
  float t = .1;
  bool hit = false;
  for (int i = 0; i < 90; i++) {
    float d = eqMap(ro + rd * t);
    if (d < .003) { hit = true; break; }
    t += d * .8;
    if (t > 40.) break;
  }
  vec3 col = mix(pal(.75) * .14, vec3(0.), clamp(rd.y * 2., 0., 1.));
  float isFloor = 0.;
  if (hit) {
    vec3 pos = ro + rd * t;
    gHit = pos;
    vec2 e = vec2(.004, 0.);
    vec3 n = normalize(vec3(eqMap(pos + e.xyy) - eqMap(pos - e.xyy),
                            eqMap(pos + e.yxy) - eqMap(pos - e.yxy),
                            eqMap(pos + e.yyx) - eqMap(pos - e.yyx)));
    if (pos.y < .01) {
      vec2 gd = .5 - abs(fract(pos.xz * 2.) - .5);
      float ln = 1. - smoothstep(0., .02 + t * .003, min(gd.x, gd.y));
      col = pal(.62) * .03 + pal(pos.z * .05 + uTime * .1) * ln * .25 * (.4 + uBeat);
      isFloor = 1.;
    } else {
      float hx = (VARIANT == 2) ? atan(pos.z, pos.x) * 8. : pos.x * 2.;
      vec3 side = pal(pos.y * .35 + hx * .04 + uTime * .05) * (.35 + .9 * clamp(pos.y / 2.2, 0., 1.));
      float top = step(.9, n.y);
      vec4 tx = img(pos.xz * .22 + .5);
      vec3 base = mix(side, tx.rgb * 1.2, tx.a * top);
      base *= .45 + .55 * max(dot(n, normalize(vec3(.3, .8, -.4))), 0.);
      base += pal(pos.y * .3 + uTime * .1) * top * .25 * (uBass + .2);
      col = base;
    }
    col = mix(col, pal(.75) * .1, 1. - exp(-t * .04));
  }
  return vec4(col, isFloor);
}
vec3 scene(vec2 p) {
  vec3 ro, rd;
  if (VARIANT == 0) {
    vec3 f = normalize(vec3(0., -.65, .76));
    vec3 up = normalize(cross(f, vec3(1., 0., 0.)));
    ro = vec3(0., 6., -4. + uTime * 1.2) + vec3(1., 0., 0.) * p.x * 3.2 + up * p.y * 3.2;
    rd = f;
  } else if (VARIANT == 2) {
    ro = vec3(0., 5.5, -4.5);
    ro.xz = rot(uTime * .15) * ro.xz;
    vec3 f = normalize(-ro);
    vec3 rt = normalize(cross(vec3(0., 1., 0.), f));
    vec3 up = cross(f, rt);
    rd = normalize(f * 1.6 + rt * p.x + up * p.y);
  } else if (VARIANT == 1) {
    ro = vec3(0., 2.2, uTime * 1.8);
    rd = normalize(vec3(p.x, p.y - .25, 1.6));
  } else {
    ro = vec3(0., 1.1, uTime * 1.5);
    rd = normalize(vec3(p.x, p.y - .12, 1.6));
  }
  vec4 r = eqRay(ro, rd);
  vec3 col = r.rgb;
  if (VARIANT == 3 && r.a > .5) {
    vec3 ro2 = gHit + vec3(0., .01, 0.);
    vec3 rd2 = vec3(rd.x, -rd.y, rd.z);
    vec4 r2 = eqRay(ro2, rd2);
    col = mix(r.rgb, r2.rgb, .55);
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

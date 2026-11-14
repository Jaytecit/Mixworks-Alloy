#version 330 core
// Soap Film Frame / Twisted Frame
#define VARIANT 2

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_Level;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float frameTwist;
uniform float iridescence;

#define PI 3.14159265359
#define TAU 6.28318530718

mat2 rot2(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm2(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    mat2 m = mat2(0.80, -0.60, 0.60, 0.80);
    for (int i = 0; i < 4; i++) {
        v += a * noise2(p);
        p = m * p * 2.03 + vec2(13.1, 7.7);
        a *= 0.5;
    }
    return v;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + vec3(r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdCappedCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float denom = max(dot(ba, ba), 1e-5);
    float h = clamp(dot(pa, ba) / denom, 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1e-4), 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

vec2 minMat(vec2 a, vec2 b) {
    return (a.x < b.x) ? a : b;
}

float mediaLuma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

vec3 acesTone(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float samplePinMask(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return (tex.a < 0.96) ? tex.a : mediaLuma(tex.rgb);
    }
    // Four symmetrical pin nodes fallback
    float d1 = 1.0 - smoothstep(0.06, 0.14, length(cuv - vec2(0.32, 0.38)));
    float d2 = 1.0 - smoothstep(0.06, 0.14, length(cuv - vec2(0.68, 0.62)));
    return clamp(max(d1, d2), 0.0, 1.0);
}

// Evaluates bent wire loop SDF and translucent soap film membrane surface distance
vec2 mapScene(vec3 p, out float filmDist, out vec2 filmUV, out float filmThick) {
    filmDist = 99.0;
    filmUV = vec2(0.5);
    filmThick = 0.0;

    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float mids = (syn_MidLevel * response);
    float clk = motion;
    vec2 res = vec2(p.y + 2.4, 0.0);

    vec3 q = p - vec3(0.0, 0.2, 0.0);
    q.xz *= rot2(sin(clk * 0.45) * 0.35);

    filmUV = clamp(q.xy / 3.4 + 0.5, 0.0, 1.0);
    float pin = samplePinMask(filmUV) * imprint;
    float vib = (1.0 - pin * 0.92) * (0.22 + 0.38 * bass) * sin(clk * 2.6 + length(q.xy) * 2.2);

#if VARIANT == 0
    // 01: Single Film — circular bent wire wand spanning billowing iridescent membrane
    float loopR = 1.65;
    float dWire = sdTorus(q.xzy, vec2(loopR, 0.045));
    float dStem = sdCapsule(q, vec3(0.0, -1.65, 0.0), vec3(0.0, -2.6, 0.0), 0.05);
    res = minMat(res, vec2(min(dWire, dStem), 1.0));
    if (length(q.xy) < loopR) {
        filmDist = abs(q.z - vib * cos(length(q.xy) / loopR * PI * 0.5));
    }

#elif VARIANT == 1
    // 02: Joined Bubbles — two overlapping wire rings sharing a central Plateau strut
    float dW1 = sdTorus((q - vec3(-0.78, 0.0, 0.0)).xzy, vec2(1.18, 0.042));
    float dW2 = sdTorus((q - vec3(0.78, 0.0, 0.0)).xzy, vec2(1.18, 0.042));
    float dStrut = sdCapsule(q, vec3(0.0, -0.9, 0.0), vec3(0.0, 0.9, 0.0), 0.04);
    res = minMat(res, vec2(min(min(dW1, dW2), dStrut), 1.0));
    if (min(length(q.xy - vec2(-0.78, 0.0)), length(q.xy - vec2(0.78, 0.0))) < 1.18) {
        float dome = 0.45 * (1.0 - pin * 0.9) * (1.0 + 0.4 * bass);
        filmDist = abs(abs(q.z) - dome * 0.65);
    }

#elif VARIANT == 2
    // 03: Twisted Frame — hyperbolic saddle wire perimeter spanning minimal surface
    float ang = atan(q.y, q.x);
    float r = length(q.xy);
    float saddleZ = (0.48 * frameTwist) * cos(ang * 2.0) * (r / 1.65);
    float dWire = length(vec2(r - 1.65, q.z - saddleZ)) - 0.045;
    res = minMat(res, vec2(dWire, 1.0));
    if (r < 1.65) {
        filmDist = abs(q.z - (saddleZ * (r / 1.65) + vib * 0.7));
    }

#else
    // 04: Rupturing Chambers — 2x2 wire frame grid where cells rupture and reform on hits
    float dOuter = sdBox(q, vec3(1.75, 1.45, 0.04));
    float dCut = sdBox(q, vec3(1.65, 1.35, 0.1));
    float dFrame = max(dOuter, -dCut);
    float dCross = min(sdBox(q, vec3(0.035, 1.4, 0.04)), sdBox(q, vec3(1.7, 0.035, 0.04)));
    res = minMat(res, vec2(min(dFrame, dCross), 1.0));
    float cellBurst = step(0.32, fract(hash21(floor(q.xy * 0.7 + 1.0)) + (syn_Hits * response) * 0.35 + clk * 0.12));
    if (abs(q.x) < 1.65 && abs(q.y) < 1.35 && (cellBurst > 0.5 || pin > 0.35)) {
        filmDist = abs(q.z - vib);
    }
#endif

    // Thin-film thickness varies with fluid convection + steep gradients around pinned sprite nodes
    filmThick = 2.5 + 3.8 * fbm2(filmUV * 3.5 + vec2(clk * 0.35, -clk * 0.22 + TIME * 0.05))
              + 4.5 * mids + 5.2 * pin;
    return res;
}

vec3 calcNormal(vec3 p) {
    float fd, ft;
    vec2 fuv;
    vec2 e = vec2(1.0, -1.0) * 0.0035;
    return normalize(
        e.xyy * mapScene(p + e.xyy, fd, fuv, ft).x +
        e.yyx * mapScene(p + e.yyx, fd, fuv, ft).x +
        e.yxy * mapScene(p + e.yxy, fd, fuv, ft).x +
        e.xxx * mapScene(p + e.xxx, fd, fuv, ft).x
    );
}

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;

    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_Hits * response), 0.0, 1.5);

    vec3 ro = vec3(0.35, 0.55, -4.6);
    vec3 ta = vec3(0.0, 0.15, 0.0);
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 bgCol = mix(vec3(0.03, 0.04, 0.06), vec3(0.10, 0.12, 0.16), clamp(rd.y + 0.4, 0.0, 1.0));

    float t = 0.0;
    float maxT = 20.0;
    vec2 hit = vec2(maxT, -1.0);
    float minFilm = 99.0;
    vec2 hitFilmUV = vec2(0.5);
    float hitThick = 0.0;

    for (int i = 0; i < 68; i++) {
        vec3 pos = ro + rd * t;
        float fDist, fThick;
        vec2 fUV;
        vec2 res = mapScene(pos, fDist, fUV, fThick);
        if (fDist < minFilm) {
            minFilm = fDist;
            hitFilmUV = fUV;
            hitThick = fThick;
        }
        if (res.x < 0.002 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            break;
        }
        t += min(res.x, max(fDist * 0.5 + 0.02, 0.04)) * 0.72;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(0.45, 0.82, -0.4));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 64.0);
        vec3 baseCol = (hit.y < 0.5) ? vec3(0.06, 0.07, 0.09) : vec3(0.75, 0.77, 0.82);
        col = baseCol * (vec3(1.05, 1.02, 1.0) * diff + vec3(0.16, 0.18, 0.22))
            + vec3(1.0) * spec * (0.45 + 0.8 * highs);
    }

    // Translucent thin-film optical interference radiance over background & wire frame
    float onFilm = exp(-max(minFilm, 0.0) * 32.0);
    if (onFilm > 0.01) {
        vec3 irid = 0.5 + 0.5 * cos(hitThick * vec3(1.0, 1.28, 1.62) + vec3(0.0, 1.8, 3.6));
        float rim = pow(clamp(1.0 - abs(dot(rd, vec3(0.0, 0.0, 1.0))), 0.0, 1.0), 1.8);
        float pinRing = samplePinMask(hitFilmUV) * imprint;
        irid = clamp(mix(vec3(dot(irid, vec3(0.2126, 0.7152, 0.0722))), irid, iridescence), 0.0, 1.0);
        vec3 filmCol = mix(irid, vec3(0.95, 0.98, 1.0), rim * 0.45 + pinRing * 0.35);
        col = mix(col, col * 0.65 + filmCol * (0.55 + 0.45 * (syn_Level * response) + 0.35 * highs), clamp(onFilm * 0.78, 0.0, 0.92));
    }

    col = acesTone(col * 1.15);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

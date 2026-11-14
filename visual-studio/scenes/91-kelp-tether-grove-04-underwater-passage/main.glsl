#version 330 core
// Kelp Tether Grove / Underwater Passage
#define VARIANT 3

out vec4 fragColor;

uniform vec2 RENDERSIZE;
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
uniform float kelpSway;
uniform float causticStrength;

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


float sampleResistMask(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return (tex.a < 0.96) ? tex.a : mediaLuma(tex.rgb);
    }
    return 1.0 - smoothstep(0.14, 0.34, length(cuv - 0.5));
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float dBed = p.y + 1.85;
    vec2 res = vec2(dBed, 0.0);

    float gx = floor(p.x / 1.35 + 0.5);
    float gz = floor(p.z / 1.35 + 0.5);
    vec2 guv = clamp(vec2(gx / 6.0 + 0.5, gz / 6.0 + 0.25), 0.0, 1.0);
    float resist = sampleResistMask(guv) * imprint;

    float hNorm = clamp((p.y + 1.85) / 4.2, 0.0, 1.0);
    float sway = (1.0 - resist * 0.85) * (0.32 + 0.65 * bass) * sin(clk * 1.4 + p.y * 0.9 + gx * 1.3) * hNorm * kelpSway;
    vec3 q = vec3(p.x - gx * 1.35 - sway, p.y, p.z - gz * 1.35);

#if VARIANT == 3
    if (abs(gx) < 0.6) q.x += 99.0; // central underwater kelp passage
#endif
    float blade = (0.08 * kelpSway) * sin(p.y * 5.0 + clk * 2.0 + (syn_MidLevel * response) * 3.0);
    float dStipe = max(length(q.xz - vec2(blade, 0.0)) - (0.09 + 0.06 * sin(p.y * 7.0)), p.y - 2.4);
    if (dStipe < res.x) { res = vec2(dStipe * 0.72, 1.0); info = vec4(guv, resist, hNorm); }
    return res;
}

void getCamera(out vec3 ro, out vec3 ta) {
#if VARIANT == 0
    ro = vec3(0.0, -0.85, -4.4);
    ta = vec3(0.0, 1.25, 1.8);
#else
    ro = vec3(0.2, 0.15, -4.8);
    ta = vec3(0.0, 0.35, 1.8);
#endif
}

vec3 getSky(vec3 rd, out vec3 sunDir) {
    sunDir = normalize(vec3(0.2, 0.95, 0.2));
    return mix(vec3(0.02, 0.14, 0.20), vec3(0.08, 0.42, 0.48), clamp(rd.y * 1.4 + 0.35, 0.0, 1.0));
}

vec3 shadeSurface(vec3 pos, vec3 nor, vec3 rd, float dist, float matId, vec4 info, vec3 sunDir, vec3 skyCol, vec2 p) {
    float diff = max(dot(nor, sunDir), 0.0);
    float caust = 0.5 + 0.5 * sin(pos.x * 4.0 + motion * 2.0) * cos(pos.z * 4.0 - motion * 1.5);
    vec3 baseCol = (matId < 0.5) ? vec3(0.08, 0.14, 0.14)
                                 : mix(vec3(0.14, 0.38, 0.18), vec3(0.42, 0.68, 0.22), info.w);
    vec3 lit = baseCol * (diff + 0.28) + vec3(0.20, 0.68, 0.62) * caust * (0.2 + 0.5 * (syn_HighLevel * response) + 0.2 * (syn_Hits * response)) * causticStrength;
    return mix(lit, skyCol, clamp(1.0 - exp(-dist * 0.075), 0.0, 1.0));
}

vec3 postMissGlow(vec3 rd, vec2 p) {
    return vec3(0.18, 0.65, 0.72) * pow(max(rd.y, 0.0), 3.0) * (0.35 + 0.3 * (syn_Level * response));
}


vec3 calcNormal(vec3 p) {
    vec4 dummy;
    vec2 e = vec2(1.0, -1.0) * 0.0035;
    return normalize(
        e.xyy * mapScene(p + e.xyy, dummy).x +
        e.yyx * mapScene(p + e.yyx, dummy).x +
        e.yxy * mapScene(p + e.yxy, dummy).x +
        e.xxx * mapScene(p + e.xxx, dummy).x
    );
}

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;

    vec3 ro, ta;
    getCamera(ro, ta);
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.52 * cw);

    vec3 sunDir;
    vec3 skyCol = getSky(rd, sunDir);

    float t = 0.0;
    float maxT = 30.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);

    for (int i = 0; i < 68; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        vec2 res = mapScene(pos, info);
        if (res.x < 0.0022 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            hitInfo = info;
            break;
        }
        t += res.x * 0.66;
        if (t > maxT) break;
    }

    vec3 col = skyCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        col = shadeSurface(pos, nor, rd, hit.x, hit.y, hitInfo, sunDir, skyCol, p);
    } else {
        col += postMissGlow(rd, p);
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.21 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

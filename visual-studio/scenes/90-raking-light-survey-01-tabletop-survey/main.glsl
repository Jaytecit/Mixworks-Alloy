#version 330 core
// Raking Light Survey / Tabletop Survey
#define VARIANT 0

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_OnBeat;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float lightElevation;
uniform float sweepStrength;

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


float sampleSurveyRelief(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return tex.a * mediaLuma(tex.rgb);
    }
    float r = length(cuv - 0.5);
    return clamp(0.6 * (1.0 - smoothstep(0.15, 0.44, r)) + 0.4 * sin(cuv.x * 24.0) * cos(cuv.y * 24.0), 0.0, 1.0);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    vec2 res = vec2(p.y + 1.45, 0.0);

#if VARIANT == 1
    vec2 suv = clamp(vec2(p.x / 4.4 + 0.5, p.y / 3.2 + 0.45), 0.0, 1.0);
    float rel = sampleSurveyRelief(suv) * imprint * 0.36;
    float dSlab = sdBox(p - vec3(0.0, 0.25, 0.8 - rel * 0.5), vec3(2.2, 1.6, 0.25 + rel * 0.5));
#elif VARIANT == 2
    vec3 q = p - vec3(0.0, 0.2, 0.6);
    float ang = atan(q.z, q.x) / TAU + 0.5;
    vec2 suv = clamp(vec2(ang, q.y / 2.8 + 0.5), 0.0, 1.0);
    float rel = sampleSurveyRelief(suv) * imprint * 0.32;
    float dSlab = max(length(q.xz) - (1.25 + rel), abs(q.y) - 1.35);
#elif VARIANT == 3
    vec2 suv = clamp(vec2(p.x / 4.4 + 0.5, p.z / 3.6 + 0.5), 0.0, 1.0);
    float tile = 0.08 * sin(floor(p.x * 1.4) + floor(p.z * 1.4));
    float rel = sampleSurveyRelief(suv) * imprint * 0.34;
    float dSlab = sdBox(p - vec3(0.0, -0.35 + tile + rel * 0.5, 0.2), vec3(2.2, 0.2 + rel * 0.5, 1.8));
#else
    vec2 suv = clamp(vec2(p.x / 4.4 + 0.5, p.z / 3.6 + 0.5), 0.0, 1.0);
    float rel = sampleSurveyRelief(suv) * imprint * 0.38;
    float dSlab = sdBox(p - vec3(0.0, -0.35 + rel * 0.5, 0.2), vec3(2.2, 0.2 + rel * 0.5, 1.8));
#endif
    if (dSlab < res.x) { res = vec2(dSlab * 0.68, 1.0); info = vec4(suv, rel, 0.0); }
    return res;
}

void getCamera(out vec3 ro, out vec3 ta) {
#if VARIANT == 1
    ro = vec3(0.6, 0.35, -4.2);
    ta = vec3(0.0, 0.25, 0.8);
#else
    ro = vec3(0.4, 2.25, -4.1);
    ta = vec3(0.0, -0.15, 0.2);
#endif
}

vec3 getSky(vec3 rd, out vec3 sunDir) {
    float stepAng = floor(motion * 0.8 + (syn_Hits * response) * 0.5 + (syn_OnBeat * response)) * 0.7854 + (syn_MidLevel * response) * 1.6;
    sunDir = normalize(vec3(cos(stepAng), 0.20 * lightElevation, sin(stepAng)));
#if VARIANT == 1
    // Keep the raking lamp in front of the relief as it sweeps across the wall.
    sunDir = normalize(vec3(cos(stepAng), 0.25 + 0.35 * (syn_MidLevel * response),
                            -0.23 - 0.42 * (syn_Hits * response)));
#endif
    return vec3(0.03, 0.035, 0.045);
}

vec3 shadeSurface(vec3 pos, vec3 nor, vec3 rd, float dist, float matId, vec4 info, vec3 sunDir, vec3 skyCol, vec2 p) {
    if (matId < 0.5) return vec3(0.05, 0.055, 0.06);
    vec4 dummy;
    float sh = 1.0;
    float st = 0.04;
    for (int i = 0; i < 16; i++) {
        float d = mapScene(pos + sunDir * st, dummy).x;
        if (d < 0.005) { sh = 0.08; break; }
        sh = min(sh, clamp(10.0 * d / st, 0.08, 1.0));
        st += clamp(d, 0.04, 0.3);
    }
    float diff = max(dot(nor, sunDir), 0.0) * sh;
    float spec = pow(max(dot(nor, normalize(sunDir - rd)), 0.0), 40.0) * sh;
    vec3 stone = mix(vec3(0.46, 0.44, 0.40), vec3(0.78, 0.74, 0.66), info.z * 2.5);
    return stone * (vec3(1.18, 1.04, 0.84) * (diff * sweepStrength) + 0.08) + vec3(1.0, 0.90, 0.72) * spec * (0.3 + 0.7 * (syn_HighLevel * response)) * sweepStrength;
}

vec3 postMissGlow(vec3 rd, vec2 p) { return vec3(0.0); }


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

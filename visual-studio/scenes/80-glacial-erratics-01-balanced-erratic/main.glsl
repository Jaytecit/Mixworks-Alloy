#version 330 core
// Glacial Erratics / Balanced Erratic
#define VARIANT 0

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_BassHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float rockTilt;
uniform float veinGlow;

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


vec4 sampleMineralVeins(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return vec4(tex.rgb, tex.a * mediaLuma(tex.rgb));
    }
    float vein = 1.0 - smoothstep(0.02, 0.08, abs(sin(cuv.x * 10.0 + sin(cuv.y * 8.0))));
    return vec4(vec3(0.95, 0.82, 0.38), vein);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float iceRecede = -0.35 - 0.52 * bass;
    float dIce = p.y - (iceRecede + 0.15 * sin(p.x * 0.8) + 0.12 * fbm2(p.xz * 1.4));

#if VARIANT == 2
    dIce = max(dIce, -(length(p.xz) - 1.55) * 0.25);
#elif VARIANT == 3
    dIce = max(dIce, -p.z - 0.5);
#endif

    vec2 res = vec2(dIce * 0.72, 1.0);
    int count = (VARIANT == 1) ? 3 : 1;
    for (int i = 0; i < 3; i++) {
        if (i >= count) break;
        float fi = float(i);
        vec3 center = (VARIANT == 1) ? vec3((fi - 1.0) * 2.1, 0.25 + iceRecede + 0.5, fi * 0.9)
                                     : vec3(0.0, 0.55 + iceRecede + 0.4, 0.4);
        vec3 q = p - center;
        q.xz *= rot2(clk * 0.28 + fi * 1.3);
        q.xy *= rot2((0.18 * rockTilt) * sin(clk + bass * 2.0 + fi));
        float facet = 0.18 * abs(sin(q.x * 2.4) * cos(q.y * 2.2) * sin(q.z * 2.5));
        float rB = (VARIANT == 1) ? 0.82 : 1.35;
        float dRock = length(q) - rB - facet;
        if (dRock < res.x) {
            vec2 ruv = clamp(q.xy / (rB * 2.0) + 0.5, 0.0, 1.0);
            res = vec2(dRock * 0.72, 2.0);
            info = vec4(ruv, 0.0, 0.0);
        }
    }
    return res;
}

void getCamera(out vec3 ro, out vec3 ta) {
    ro = vec3(0.4, 1.35, -4.8);
    ta = vec3(0.0, 0.2, 0.5);
}

vec3 getSky(vec3 rd, out vec3 sunDir) {
    sunDir = normalize(vec3(-0.45, 0.65, 0.60));
    return mix(vec3(0.32, 0.44, 0.56), vec3(0.58, 0.74, 0.88), clamp(rd.y * 1.4 + 0.3, 0.0, 1.0));
}

vec3 shadeSurface(vec3 pos, vec3 nor, vec3 rd, float dist, float matId, vec4 info, vec3 sunDir, vec3 skyCol, vec2 p) {
    float diff = max(dot(nor, sunDir), 0.0);
    float spec = pow(max(dot(nor, normalize(sunDir - rd)), 0.0), 48.0);
    if (matId < 1.5) {
        vec3 iceCol = vec3(0.48, 0.78, 0.92);
        vec3 lit = iceCol * (diff + 0.35) + vec3(0.15, 0.52, 0.72) * (0.2 + 0.4 * (syn_MidLevel * response)) + vec3(1.0) * spec * 0.6;
        return mix(lit, skyCol, clamp(1.0 - exp(-dist * 0.04), 0.0, 1.0));
    }
    vec4 vein = sampleMineralVeins(info.xy);
    vec3 granite = vec3(0.28, 0.29, 0.31) * (0.8 + 0.35 * fbm2(pos.xy * 4.0));
    vec3 baseCol = mix(granite, vein.rgb, clamp(vein.w * imprint, 0.0, 1.0));
    vec3 lit = baseCol * (vec3(1.1, 1.04, 0.98) * diff + 0.22)
             + vein.rgb * vein.w * imprint * (0.25 + 0.75 * (syn_HighLevel * response)) * veinGlow
             + vec3(1.0) * spec * (0.3 + 0.7 * (syn_HighLevel * response));
    return mix(lit, skyCol, clamp(1.0 - exp(-dist * 0.04), 0.0, 1.0));
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
    float maxT = 32.0;
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

#version 330 core
// Clock Escapement / Lever Escapement
#define VARIANT 0

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float syn_BassLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_HighHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float balanceSwing;
uniform float jewelGlow;

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


float sampleWheelCut(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return (tex.a < 0.96) ? smoothstep(0.3, 0.6, tex.a) : smoothstep(0.35, 0.78, mediaLuma(tex.rgb));
    }
    float spokes = step(0.25, abs(sin(atan(cuv.y - 0.5, cuv.x - 0.5) * 5.0)));
    return spokes;
}

vec2 sdGear(vec3 q, float r, float teeth, float rotA, out vec2 guv) {
    vec3 rq = q;
    rq.xy *= rot2(rotA);
    float ang = atan(rq.y, rq.x);
    float tooth = 0.11 * smoothstep(0.2, 0.7, cos(ang * teeth));
    float dWheel = max(length(rq.xy) - (r + tooth), abs(rq.z) - 0.085);
    guv = clamp(rq.xy / (r * 2.0) + 0.5, 0.0, 1.0);
    float cut = sampleWheelCut(guv) * imprint;
    if (length(rq.xy) < r * 0.78 && length(rq.xy) > r * 0.24) {
        dWheel = max(dWheel, (cut - 0.45) * 0.15);
    }
    return vec2(dWheel * 0.75, 1.0);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float hits = clamp((syn_Hits * response) + 0.65 * (syn_HighHits * response), 0.0, 1.5);
    float tick = floor(motion * 2.0) + smoothstep(0.75, 1.0, fract(motion * 2.0)) + hits * 0.18;
    float rotA = tick * (TAU / 16.0);
    vec2 guv;
    vec2 res = vec2(1.25 - p.z, 0.0); // dark clockwork backplate

#if VARIANT == 1
    vec2 g1 = sdGear(p - vec3(-0.75, 0.15, 0.2), 0.95, 16.0, rotA, guv);
    if (g1.x < res.x) { res = g1; info = vec4(guv, 0.0, 0.0); }
    vec2 g2 = sdGear(p - vec3(0.85, -0.25, 0.2), 0.75, 12.0, -rotA * 1.33, guv);
    if (g2.x < res.x) { res = g2; info = vec4(guv, 0.0, 0.0); }
#else
    vec2 g1 = sdGear(p - vec3(0.0, (VARIANT == 2) ? -0.7 : -0.25, 0.2),
                     (VARIANT == 2) ? 0.82 : 1.25, 16.0, rotA, guv);
    if (g1.x < res.x) { res = g1; info = vec4(guv, 0.0, 0.0); }
    // Oscillating balance wheel & anchor pallet fork above
    float balOsc = sin(motion * 3.8 + (syn_BassLevel * response) * 2.0) * (1.2 * balanceSwing);
    vec3 bq = p - vec3(0.0, 1.15, -0.15);
    bq.xy *= rot2(balOsc);
    float dBal = sdTorus(bq.xzy, vec2((VARIANT == 2) ? 1.02 : 0.73, 0.065));
    #if VARIANT == 0
        // A balance arm makes the wheel's swing visible on its circular rim.
        dBal = min(dBal, sdBox(bq, vec3(0.70, 0.045, 0.04)));
    #elif VARIANT == 2
        for (int spoke = 0; spoke < 3; ++spoke) {
            vec2 arm = rot2(float(spoke) * PI / 3.0) * bq.xy;
            dBal = min(dBal, sdBox(vec3(arm, bq.z), vec3(1.0, 0.025, 0.045)));
        }
    #endif
    float dFork = sdBox(p - vec3(0.0, 0.65, 0.05), vec3(0.55, 0.06, 0.06));
    res = minMat(res, vec2(min(dBal, dFork), 2.0));
    #if VARIANT == 3
        float weightY = -0.55 + 0.25 * sin(motion * 0.65);
        float weight = sdCappedCylinder(p - vec3(-1.62, weightY, 0.4), 0.46, 0.21);
        float chain = sdCappedCylinder(p - vec3(-1.62, weightY + 1.1, 0.4), 0.67, 0.026);
        res = minMat(res, vec2(min(weight, chain), 2.0));
    #endif
#endif
    return res;
}

void getCamera(out vec3 ro, out vec3 ta) {
    ro = vec3(0.62 + 0.24 * sin(motion * 0.4), 0.65, -5.8);
    ta = vec3(-0.15, 0.25, 0.2);
}

vec3 getSky(vec3 rd, out vec3 sunDir) {
    sunDir = normalize(vec3(0.45, 0.72, -0.52));
    return vec3(0.05, 0.05, 0.06);
}

vec3 shadeSurface(vec3 pos, vec3 nor, vec3 rd, float dist, float matId, vec4 info, vec3 sunDir, vec3 skyCol, vec2 p) {
    float diff = max(dot(nor, sunDir), 0.0);
    float spec = pow(max(dot(nor, normalize(sunDir - rd)), 0.0), 56.0);
    vec3 baseCol = (matId < 0.5) ? vec3(0.09, 0.10, 0.12)
                                 : ((matId < 1.5) ? vec3(0.80, 0.62, 0.26) : vec3(0.76, 0.78, 0.82));
    if (matId > 0.5 && matId < 1.5 && syn_MediaType > 0.5) {
        vec4 art = texture(syn_Media, info.xy);
        baseCol = mix(baseCol, art.rgb, imprint * art.a * 0.62);
    }
    vec3 jewel = vec3(0.95, 0.12, 0.22) * exp(-length(pos.xy - vec2(0.0, 1.15)) * 6.0) * (0.5 + (syn_Hits * response)) * jewelGlow;
    return baseCol * (vec3(1.12, 1.0, 0.84) * diff + 0.18) + vec3(1.0, 0.94, 0.82) * spec * (0.4 + 0.8 * (syn_HighLevel * response)) + jewel;
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

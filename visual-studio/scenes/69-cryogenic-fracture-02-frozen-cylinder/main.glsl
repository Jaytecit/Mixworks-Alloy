#version 330 core
// Cryogenic Fracture / Frozen Cylinder
#define VARIANT 1

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float syn_Level;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_HighHits;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float fractureOpening;
uniform float seamGlow;

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

vec4 sampleHeatInlay(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return vec4(tex.rgb, tex.a * mediaLuma(tex.rgb));
    }
    // Hexagonal radial thermal origin inlay fallback
    float r = length(cuv - 0.5);
    float ring = 1.0 - smoothstep(0.04, 0.14, abs(r - 0.24));
    float core = 1.0 - smoothstep(0.05, 0.12, r);
    float spokes = smoothstep(0.82, 0.96, abs(cos(atan(cuv.y - 0.5, cuv.x - 0.5) * 3.0)));
    float heat = clamp(max(core, max(ring, spokes * (1.0 - smoothstep(0.25, 0.45, r)))), 0.0, 1.0);
    return vec4(vec3(1.0, 0.55, 0.12), heat);
}

// Cellular Voronoi crack distance on 2D slab surface
float crackField(vec2 p, out vec2 cellId) {
    vec2 n = floor(p);
    vec2 f = fract(p);
    float d1 = 8.0;
    float d2 = 8.0;
    cellId = vec2(0.0);
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = vec2(hash21(n + g), hash21(n + g + 17.3));
            vec2 r = g + 0.5 + 0.38 * sin(o * TAU) - f;
            float d = dot(r, r);
            if (d < d1) {
                d2 = d1;
                d1 = d;
                cellId = n + g;
            } else if (d < d2) {
                d2 = d;
            }
        }
    }
    return sqrt(d2) - sqrt(d1);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.5 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float stress = 0.18 + 0.72 * bass + 0.12 * sin(clk * 0.9);

    vec2 res = vec2(p.y + 1.4, 0.0);

#if VARIANT == 0 || VARIANT == 2
    // 01: Thermal Plates & 03: Notched Slab
    vec2 suv = clamp(vec2(p.x / 5.2 + 0.5, p.z / 4.2 + 0.5), 0.0, 1.0);
    vec4 art = sampleHeatInlay(suv);
    float heat = art.w * imprint;

    vec2 cId;
    float edge = crackField(p.xz * 1.15, cId);
    float gapWidth = (0.04 + 0.24 * stress) * (0.35 + 0.95 * heat);
    #if VARIANT == 2
        // Directional stress notches across notched slab
        float notch = abs(mod(p.x + 0.75, 1.5) - 0.75);
        edge = min(edge, notch * 1.4);
    #endif
    float dHot = sdBox(p - vec3(0.0, -0.35, 0.0), vec3(2.6, 0.28, 2.1));
    res = minMat(res, vec2(dHot, 2.0));

    float shiftY = 0.04 * sin(hash21(cId) * 6.0 + clk) * stress;
    float dIce = sdBox(p - vec3(0.0, 0.15 + shiftY, 0.0), vec3(2.65, 0.34, 2.15));
    // Carve thermal contraction cracks + recessed hot inlay pockets
    float dGap = max(gapWidth - edge, (heat - 0.38) * 0.35);
    dIce = max(dIce, dGap);
    if (dIce < res.x) { res = vec2(dIce * 0.70, 1.0); info = vec4(suv, heat, edge); }

#elif VARIANT == 1
    // 02: Frozen Cylinder — radial chilled sleeve cracking around hot glowing core
    vec3 q = p - vec3(0.0, 0.25, 0.2);
    q.xz *= rot2(clk * 0.35);
    float ang = atan(q.z, q.x) / TAU + 0.5;
    vec2 suv = clamp(vec2(ang, q.y / 2.6 + 0.5), 0.0, 1.0);
    vec4 art = sampleHeatInlay(suv);
    float heat = art.w * imprint;
    vec2 cId;
    float edge = crackField(vec2(ang * 7.0, q.y * 1.6), cId);
    float gap = (0.04 + 0.24 * stress) * (0.35 + 0.9 * heat) * fractureOpening;

    float dCore = sdCappedCylinder(q, 1.25, 1.05);
    res = minMat(res, vec2(dCore, 2.0));

    float dShell = max(sdCappedCylinder(q, 1.32, 1.52 + 0.12 * stress),
                       -sdCappedCylinder(q, 1.5, 1.12 + 0.08 * stress));
    dShell = max(dShell, max(gap - edge, (heat - 0.4) * 0.35));
    if (dShell < res.x) { res = vec2(dShell * 0.7, 1.0); info = vec4(suv, heat, edge); }

#else
    // 04: Laminated Block — alternating chilled slate layers delaminating from hot thermal sheets
    vec2 suv = clamp(vec2(p.x / 4.4 + 0.5, p.z / 3.6 + 0.5), 0.0, 1.0);
    vec4 art = sampleHeatInlay(suv);
    float heat = art.w * imprint;
    float delam = 0.06 + 0.22 * stress * (0.4 + 0.8 * heat);
    float ly = mod(p.y + 0.35, 0.70) - 0.35;
    float dHot = sdBox(p - vec3(0.0, 0.45, 0.0), vec3(2.15, 1.1, 1.75));
    res = minMat(res, vec2(dHot, 2.0));
    float dPlate = max(sdBox(p - vec3(0.0, 0.45, 0.0), vec3(2.25, 1.2, 1.85)),
                       -(abs(ly) - delam * 0.5));
    if (dPlate < res.x) { res = vec2(dPlate * 0.72, 1.0); info = vec4(suv, heat, 0.5); }
#endif

    return res;
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

    float mids = (syn_MidLevel * response);
    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_HighHits * response), 0.0, 1.5);
    float hits = (syn_Hits * response);

    vec3 ro = vec3(0.45, 2.65, -4.6);
    vec3 ta = vec3(0.0, 0.1, 0.0);
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 bgCol = mix(vec3(0.04, 0.06, 0.09), vec3(0.11, 0.16, 0.22), clamp(rd.y + 0.4, 0.0, 1.0));

    float t = 0.0;
    float maxT = 22.0;
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
        t += res.x * 0.65;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(-0.45, 0.85, -0.35));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 48.0);
        float fres = pow(1.0 - max(dot(nor, viewDir), 0.0), 3.0);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            baseCol = vec3(0.08, 0.10, 0.13);
        } else if (hit.y < 1.5) {
            // Chilled frosted slate-ice blocks
            float frost = fbm2(pos.xz * 4.5 + pos.xy * 2.0);
            baseCol = mix(vec3(0.32, 0.46, 0.58), vec3(0.75, 0.88, 0.96), frost);
            // Hot thermal scatter at crack rims
            float rimHeat = exp(-hitInfo.w * 4.5) * hitInfo.z;
            emissive = vec3(1.0, 0.42, 0.10) * rimHeat * 0.65 + vec3(0.65, 0.88, 1.0) * fres * (0.25 + 0.45 * highs);
        } else {
            // Hot glowing thermal inlays visible through contraction fissures
            vec2 suv = clamp(vec2(pos.x / 5.2 + 0.5, pos.z / 4.2 + 0.5), 0.0, 1.0);
            vec4 art = sampleHeatInlay(suv);
            vec3 hotHue = mix(vec3(1.0, 0.42, 0.08), art.rgb, 0.45 * imprint);
            baseCol = hotHue;
            emissive = hotHue * (0.95 + 0.65 * mids + 0.55 * (syn_Level * response) + 0.45 * hits) * seamGlow;
        }

        vec3 lit = baseCol * (vec3(0.95, 1.02, 1.12) * diff + vec3(0.16, 0.22, 0.28));
        lit += vec3(0.88, 0.96, 1.0) * spec * (0.35 + 0.75 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.045), 0.0, 1.0));
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

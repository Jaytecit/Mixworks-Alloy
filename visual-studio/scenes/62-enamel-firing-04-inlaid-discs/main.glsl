#version 330 core
// Enamel Firing / Inlaid Discs
#define VARIANT 3

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
uniform float glazeGloss;
uniform float inlayContrast;

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

vec4 sampleEnamelGlaze(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        float lum = mediaLuma(tex.rgb);
        return vec4(tex.rgb, tex.a * (0.24 + 0.76 * lum));
    }
    // Cloisonne geometric rosette channels fallback
    vec2 q = cuv - 0.5;
    float r = length(q);
    float a = atan(q.y, q.x);
    float petal = 0.5 + 0.5 * cos(a * 6.0);
    float rings = smoothstep(0.12, 0.22, abs(sin(r * 14.0)));
    float thick = clamp((1.0 - smoothstep(0.32, 0.48, r)) * (0.4 + 0.6 * petal * rings), 0.0, 1.0);
    vec3 col = mix(vec3(0.08, 0.52, 0.78), vec3(0.92, 0.36, 0.16), petal);
    return vec4(col, thick);
}

vec2 sdEnamelPlate(vec3 q, float radius, float bowlCurve, out vec4 pInfo) {
    pInfo = vec4(0.0);
    vec2 puv = clamp(q.xz / max(radius * 2.0, 0.1) + 0.5, 0.0, 1.0);
    vec4 art = sampleEnamelGlaze(puv);
    float thick = art.w * imprint;

    float r = length(q.xz);
    float dishY = bowlCurve * r * r;
    float recess = thick * 0.12;
    float dCopper = max(length(q.xz) - radius, abs(q.y - dishY + recess * 0.45) - (0.13 - recess * 0.45));

    // Vitreous enamel pool settles inside recessed channels where glaze thickness > 0.15
    if (thick > 0.14 && r < radius * 0.92) {
        float poolTop = dishY + 0.095 + 0.03 * thick;
        float dEnamel = max(length(q.xz) - radius * 0.92, abs(q.y - (poolTop - 0.03)) - 0.035);
        if (dEnamel < dCopper) {
            pInfo = vec4(puv, thick, 1.0);
            return vec2(dEnamel, 2.0);
        }
    }
    pInfo = vec4(puv, thick, 0.0);
    return vec2(dCopper, 1.0);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float clk = motion;
    float bass = clamp((syn_BassLevel * response) + 0.4 * (syn_BassHits * response), 0.0, 1.5);
    vec2 res = vec2(p.y + 1.1, 0.0); // glowing refractory kiln floor
    vec4 pInfo = vec4(0.0);

#if VARIANT == 0
    // 01: Stamped Crest — heavy circular copper crest resting on glowing kiln trivets
    vec3 q = p - vec3(0.0, 0.05, 0.0);
    q.xz *= rot2(clk * 0.35);
    res = minMat(res, vec2(sdCappedCylinder(p - vec3(0.0, -0.55, 0.0), 0.48, 1.65), 3.0));
    vec2 crest = sdEnamelPlate(q, 2.05, 0.02, pInfo);
    if (crest.x < res.x) { res = crest; info = pInfo; }

#elif VARIANT == 1
    // 02: Curved Bowl — deep spun copper vessel tilted toward camera showing glossy enamel pools
    vec3 q = p - vec3(0.0, 0.35, 0.2);
    q.yz *= rot2(-0.42);
    q.xz *= rot2(clk * 0.45);
    vec2 bowl = sdEnamelPlate(q, 2.15, 0.22 + 0.04 * bass, pInfo);
    if (bowl.x < res.x) { res = bowl; info = pInfo; }

#elif VARIANT == 2
    // 03: Split-Glaze Panel — two opposing angled diptych plaques firing over a central hearth slot
    float side = (p.x < 0.0) ? -1.0 : 1.0;
    vec3 q = p - vec3(side * 1.45, 0.2, 0.1);
    q.xy *= rot2(side * 0.34);
    q.yz *= rot2(-0.22);
    vec2 pan = sdEnamelPlate(q, 1.45, 0.04, pInfo);
    if (pan.x < res.x) { res = pan; info = pInfo; }

#else
    // 04: Inlaid Discs — stepped concentric copper medallions turning on a kiln turntable
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        vec3 q = p - vec3((fi - 1.0) * 1.75, -0.15 + fi * 0.24, (fi == 1.0) ? -0.4 : 0.4);
        q.xz *= rot2(clk * (0.4 + 0.2 * fi) * ((i == 1) ? -1.0 : 1.0));
        vec2 disc = sdEnamelPlate(q, 1.15, 0.05, pInfo);
        if (disc.x < res.x) { res = disc; info = pInfo; }
    }
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

    float bass = (syn_BassLevel * response);
    float mids = (syn_MidLevel * response);
    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_Hits * response), 0.0, 1.5);

#if VARIANT == 1
    vec3 ro = vec3(0.65, 5.7, -5.1);
    vec3 ta = vec3(0.0, 0.70, 0.2);
#else
    vec3 ro = vec3(0.0, 2.85, -4.2);
    vec3 ta = vec3(0.0, 0.15, 0.15);
#endif
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 bgCol = mix(vec3(0.08, 0.03, 0.02), vec3(0.22, 0.08, 0.04), clamp(1.0 - rd.y, 0.0, 1.0));

    float t = 0.0;
    float maxT = 22.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);

    for (int i = 0; i < 64; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        vec2 res = mapScene(pos, info);
        if (res.x < 0.002 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            hitInfo = info;
            break;
        }
        t += res.x * 0.72;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(0.45, 0.85, -0.35));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 64.0);
        float fres = pow(1.0 - max(dot(nor, viewDir), 0.0), 3.5);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5 || hit.y > 2.5) {
            // Heated refractory kiln floor & trivets
            float crack = smoothstep(0.65, 0.92, fbm2(pos.xz * 2.5));
            baseCol = vec3(0.14, 0.09, 0.08);
            emissive = vec3(1.0, 0.32, 0.05) * (0.22 + 0.45 * crack + 0.35 * bass);
        } else if (hit.y < 1.5) {
            // Heated brushed copper plate & raised cloisons
            baseCol = vec3(0.76, 0.44, 0.24);
            emissive = vec3(0.95, 0.28, 0.04) * (0.08 + 0.18 * bass);
            spec *= 0.9;
        } else {
            // Glossy vitreous enamel glaze pools whose depth follows sprite luma
            vec4 art = sampleEnamelGlaze(hitInfo.xy);
            float thick = hitInfo.z;
            float irid = 0.5 + 0.5 * sin(thick * 12.0 + motion * 1.8 + TIME);
            vec3 glazeCol = mix(art.rgb * 0.65, art.rgb * 1.25 + vec3(0.05, 0.12, 0.18) * irid * mids, clamp(thick, 0.0, 1.0));
            glazeCol = max(mix(art.rgb * 0.95, glazeCol, inlayContrast), vec3(0.0));
            baseCol = glazeCol;
            emissive = glazeCol * (0.25 + 0.4 * (syn_Level * response)) + vec3(1.0, 0.92, 0.85) * fres * (0.35 * glazeGloss);
            spec *= (2.0 + 1.5 * highs) * glazeGloss;
        }

        vec3 lit = baseCol * (vec3(1.12, 0.96, 0.82) * diff + vec3(0.18, 0.12, 0.10));
        lit += vec3(1.0, 0.95, 0.88) * spec * (0.4 + 0.8 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.05), 0.0, 1.0));
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

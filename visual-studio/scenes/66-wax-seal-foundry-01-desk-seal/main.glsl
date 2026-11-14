#version 330 core
// Wax Seal Foundry / Desk Seal
#define VARIANT 0

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
uniform float pressTravel;
uniform float waxTranslucency;

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

vec4 sampleSealDepth(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    float edgeMask = 1.0 - smoothstep(0.36, 0.48, length(cuv - 0.5));
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        float lum = mediaLuma(tex.rgb);
        float depth = tex.a * (0.25 + 0.75 * lum) * edgeMask;
        return vec4(tex.rgb, depth);
    }
    // Heraldic compass crest fallback
    vec2 q = cuv - 0.5;
    float r = length(q);
    float a = atan(q.y, q.x);
    float star = 1.0 - smoothstep(0.06, 0.24, r - 0.09 * cos(a * 6.0));
    float ring = 1.0 - smoothstep(0.02, 0.05, abs(r - 0.30));
    float depth = clamp(max(star, ring), 0.0, 1.0) * edgeMask;
    return vec4(vec3(0.85, 0.22, 0.16), depth);
}

// Brass signet stamp with turned handle
float sdBrassStamp(vec3 q, float r) {
    float dDie = sdCappedCylinder(q, 0.14, r);
    float dNeck = sdCappedCylinder(q - vec3(0.0, 0.32, 0.0), 0.24, r * 0.42);
    float dKnob = length(q - vec3(0.0, 0.72, 0.0)) - r * 0.56;
    return smin(min(dDie, dNeck), dKnob, 0.08);
}

// Wax medallion with organic rim & sprite-carved engraved relief depth
vec2 sdWaxPool(vec3 q, float radius, float softRim, float pressPress, out vec4 wInfo) {
    wInfo = vec4(0.0);
    float r = length(q.xz);
    float ang = atan(q.z, q.x);
    float wobble = softRim * (0.14 * sin(ang * 5.0 + 0.7) + 0.10 * fbm2(q.xz * 2.2));
    float rimBead = 0.065 * exp(-pow((r - (radius * 0.84 + wobble * 0.4)) * 3.8, 2.0));

    vec2 suv = clamp(q.xz / max(radius * 1.85, 0.1) + 0.5, 0.0, 1.0);
    vec4 art = sampleSealDepth(suv);
    float engrave = art.w * imprint * (0.09 + 0.05 * pressPress);
    float topY = 0.14 + rimBead - engrave;

    float dWax = max(r - (radius + wobble), abs(q.y - topY * 0.5) - topY * 0.5);
    wInfo = vec4(suv, engrave, 1.0);
    return vec2(dWax * 0.72, 2.0);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float hits = clamp((syn_Hits * response) + 0.7 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float pressCycle = clamp(0.5 + 0.5 * sin(clk * 1.45) - 0.55 * hits, 0.0, 1.0);
    float stampLift = mix(0.16, 1.38, pressCycle * pressTravel);

    vec2 res = vec2(p.y + 0.42, 0.0); // parchment / slate press bed
    vec4 wInfo = vec4(0.0);

#if VARIANT == 0
    // 01: Desk Seal — single heavy brass signet stamp pulling away from crimson wax medallion
    vec2 wax = sdWaxPool(p - vec3(0.0, -0.40, 0.0), 1.48 + 0.08 * bass, 0.45, 1.0 - pressCycle, wInfo);
    if (wax.x < res.x) { res = wax; info = wInfo; }
    vec3 qs = p - vec3(0.0, stampLift, 0.0);
    qs.xy *= rot2(0.12 * pressCycle);
    res = minMat(res, vec2(sdBrassStamp(qs, 1.05), 1.0));

#elif VARIANT == 1
    // 02: Repeating Press — assembly belt of three medallions passing under reciprocating press die
    float beltX = mod(p.x - clk * 1.4 + 2.2, 4.4) - 2.2;
    vec2 wax = sdWaxPool(vec3(beltX, p.y + 0.40, p.z), 1.15, 0.25, 1.0 - pressCycle, wInfo);
    if (wax.x < res.x) { res = wax; info = wInfo; }
    // Fixed central vertical press head
    float dHead = sdBrassStamp(p - vec3(0.0, stampLift * 1.16 + 0.32, 0.0), 0.95);
    float dRail = sdBox(p - vec3(0.0, -0.44, 0.0), vec3(8.0, 0.04, 1.55));
    res = minMat(res, vec2(min(dHead, dRail), 1.0));

#elif VARIANT == 2
    // 03: Split Impression — twin opposing semicircular dies converging onto rectangular wax bar
    vec2 wax = sdWaxPool(vec3(p.x * 0.85, p.y + 0.40, p.z), 1.55, 0.20, 1.0 - pressCycle, wInfo);
    if (wax.x < res.x) { res = wax; info = wInfo; }
    float splitGap = 0.28 + 0.78 * pressCycle;
    vec3 qHalf = vec3(abs(p.x) - splitGap - 0.55, p.y - mix(0.30, 1.18, pressCycle), p.z);
    float dDie = sdBox(qHalf, vec3(0.55, 0.16, 1.15));
    res = minMat(res, vec2(dDie, 1.0));

#else
    // 04: Softened Edge — broad low-angle molten wax pool with wide organic border drips
    vec2 wax = sdWaxPool(p - vec3(0.0, -0.40, 0.0), 1.85 + 0.12 * bass, 1.15, 1.0 - pressCycle, wInfo);
    if (wax.x < res.x) { res = wax; info = wInfo; }
    vec3 qs = p - vec3(0.35 * pressCycle, stampLift + 0.15, 0.25 * pressCycle);
    qs.yz *= rot2(-0.18 * pressCycle);
    res = minMat(res, vec2(sdBrassStamp(qs, 1.15), 1.0));
#endif

    return res;
}

vec3 calcNormal(vec3 p) {
    vec4 dummy;
    vec2 e = vec2(1.0, -1.0) * 0.003;
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
    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_Hits * response), 0.0, 1.5);

#if VARIANT == 1
    vec3 ro = vec3(0.60, 3.25, -4.5);
    vec3 ta = vec3(-1.10, -0.12, 0.0);
#elif VARIANT == 2
    vec3 ro = vec3(1.25, 3.65, -4.25);
    vec3 ta = vec3(0.0, -0.18, 0.0);
#elif VARIANT == 3
    vec3 ro = vec3(0.2, 1.25, -3.9);
    vec3 ta = vec3(0.0, -0.15, 0.0);
#else
    vec3 ro = vec3(0.65, 2.25, -3.7);
    vec3 ta = vec3(0.0, -0.05, 0.0);
#endif

    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 bgCol = mix(vec3(0.07, 0.05, 0.05), vec3(0.16, 0.12, 0.10), clamp(rd.y + 0.4, 0.0, 1.0));

    float t = 0.0;
    float maxT = 18.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);

    for (int i = 0; i < 68; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        vec2 res = mapScene(pos, info);
        if (res.x < 0.0018 * (1.0 + 0.25 * t)) {
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
        vec3 keyDir = normalize(vec3(-0.65, 0.52, -0.55));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 56.0);
        float fres = pow(1.0 - max(dot(nor, viewDir), 0.0), 3.5);

        vec3 baseCol = vec3(0.2);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            // Textured parchment desk surface
            float paper = fbm2(pos.xz * 6.5);
            baseCol = mix(vec3(0.56, 0.50, 0.40), vec3(0.42, 0.36, 0.28), paper);
            spec *= 0.2;
        } else if (hit.y < 1.5) {
            // Turned machined brass stamp
            baseCol = vec3(0.78, 0.58, 0.24);
            spec *= (1.4 + highs);
        } else {
            // Glossy crimson/amber sealing wax with subsurface scatter & relief shading
            vec4 art = sampleSealDepth(hitInfo.xy);
            vec3 waxHue = (VARIANT == 3) ? vec3(0.76, 0.42, 0.10) : vec3(0.68, 0.09, 0.08);
            baseCol = mix(waxHue, art.rgb * 0.85, clamp(hitInfo.z * 5.0 * imprint, 0.0, 0.65));
            // Subsurface wax glow through thin relief ridges
            float sss = pow(1.0 - abs(dot(nor, viewDir)), 2.0) * 0.45 + 0.35;
            emissive = waxHue * (sss * waxTranslucency) * (0.28 + 0.35 * mids + 0.35 * (syn_Level * response)) + vec3(1.0, 0.85, 0.7) * fres * 0.22;
            spec *= (1.8 + 1.4 * highs);
        }

        vec3 lit = baseCol * (vec3(1.18, 0.98, 0.80) * diff + vec3(0.18, 0.14, 0.13));
        lit += vec3(1.0, 0.92, 0.80) * spec * (0.35 + 0.75 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.048), 0.0, 1.0));
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

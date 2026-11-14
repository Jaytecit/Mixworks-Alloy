#version 330 core
// Pneumatic Dispatch / Transparent Carousel
#define VARIANT 3

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_Level;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_MidHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float carouselRadius;
uniform float airWaveStrength;

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

vec4 sampleCapsuleLabel(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    float band = smoothstep(0.05, 0.12, cuv.y) * (1.0 - smoothstep(0.88, 0.95, cuv.y));
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return vec4(tex.rgb, tex.a * band);
    }
    // Procedural pneumatic routing label fallback
    float stripe = step(0.35, fract(cuv.x * 4.0));
    float ticks = step(0.65, fract(cuv.x * 16.0)) * step(0.68, cuv.y);
    float pat = clamp(max(stripe * 0.65, ticks), 0.0, 1.0) * band;
    vec3 col = mix(vec3(0.95, 0.42, 0.14), vec3(0.20, 0.85, 0.95), step(0.5, cuv.x));
    return vec4(col, pat);
}

// Capsule oriented along local Z axis with wrapped artwork sleeve and brass end-caps
vec2 sdDispatchCapsule(vec3 q, float r, float halfLen, out vec4 cInfo) {
    cInfo = vec4(0.0);
    float dBody = sdCappedCylinder(q.xzy, halfLen, r);
    float dCap = sdCappedCylinder(vec3(q.x, abs(q.z) - halfLen * 0.92, q.y), halfLen * 0.16, r * 1.04);
    float ang = atan(q.y, q.x) / TAU + 0.5;
    float vCoord = clamp(q.z / max(halfLen * 1.65, 0.1) + 0.5, 0.0, 1.0);
    cInfo = vec4(ang, vCoord, 0.0, 0.0);
    if (dCap < dBody) return vec2(dCap, 3.0);
    return vec2(dBody, 4.0);
}

// Evaluates both opaque interior hardware AND transparent glass pipe distance (for Fresnel shell)
vec2 mapScene(vec3 p, out vec4 info, out float glassDist) {
    info = vec4(0.0);
    glassDist = 99.0;
    float hits = clamp((syn_Hits * response) + 0.65 * (syn_MidHits * response) + 0.4 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float switchPhase = smoothstep(0.25, 0.75, 0.5 + 0.5 * sin(clk * 0.9 + hits * 1.4));

    vec2 res = vec2(p.y + 2.4, 0.0); // dark architectural floor
    vec4 cInfo = vec4(0.0);

#if VARIANT == 0
    // 01: Express Tube — twin curved horizontal transparent pipes with streaking wrapped capsules
    float curveZ = 0.55 * sin(p.x * 0.25);
    for (int lane = 0; lane < 2; lane++) {
        float fl = float(lane);
        float ty = (lane == 0) ? 0.55 : -0.55;
        float tz = curveZ + (fl - 0.5) * 1.3;
        vec3 pTube = vec3(p.x, p.y - ty, p.z - tz);
        float dShell = abs(length(pTube.yz) - 0.46) - 0.03;
        glassDist = min(glassDist, dShell);

        // Brass mounting collars every 2.8m
        float colX = mod(p.x + 1.4, 2.8) - 1.4;
        float dCollar = max(length(vec2(pTube.y, pTube.z)) - 0.53, abs(colX) - 0.08);
        float dBracket = sdBox(vec3(colX, p.y - ty + 0.85, p.z - tz), vec3(0.07, 0.45, 0.08));
        res = minMat(res, vec2(min(dCollar, dBracket), 2.0));

        // Racing capsule inside tube
        float dir = (lane == 0) ? 1.0 : -1.0;
        float capX = mod(p.x - dir * clk * (3.8 + fl * 0.9) + 4.5, 9.0) - 4.5;
        vec2 capRes = sdDispatchCapsule(vec3(pTube.z, pTube.y, capX), 0.34, 0.72, cInfo);
        if (capRes.x < res.x) { res = capRes; info = cInfo; }
    }

#elif VARIANT == 1
    // 02: Branch Exchange — Y-junction manifold routing capsules left/right on percussion hits
    float trunkX = 0.0;
    float branchSpread = smoothstep(-0.5, 4.5, p.z) * 1.85;
    float dTrunk = abs(length(vec2(p.x - trunkX, p.y)) - 0.46) - 0.03;
    float dBrL = abs(length(vec2(p.x + branchSpread, p.y)) - 0.44) - 0.03;
    float dBrR = abs(length(vec2(p.x - branchSpread, p.y)) - 0.44) - 0.03;
    glassDist = min(glassDist, (p.z < -0.5) ? dTrunk : min(dBrL, dBrR));

    // Brass Y-junction diverter housing at z = -0.5
    float dValve = sdBox(p - vec3(0.0, 0.0, -0.5), vec3(0.75, 0.58, 0.55));
    float dGate = sdBox(p - vec3(mix(-0.22, 0.22, switchPhase), 0.0, 0.2), vec3(0.08, 0.42, 0.65));
    res = minMat(res, vec2(min(dValve, dGate), 2.0));

    // Capsule traveling through Y-junction and turning down active branch
    float cz = mod(p.z - clk * 3.4 + 4.0, 8.0) - 4.0;
    float zPos = p.z - cz;
    float routeSide = mix(-1.0, 1.0, step(0.5, switchPhase));
    float cx = (zPos > -0.5) ? routeSide * smoothstep(-0.5, 4.5, zPos) * 1.85 : 0.0;
    vec3 qCap = vec3(p.x - cx, p.y, cz);
    qCap.xz *= rot2((zPos > -0.5) ? -routeSide * 0.32 : 0.0);
    vec2 capRes = sdDispatchCapsule(qCap, 0.33, 0.68, cInfo);
    if (capRes.x < res.x) { res = capRes; info = cInfo; }

#elif VARIANT == 2
    // 03: Vertical Drop — bank of 3 vertical transparent chutes plunging into brass receiver bays
    float laneX = clamp(round(p.x / 1.9), -1.0, 1.0) * 1.9;
    float laneIdx = round(p.x / 1.9) + 1.0;
    vec3 pCol = vec3(p.x - laneX, p.y, p.z - 0.6);
    glassDist = min(glassDist, abs(length(pCol.xz) - 0.44) - 0.03);

    // Brass cushion receivers at y = -1.6
    float dRec = sdCappedCylinder(vec3(pCol.x, p.y + 1.65, pCol.z), 0.35, 0.56);
    float dHeader = sdBox(p - vec3(0.0, 2.35, 0.6), vec3(2.8, 0.26, 0.55));
    res = minMat(res, vec2(min(dRec, dHeader), 2.0));

    // Vertical plummeting capsules switching active lanes with hits
    float dropSpeed = 3.2 + laneIdx * 0.6 + 0.5 * hits;
    float cy = mod(p.y + clk * dropSpeed + laneIdx * 2.1 + 3.0, 6.0) - 3.0;
    vec2 capRes = sdDispatchCapsule(vec3(pCol.x, pCol.z, cy), 0.32, 0.64, cInfo);
    if (capRes.x < res.x) { res = capRes; info = cInfo; }

#else
    // 04: Transparent Carousel — toroidal orbital tube ring around central brass air turbine
    vec3 pRing = p - vec3(0.0, 0.25, 0.8);
    pRing.yz *= rot2(0.45);
    float rMaj = 2.45 * carouselRadius;
    float dTor = abs(sdTorus(pRing, vec2(rMaj, 0.44))) - 0.03;
    glassDist = min(glassDist, dTor);

    // Central brass compressor hub & radial strut arms
    float dHub = sdCappedCylinder(pRing, 0.65, 0.85);
    float ang = atan(pRing.z, pRing.x);
    float sec = TAU / 4.0;
    float aMod = mod(ang + 0.5 * sec, sec) - 0.5 * sec;
    float rXZ = length(pRing.xz);
    vec3 pArm = vec3(cos(aMod) * rXZ - 1.5 * carouselRadius, pRing.y, sin(aMod) * rXZ);
    float dArm = sdBox(pArm, vec3(0.85 * carouselRadius, 0.12, 0.14));
    res = minMat(res, vec2(min(dHub, dArm), 2.0));

    // Orbiting capsules inside carousel ring (4 capsules evenly spaced)
    float orbitAng = ang - clk * (1.25 + 0.35 * switchPhase);
    float cSec = TAU / 4.0;
    float caMod = mod(orbitAng + 0.5 * cSec, cSec) - 0.5 * cSec;
    vec3 qCap = vec3(rXZ - rMaj, pRing.y, caMod * rMaj);
    vec2 capRes = sdDispatchCapsule(qCap, 0.31, 0.62, cInfo);
    if (capRes.x < res.x) { res = capRes; info = cInfo; }
#endif

    return res;
}

vec3 calcNormal(vec3 p) {
    vec4 dummy;
    float gDummy;
    vec2 e = vec2(1.0, -1.0) * 0.0035;
    return normalize(
        e.xyy * mapScene(p + e.xyy, dummy, gDummy).x +
        e.yyx * mapScene(p + e.yyx, dummy, gDummy).x +
        e.yxy * mapScene(p + e.yxy, dummy, gDummy).x +
        e.xxx * mapScene(p + e.xxx, dummy, gDummy).x
    );
}

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;

    float mids = (syn_MidLevel * response);
    float highs = (syn_HighLevel * response);
    float hits = clamp((syn_Hits * response) + 0.5 * (syn_MidHits * response), 0.0, 1.5);

#if VARIANT == 0
    vec3 ro = vec3(0.0, 0.45, -5.6);
    vec3 ta = vec3(0.0, 0.05, 0.4);
#elif VARIANT == 1
    vec3 ro = vec3(3.5, 3.05, -4.9);
    vec3 ta = vec3(0.0, 0.05, 1.6);
#elif VARIANT == 2
    vec3 ro = vec3(0.0, 0.35, -5.6);
    vec3 ta = vec3(0.0, 0.25, 0.6);
#else
    vec3 ro = vec3(0.0, 1.65, -5.8);
    vec3 ta = vec3(0.0, 0.15, 0.8);
#endif

    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.55 * cw);

    vec3 bgCol = mix(vec3(0.05, 0.07, 0.10), vec3(0.14, 0.18, 0.24), clamp(rd.y + 0.4, 0.0, 1.0));

    float t = 0.0;
    float maxT = 26.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);
    float glassAccum = 0.0;

    for (int i = 0; i < 68; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        float gDist;
        vec2 res = mapScene(pos, info, gDist);
        // Accumulate translucent glass tube rim glow along ray
        float nearGlass = exp(-max(gDist, 0.0) * 18.0);
        glassAccum = max(glassAccum, nearGlass);
        if (res.x < 0.0022 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            hitInfo = info;
            break;
        }
        t += res.x * 0.68;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(0.45, 0.78, -0.45));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 44.0);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            baseCol = vec3(0.08, 0.10, 0.12);
        } else if (hit.y < 2.5) {
            // Brushed brass pneumatic manifolds & collars
            baseCol = vec3(0.68, 0.50, 0.22);
            spec *= (1.4 + highs);
            vec2 routingUV = fract(vec2(pos.x * 0.29 + pos.z * 0.19, pos.y * 0.52 + 0.5));
            vec4 routingLabel = sampleCapsuleLabel(routingUV);
            baseCol = mix(baseCol, routingLabel.rgb, routingLabel.a * imprint * 0.66);
        } else if (hit.y < 3.5) {
            // The brass end caps carry matching dispatch seals.
            vec4 seal = sampleCapsuleLabel(hitInfo.xy * vec2(1.0, 0.0) + vec2(0.0, 0.5));
            baseCol = mix(vec3(0.75, 0.56, 0.25), seal.rgb, seal.a * imprint * 0.65);
        } else {
            // Racing dispatch capsule wrapped with Nested artwork label
            vec4 art = sampleCapsuleLabel(hitInfo.xy);
            vec3 metalBody = vec3(0.82, 0.84, 0.86);
            baseCol = mix(metalBody, art.rgb, clamp(art.w * imprint, 0.0, 1.0));
            emissive = art.rgb * art.w * imprint * (0.35 + 0.65 * (syn_Level * response));
        }

        vec3 lit = baseCol * (vec3(1.1, 1.02, 0.92) * diff + vec3(0.16, 0.20, 0.26));
        lit += vec3(1.0, 0.94, 0.82) * spec * (0.35 + 0.65 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.045), 0.0, 1.0));
    }

    // Composite translucent glass tube reflections & air pressure pulse
    float airWave = 0.5 + 0.5 * sin((p.x + p.y) * 14.0 - motion * 8.0 + TIME * 3.0);
    vec3 glassTint = mix(vec3(0.35, 0.78, 0.98), vec3(0.85, 0.95, 1.0), clamp(airWave * mids * airWaveStrength, 0.0, 1.0));
    col += glassTint * glassAccum * (0.13 + (0.13 * mids + 0.13 * hits) * airWaveStrength);

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

#version 330 core
// Elevator Shaft Atlas / Diagonal Funicular
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
uniform float syn_MidHits;
uniform float syn_HighHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float cageTravel;
uniform float shaftLightSweep;

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

vec4 sampleShaftPanel(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    float frame = smoothstep(0.02, 0.06, cuv.x) * (1.0 - smoothstep(0.94, 0.98, cuv.x))
                * smoothstep(0.02, 0.06, cuv.y) * (1.0 - smoothstep(0.94, 0.98, cuv.y));
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return vec4(tex.rgb, tex.a * frame);
    }
    // Procedural Atlas floor telemetry plate fallback
    float grid = step(0.12, fract(cuv.y * 6.0));
    float arrow = 1.0 - smoothstep(0.08, 0.22, abs(cuv.x - 0.5) + abs(cuv.y - 0.5) * 0.6);
    float bar = step(0.45, fract(cuv.x * 5.0)) * step(0.65, cuv.y);
    float pat = clamp(max(arrow, bar) * grid, 0.0, 1.0) * frame;
    vec3 col = mix(vec3(0.18, 0.78, 0.96), vec3(0.98, 0.65, 0.22), step(0.5, cuv.y));
    return vec4(col, pat);
}

// Elevator cage with ribbed frame + passing artwork display panel facing camera
vec2 sdElevatorCage(vec3 q, vec3 halfSize, out vec4 pInfo) {
    pInfo = vec4(0.0);
    float dFrame = max(sdRoundBox(q, halfSize, 0.04),
                       -sdBox(q, halfSize - vec3(0.14, 0.14, 0.22)));
    float dCore = sdBox(q, halfSize - vec3(0.12, 0.12, 0.12));
    float dRoof = sdBox(q - vec3(0.0, halfSize.y + 0.08, 0.0), vec3(halfSize.x * 0.85, 0.06, halfSize.z * 0.85));
    vec2 res = vec2(min(dFrame, dRoof), 1.0);
    res = minMat(res, vec2(dCore, 6.0));

    // Passing panel mounted across front/rear face
    vec2 puv = vec2(q.x / max(halfSize.x * 1.75, 0.1) + 0.5,
                    q.y / max(halfSize.y * 1.75, 0.1) + 0.5);
    if (abs(q.x) < halfSize.x * 0.86 && abs(q.y) < halfSize.y * 0.86) {
        float dPanel = sdBox(vec3(q.x, q.y, abs(q.z) - halfSize.z - 0.025),
                             vec3(halfSize.x * 0.84, halfSize.y * 0.84, 0.02));
        if (dPanel < res.x) {
            res = vec2(dPanel, 5.0);
            pInfo = vec4(puv, 1.0, 0.0);
        }
    }
    return res;
}

// Stacked cast-iron counterweight frame
float sdCounterweight(vec3 q, vec2 sz) {
    float plateY = mod(q.y + 0.12, 0.24) - 0.12;
    float dPlates = max(sdBox(q, vec3(sz.x, sz.y, 0.16)),
                        -(abs(plateY) - 0.015));
    float dCarriage = sdBox(vec3(abs(q.x) - sz.x - 0.04, q.y, q.z), vec3(0.05, sz.y + 0.1, 0.18));
    return min(dPlates, dCarriage);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.4 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    vec4 pInfo = vec4(0.0);

    vec2 res = vec2(99.0, 0.0);

#if VARIANT == 0
    // 01: Central Cage — tall vertical shaft with central cage + flanking counterweights
    float shaftBox = min(4.2 - abs(p.x), 5.0 - p.z);
    res = minMat(res, vec2(shaftBox, 0.0));

    // Repeating floor landings & glowing floor marker rings every 3.6m in Y
    float fy = mod(p.y + 1.8, 3.6) - 1.8;
    float dRing = max(sdBox(vec3(p.x, fy, p.z - 1.2), vec3(4.25, 0.12, 3.85)),
                      -sdBox(vec3(p.x, fy, p.z - 1.2), vec3(3.55, 0.3, 3.15)));
    float dMarker = sdBox(vec3(p.x, fy, p.z - 1.2), vec3(3.62, 0.045, 3.22));
    dMarker = max(dMarker, -sdBox(vec3(p.x, fy, p.z - 1.2), vec3(3.48, 0.2, 3.08)));
    res = minMat(res, vec2(dRing, 0.0));
    res = minMat(res, vec2(dMarker, 3.0));

    // Guide rails & vertical cables
    float dRail = sdBox(vec3(abs(p.x) - 1.35, 0.0, p.z - 1.4), vec3(0.07, 40.0, 0.09));
    float cableX = mod(p.x + 0.22, 0.44) - 0.22;
    float dCab = sdCappedCylinder(vec3(cableX, 0.0, p.z - 1.4), 40.0, 0.028);
    if (abs(p.x) < 1.0) res = minMat(res, vec2(dCab, 4.0));
    res = minMat(res, vec2(dRail, 1.0));

    // Repeating passing central cages along infinite shaft
    float travel = clk * 2.6 + 0.35 * bass;
    float cy = mod(p.y - travel + 5.0, 10.0) - 5.0;
    vec2 cRes = sdElevatorCage(vec3(p.x, cy, p.z - 1.4), vec3(1.18, 1.45, 0.95), pInfo);
    if (cRes.x < res.x) { res = cRes; info = pInfo; }

    // Flanking counterweights moving in opposite direction
    float wY = mod(p.y + travel + 5.0, 10.0) - 5.0;
    float dCW = sdCounterweight(vec3(abs(p.x) - 2.65, wY, p.z - 1.6), vec2(0.55, 1.25));
    res = minMat(res, vec2(dCW, 2.0));

#elif VARIANT == 1
    // 02: Side-by-Side Lifts — dual opposing lift cars racing across a twin-bay shaft
    float shaftBox = min(4.8 - abs(p.x), 4.8 - p.z);
    float dDivider = sdBox(vec3(p.x, 0.0, p.z - 1.2), vec3(0.12, 40.0, 2.8));
    res = minMat(res, vec2(min(shaftBox, dDivider), 0.0));

    float fy = mod(p.y + 1.6, 3.2) - 1.6;
    float dMarker = sdBox(vec3(abs(p.x) - 2.2, fy, p.z + 1.2), vec3(1.8, 0.05, 0.08));
    res = minMat(res, vec2(dMarker, 3.0));

    float travel = clk * 2.8 + 0.3 * bass;
    float yL = mod(p.y - travel + 4.5, 9.0) - 4.5;
    float yR = mod(p.y + travel + 4.5, 9.0) - 4.5;
    vec2 cL = sdElevatorCage(vec3(p.x + 1.85, yL, p.z - 1.2), vec3(1.12, 1.38, 0.92), pInfo);
    if (cL.x < res.x) { res = cL; info = pInfo; }
    vec2 cR = sdElevatorCage(vec3(p.x - 1.85, yR, p.z - 1.2), vec3(1.12, 1.38, 0.92), pInfo);
    if (cR.x < res.x) { res = cR; info = pInfo; }

    float dCab = sdCappedCylinder(vec3(abs(p.x) - 1.85, 0.0, p.z - 1.2), 40.0, 0.04);
    res = minMat(res, vec2(dCab, 4.0));

#elif VARIANT == 2
    // 03: Counterweight View — looking past heavy foreground counterweight toward distant illuminated cage
    float shaftBox = min(4.0 - abs(p.x), 6.0 - p.z);
    res = minMat(res, vec2(shaftBox, 0.0));

    float fy = mod(p.y + 1.5, 3.0) - 1.5;
    float dMarker = sdBox(vec3(abs(p.x) - 3.6, fy, p.z - 1.5), vec3(0.18, 0.06, 2.8));
    res = minMat(res, vec2(dMarker, 3.0));

    float travel = clk * 2.4 + 0.35 * bass;
    // Heavy counterweight drops right past foreground lens
    float wY = mod(p.y + travel + 4.5, 9.0) - 4.5;
    float dCW = sdCounterweight(vec3(p.x + 0.75, wY, p.z + 0.45), vec2(0.82, 1.35));
    res = minMat(res, vec2(dCW, 2.0));

    // Main elevator cage rising behind cable curtain
    float cY = mod(p.y - travel + 4.5, 9.0) - 4.5;
    vec2 cRes = sdElevatorCage(vec3(p.x - 0.45, cY, p.z - 2.2), vec3(1.32, 1.55, 1.0), pInfo);
    if (cRes.x < res.x) { res = cRes; info = pInfo; }

    float cableX = mod(p.x + 0.16, 0.32) - 0.16;
    if (abs(p.x + 0.75) < 0.75) {
        res = minMat(res, vec2(sdCappedCylinder(vec3(cableX, 0.0, p.z + 0.45), 40.0, 0.028), 4.0));
    }

#else
    // 04: Diagonal Funicular — steep 36-degree inclined shaft with diagonal cages & skewed floor rings
    vec3 pInc = p - vec3(0.0, 0.2, 1.2);
    pInc.yz *= rot2(0.62);

    float shaftBox = min(3.8 - abs(pInc.x), 3.2 - pInc.z);
    res = minMat(res, vec2(shaftBox, 0.0));

    float fy = mod(pInc.y + 1.75, 3.5) - 1.75;
    float dMarker = sdBox(vec3(abs(pInc.x) - 3.4, fy, pInc.z), vec3(0.18, 0.06, 2.6));
    res = minMat(res, vec2(dMarker, 3.0));

    // Twin inclined rails
    res = minMat(res, vec2(sdBox(vec3(abs(pInc.x) - 1.4, 0.0, pInc.z - 0.9), vec3(0.08, 40.0, 0.10)), 1.0));

    float travel = clk * 2.5 + 0.3 * bass;
    float halfTravel = 5.0 * cageTravel;
    float cy = mod(pInc.y - travel + halfTravel, 2.0 * halfTravel) - halfTravel;
    vec2 cRes = sdElevatorCage(vec3(pInc.x, cy, pInc.z), vec3(1.22, 1.42, 0.92), pInfo);
    if (cRes.x < res.x) { res = cRes; info = pInfo; }

    float wy = mod(pInc.y + travel + halfTravel, 2.0 * halfTravel) - halfTravel;
    res = minMat(res, vec2(sdCounterweight(pInc - vec3(2.3, wy, 0.3), vec2(0.45, 1.1)), 2.0));
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
    float highs = clamp((syn_HighLevel * response) + 0.6 * (syn_HighHits * response), 0.0, 1.5);
    float midPulse = clamp(mids + 0.6 * (syn_MidHits * response), 0.0, 1.5);

#if VARIANT == 0
    vec3 ro = vec3(0.0, -0.8, -3.4);
    vec3 ta = vec3(0.0, 1.15, 1.4);
#elif VARIANT == 1
    vec3 ro = vec3(0.0, -0.4, -3.8);
    vec3 ta = vec3(0.0, 0.5, 1.2);
#elif VARIANT == 2
    vec3 ro = vec3(0.55, -0.35, -3.4);
    vec3 ta = vec3(0.0, 0.6, 1.6);
#else
    vec3 ro = vec3(1.6, -1.2, -3.8);
    vec3 ta = vec3(0.0, 0.85, 1.2);
#endif

    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.45 * cw);

    vec3 ambCol = vec3(0.04, 0.06, 0.09);
    float t = 0.0;
    float maxT = 26.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);

    for (int i = 0; i < 64; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        vec2 res = mapScene(pos, info);
        if (res.x < 0.0022 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            hitInfo = info;
            break;
        }
        t += res.x * 0.72;
        if (t > maxT) break;
    }

    vec3 col = ambCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;

        vec3 keyDir = normalize(vec3(0.25, 0.72, -0.65));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 40.0);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            // Concrete shaftway walls
            float ribs = 0.85 + 0.15 * sin(pos.y * 8.0);
            baseCol = vec3(0.13, 0.15, 0.18) * ribs;
            vec2 markerUV = fract(vec2(pos.x * 0.26 + pos.z * 0.28, pos.y / 3.4) + 0.5);
            vec4 marker = sampleShaftPanel(markerUV);
            float scan = 0.50 + 0.50 * sin(pos.y * 1.6 - motion * 3.4);
            emissive = marker.rgb * marker.a * imprint * scan * (0.13 + 0.52 * midPulse) * shaftLightSweep;
        } else if (hit.y < 1.5) {
            // Structural steel cage frame & guide rails
            baseCol = vec3(0.32, 0.35, 0.39);
            spec *= (1.3 + 0.8 * highs);
        } else if (hit.y < 2.5) {
            // Cast-iron counterweight stack
            baseCol = vec3(0.18, 0.17, 0.19);
        } else if (hit.y < 3.5) {
            // Luminous floor markers driven by mids
            float sweep = 0.5 + 0.5 * sin(pos.y * 0.85 - motion * 3.5 + TIME * 1.5);
            baseCol = vec3(0.22, 0.65, 0.85);
            emissive = mix(vec3(0.15, 0.72, 0.98), vec3(1.0, 0.62, 0.18), sweep)
                     * (0.45 + 1.35 * midPulse) * shaftLightSweep;
        } else if (hit.y < 4.5) {
            // Tensioned hoist steel cables
            baseCol = vec3(0.45, 0.50, 0.56);
            spec *= (1.4 + highs);
        } else if (hit.y < 5.5) {
            // Passing artwork display panel on elevator cage
            vec4 art = sampleShaftPanel(hitInfo.xy);
            baseCol = mix(vec3(0.12, 0.18, 0.24), art.rgb, clamp(art.w, 0.0, 1.0));
            emissive = art.rgb * art.w * (0.55 + 0.85 * (syn_Level * response)) * (0.25 + 0.75 * imprint);
        } else {
            // Illuminated interior cabin glass core
            baseCol = vec3(0.28, 0.36, 0.44);
            emissive = vec3(0.95, 0.82, 0.62) * (0.35 + 0.3 * (syn_Hits * response));
        }

        vec3 lit = baseCol * (vec3(1.0, 0.95, 0.88) * diff + vec3(0.14, 0.20, 0.28));
        lit += vec3(0.95, 0.96, 1.0) * spec * (0.35 + 0.65 * highs);
        lit += emissive;

        float fog = 1.0 - exp(-hit.x * 0.075);
        col = mix(lit, ambCol, clamp(fog, 0.0, 1.0));
    }

    col = acesTone(col * 1.15);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.24 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

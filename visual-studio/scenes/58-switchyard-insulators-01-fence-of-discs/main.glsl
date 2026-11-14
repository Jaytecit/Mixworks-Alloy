#version 330 core
// Switchyard Insulators / Fence of Discs
#define VARIANT 0

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_Level;
uniform float syn_BassLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_HighHits;
uniform float syn_MidHighHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float coronaStrength;
uniform float arcReach;

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

float sampleSplitStrength(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return tex.a * (0.35 + 0.65 * mediaLuma(tex.rgb));
    }
    // Procedural switchyard split-node lattice fallback
    float grid = step(0.35, sin(cuv.x * 5.0 * PI) * cos(cuv.y * 3.0 * PI));
    float center = 1.0 - smoothstep(0.08, 0.45, length(cuv - 0.5));
    return clamp(max(grid * 0.7, center), 0.0, 1.0);
}

vec3 sampleSplitColor(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        return texture(syn_Media, cuv).rgb;
    }
    return mix(vec3(0.35, 0.75, 1.0), vec3(0.75, 0.52, 1.0), cuv.x);
}

// Ribbed ceramic disc insulator column along local Y axis
float sdInsulatorStack(vec3 q, float halfH, float baseR) {
    float shed = 0.085 * cos(q.y * 22.0);
    float dCol = length(q.xz) - (baseR + shed);
    return max(dCol, abs(q.y) - halfH);
}

// High-voltage arc filament distance with sprite-brightness split forks
float sdElectricArc(vec3 p, vec3 a, vec3 b, vec2 arcUV, float clk, float highs, out float splitGlow) {
    vec3 centre = (a + b) * 0.5;
    a = mix(centre, a, arcReach);
    b = mix(centre, b, arcReach);
    vec3 pa = p - a;
    vec3 ba = b - a;
    float denom = max(dot(ba, ba), 1e-5);
    float h = clamp(dot(pa, ba) / denom, 0.0, 1.0);
    float jitter = (noise2(vec2(h * 14.0 + clk * 8.0, TIME * 12.0)) - 0.5) * (0.14 + 0.16 * highs);
    vec3 closest = a + ba * h + vec3(0.0, jitter, jitter * 0.5) * sin(h * PI);
    float dMain = length(p - closest) - 0.025;

    float bright = sampleSplitStrength(arcUV) * imprint;
    splitGlow = bright;
    // Where sprite brightness is high, arc splits into a secondary downward corona branch
    if (bright > 0.25) {
        vec3 forkEnd = mix(a, b, 0.55) + vec3(0.28 * sin(h * 12.0), -0.75 * bright, 0.22);
        float dFork = sdCapsule(p, closest, forkEnd, 0.018);
        return min(dMain, dFork);
    }
    return dMain;
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float highs = clamp((syn_HighLevel * response) + 0.6 * (syn_HighHits * response), 0.0, 1.5);
    float clk = motion;
    float splitG = 0.0;

    vec2 res = vec2(p.y + 2.2, 0.0); // gravel switchyard pad

#if VARIANT == 0
    // 01: Fence of Discs — parallel rows of plinth-mounted ceramic stacks under busbars
    float rowX = (p.x < 0.0) ? -1.85 : 1.85;
    float cz = mod(p.z + 1.4, 2.8) - 1.4;
    float zId = floor((p.z + 1.4) / 2.8);
    float dPlinth = sdBox(vec3(p.x - rowX, p.y + 1.4, cz), vec3(0.32, 0.85, 0.32));
    float dStack = sdInsulatorStack(vec3(p.x - rowX, p.y - 0.25, cz), 0.85, 0.22);
    float dBus = sdCappedCylinder(vec3(p.x - rowX, p.y - 1.25, 0.0).xzy, 25.0, 0.065);
    res = minMat(res, vec2(dPlinth, 1.0));
    res = minMat(res, vec2(dStack, 2.0));
    res = minMat(res, vec2(dBus, 3.0));

    // Arc jumping between left & right busbars modulated by sprite brightness
    vec2 arcUV = vec2(fract(zId * 0.14 + 0.5), clamp(p.x / 4.0 + 0.5, 0.0, 1.0));
    float dArc = sdElectricArc(vec3(p.x, p.y, cz), vec3(-1.85, 1.25, 0.0), vec3(1.85, 1.25, 0.0),
                               arcUV, clk, highs, splitG);
    if (dArc < res.x) { res = vec2(dArc, 4.0); info = vec4(arcUV, splitG, 1.0); }

#elif VARIANT == 1
    // 02: Tower Bank — multi-tier substation gantry tower with clustered insulator banks
    float dLegs = sdBox(vec3(abs(p.x) - 3.2, p.y - 1.0, p.z - 2.4), vec3(0.24, 3.4, 0.24));
    float tierY = (p.y < 1.3) ? 0.2 : 2.6;
    float dBeam = sdBox(p - vec3(0.0, tierY, 2.4), vec3(3.4, 0.18, 0.22));
    res = minMat(res, vec2(min(dLegs, dBeam), 1.0));

    float colX = clamp(round(p.x / 1.4), -1.0, 1.0) * 1.4;
    float dStack = sdInsulatorStack(p - vec3(colX, tierY + 0.82, 2.4), 0.65, 0.20);
    float dBus = sdCappedCylinder((p - vec3(colX, tierY + 1.55, 0.0)).xzy, 20.0, 0.06);
    res = minMat(res, vec2(dStack, 2.0));
    res = minMat(res, vec2(dBus, 3.0));

    vec2 arcUV = vec2(clamp(p.x / 4.2 + 0.5, 0.0, 1.0), clamp(p.y / 4.5 + 0.2, 0.0, 1.0));
    float dArc = sdElectricArc(p, vec3(-1.4, 1.75, 2.4), vec3(1.4, 3.45, 1.8),
                               arcUV, clk, highs, splitG);
    if (dArc < res.x) { res = vec2(dArc, 4.0); info = vec4(arcUV, splitG, 1.0); }

#elif VARIANT == 2
    // 03: Crossbar Bay — orthogonal overhead busbar grid & vertical drop insulator horns
    float gx = mod(p.x + 1.5, 3.0) - 1.5;
    float gz = mod(p.z + 1.5, 3.0) - 1.5;
    float dBarX = sdCappedCylinder(vec3(0.0, p.y - 1.8, gz).yxz, 20.0, 0.065);
    float dBarZ = sdCappedCylinder(vec3(gx, p.y - 2.6, 0.0).xzy, 20.0, 0.065);
    res = minMat(res, vec2(min(dBarX, dBarZ), 3.0));

    float dDrop = sdInsulatorStack(vec3(gx, p.y - 0.85, gz), 0.78, 0.21);
    float dPost = sdBox(vec3(gx, p.y + 1.1, gz), vec3(0.18, 1.15, 0.18));
    res = minMat(res, vec2(dDrop, 2.0));
    res = minMat(res, vec2(dPost, 1.0));

    vec2 arcUV = vec2(fract(floor((p.x + 1.5) / 3.0) * 0.23 + 0.5), fract(floor((p.z + 1.5) / 3.0) * 0.23 + 0.5));
    float dArc = sdElectricArc(vec3(gx, p.y, gz), vec3(0.0, 1.8, 0.0), vec3(0.0, 2.6, 0.0),
                               arcUV, clk, highs, splitG);
    if (dArc < res.x) { res = vec2(dArc, 4.0); info = vec4(arcUV, splitG, 1.0); }

#else
    // 04: Suspended Array — high portal frame suspending swaying glazed insulator strings
    float dPortal = min(sdBox(p - vec3(0.0, 3.6, 1.6), vec3(4.4, 0.22, 0.26)),
                        sdBox(vec3(abs(p.x) - 4.1, p.y - 0.7, p.z - 1.6), vec3(0.24, 3.1, 0.26)));
    res = minMat(res, vec2(dPortal, 1.0));

    float sx = clamp(round(p.x / 1.55), -2.0, 2.0) * 1.55;
    float sway = 0.08 * sin(clk * 2.1 + sx);
    vec3 qStr = p - vec3(sx + sway, 2.1, 1.6);
    float dString = sdInsulatorStack(qStr, 1.25, 0.22);
    float dRing = sdTorus(qStr - vec3(0.0, -1.28, 0.0), vec2(0.32, 0.045));
    res = minMat(res, vec2(dString, 2.0));
    res = minMat(res, vec2(dRing, 3.0));

    vec2 arcUV = vec2(clamp(p.x / 6.2 + 0.5, 0.0, 1.0), clamp((p.y + 0.5) / 3.5, 0.0, 1.0));
    float dArc = sdElectricArc(p, vec3(-3.1, 0.82, 1.6), vec3(3.1, 0.82, 1.6),
                               arcUV, clk, highs, splitG);
    if (dArc < res.x) { res = vec2(dArc, 4.0); info = vec4(arcUV, splitG, 1.0); }
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
    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_HighHits * response), 0.0, 1.5);
    float trans = clamp((syn_Hits * response) + 0.7 * (syn_MidHighHits * response) + 0.7 * (syn_HighHits * response), 0.0, 1.6);

#if VARIANT == 0
    vec3 ro = vec3(0.0, 0.55, -6.2);
    vec3 ta = vec3(0.0, 0.45, 2.2);
#elif VARIANT == 1
    vec3 ro = vec3(-2.4, 0.85, -5.8);
    vec3 ta = vec3(0.2, 1.85, 2.4);
#elif VARIANT == 2
    vec3 ro = vec3(1.3, 0.15, -5.5);
    vec3 ta = vec3(0.0, 1.65, 1.8);
#else
    vec3 ro = vec3(0.0, 0.85, -5.8);
    vec3 ta = vec3(0.0, 1.55, 1.6);
#endif

    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 skyCol = mix(vec3(0.04, 0.05, 0.09), vec3(0.11, 0.14, 0.24), clamp(rd.y * 1.4 + 0.35, 0.0, 1.0));
    skyCol += vec3(0.18, 0.26, 0.48) * fbm2(rd.xy * 4.5 + vec2(motion * 0.05, 0.0)) * 0.35;

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
        t += res.x * 0.68;
        if (t > maxT) break;
    }

    vec3 col = skyCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(0.4, 0.7, -0.5));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 48.0);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            baseCol = vec3(0.09, 0.10, 0.11) * (0.7 + 0.5 * noise2(pos.xz * 5.0));
        } else if (hit.y < 1.5) {
            // Galvanized steel & concrete plinths
            baseCol = vec3(0.24, 0.26, 0.29);
        } else if (hit.y < 2.5) {
            // Glazed porcelain insulator discs with bass corona glow
            baseCol = vec3(0.32, 0.42, 0.48);
            spec *= (1.6 + highs);
            emissive = vec3(0.22, 0.62, 0.98) * (0.08 + 0.32 * bass) * coronaStrength;
        } else if (hit.y < 3.5) {
            // Copper/aluminum busbar conductors
            baseCol = vec3(0.66, 0.42, 0.26);
            float travelArc = pow(0.5 + 0.5 * sin(pos.z * 2.2 - motion * 9.0 + TIME * 4.0), 6.0);
            emissive = vec3(0.45, 0.85, 1.0) * travelArc * (0.2 + 0.9 * trans);
        } else {
            // High-voltage plasma arcs splitting on sprite brightness
            vec3 arcCol = sampleSplitColor(hitInfo.xy);
            baseCol = vec3(0.9);
            emissive = mix(vec3(0.45, 0.85, 1.0), arcCol, 0.55 * imprint)
                     * (1.2 + 1.8 * trans + 0.8 * (syn_Level * response)) * (0.6 + 0.8 * hitInfo.z);
        }

        // Bright split arcs cast moving coloured bands onto the hardware.
        vec2 projectionUV = fract(vec2(pos.x * 0.28 + pos.z * 0.12, pos.y * 0.36 + 0.42));
        float coronaMask = sampleSplitStrength(projectionUV) * imprint;
        float pulseFront = pow(0.5 + 0.5 * sin(pos.z * 2.9 + pos.y - motion * 8.0), 3.0);
        emissive += sampleSplitColor(projectionUV) * coronaMask * pulseFront * (0.13 + 0.38 * trans) * coronaStrength;

        vec3 lit = baseCol * (vec3(0.95, 0.95, 1.05) * diff + vec3(0.14, 0.18, 0.26));
        lit += vec3(0.85, 0.95, 1.0) * spec * (0.4 + 0.6 * highs);
        lit += emissive;
        col = mix(lit, skyCol, clamp(1.0 - exp(-hit.x * 0.042), 0.0, 1.0));
    }

    col = acesTone(col * 1.15);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

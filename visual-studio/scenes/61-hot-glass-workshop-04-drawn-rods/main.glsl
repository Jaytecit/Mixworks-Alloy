#version 330 core
// Hot Glass Workshop / Drawn Rods
#define VARIANT 3

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_HighHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float rodDrawLength;
uniform float hotGlassGlow;

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

vec4 sampleGlassFrit(vec2 uv) {
    vec2 cuv = fract(uv);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, clamp(cuv, 0.0, 1.0));
        return vec4(tex.rgb, tex.a);
    }
    // Spiral cobalt & gold cane frit fallback
    float cane = smoothstep(0.35, 0.65, sin((cuv.x * 4.0 + cuv.y * 2.0) * TAU));
    vec3 col = mix(vec3(0.15, 0.55, 0.98), vec3(1.0, 0.68, 0.18), step(0.5, fract(cuv.x * 2.0)));
    return vec4(col, cane * 0.85);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float swell = 1.0 + 0.30 * bass;

    // Studio steel marver bench & furnace glory-hole arch in background
    float dBench = p.y + 1.45;
    float dFurnace = max(abs(p.z - 4.2) - 0.55, -(length(p.xy - vec2(0.0, 0.2)) - 1.35));
    vec2 res = vec2(dBench, 0.0);
    res = minMat(res, vec2(dFurnace, 2.0));

#if VARIANT == 0
    // 01: Blown Vessel — rotating horizontal blowpipe swelling a bulbous molten gather
    vec3 q = p - vec3(0.1, 0.12 - 0.08 * bass, 0.0);
    q.yz *= rot2(clk * 1.4);
    float dPipe = sdCappedCylinder((q - vec3(-2.4, 0.0, 0.0)).yxz, 1.85, 0.09);
    res = minMat(res, vec2(dPipe, 1.0));

    // Bulbous vessel stretched along +X from pipe tip
    vec3 gq = q - vec3(0.35, 0.0, 0.0);
    float u = clamp((gq.x + 0.9) / 2.2, 0.0, 1.0);
    float bulbR = (0.32 + 0.58 * sin(u * PI)) * swell;
    float dGlass = length(gq.yz) - bulbR;
    dGlass = max(dGlass, abs(gq.x - 0.2) - 1.12);
    if (dGlass < res.x) {
        res = vec2(dGlass * 0.78, 3.0);
        float ang = atan(gq.z, gq.y) / TAU + 0.5;
        info = vec4(ang, u, length(gq.yz) / max(bulbR, 0.1), 0.0);
    }

#elif VARIANT == 1
    // 02: Hanging Drop — vertical gather stretching downward under gravity from overhead pipe
    vec3 q = p - vec3(0.0, 0.35, 0.2);
    q.xz *= rot2(clk * 1.3);
    float dPipe = sdCappedCylinder(q - vec3(0.0, 2.2, 0.0), 1.3, 0.09);
    res = minMat(res, vec2(dPipe, 1.0));

    float stretch = 1.0 + 0.28 * sin(clk * 0.9) + 0.22 * bass;
    float v = clamp((1.1 - q.y) / (2.2 * stretch), 0.0, 1.0);
    float dropR = (0.18 + 0.54 * pow(sin(clamp(v, 0.0, 1.0) * PI * 0.88), 0.7)) * (0.9 + 0.25 * bass);
    float dGlass = length(q.xz) - dropR;
    dGlass = max(dGlass, abs(q.y - (0.1 - 0.35 * stretch)) - 1.05 * stretch);
    if (dGlass < res.x) {
        res = vec2(dGlass * 0.75, 3.0);
        info = vec4(atan(q.z, q.x) / TAU + 0.5, v, dropR, 0.0);
    }

#elif VARIANT == 2
    // 03: Twisted Goblet — fluted cup bowl, twisted stem & flared foot spinning on punty
    vec3 q = p - vec3(0.0, 0.15, 0.0);
    q.yz *= rot2(clk * 1.35);
    float dPipe = sdCappedCylinder((q - vec3(-2.3, 0.0, 0.0)).yxz, 1.65, 0.085);
    res = minMat(res, vec2(dPipe, 1.0));

    float ang = atan(q.z, q.y);
    float flute = 0.045 * cos(ang * 8.0 + q.x * 5.0);
    float rCup = (0.58 + 0.18 * bass) * smoothstep(-0.7, 0.65, q.x) + flute;
    float dCup = max(length(q.yz) - rCup, abs(q.x - 0.65) - 0.62);
    float dStem = max(length(q.yz) - (0.14 + 0.03 * sin(q.x * 12.0 + ang * 3.0)), abs(q.x + 0.18) - 0.38);
    float dFoot = max(length(q.yz) - (0.55 + 0.10 * bass), abs(q.x + 0.62) - 0.08);
    float dGlass = smin(min(dCup, dFoot), dStem, 0.08);
    if (dGlass < res.x) {
        res = vec2(dGlass * 0.76, 3.0);
        info = vec4(ang / TAU + 0.5, clamp((q.x + 0.7) / 2.0, 0.0, 1.0), 0.5, 0.0);
    }

#else
    // 04: Drawn Rods — twin opposing irons stretching glowing twisted glass canes horizontally
    float pull = 0.35 + 0.32 * (0.5 + 0.5 * sin(clk * 0.75)) + 0.18 * bass;
    vec3 q = p - vec3(0.0, 0.15, 0.0);
    q.yz *= rot2(clk * 1.5);
    float dPipes = sdCappedCylinder(vec3(abs(q.x) - (2.35 + pull * 0.4) * rodDrawLength, q.y, q.z).yxz, 1.2, 0.085);
    res = minMat(res, vec2(dPipes, 1.0));

    float neck = mix(0.42, 0.15, smoothstep(0.0, 1.1, 1.15 - abs(q.x)));
    float twist = 0.06 * cos(atan(q.z, q.y) * 5.0 + q.x * 6.0);
    float dRods = max(length(q.yz) - (neck * swell + twist), abs(q.x) - (1.25 + pull * 0.45) * rodDrawLength);
    if (dRods < res.x) {
        res = vec2(dRods * 0.78, 3.0);
        info = vec4(atan(q.z, q.y) / TAU + 0.5, clamp(q.x / 2.8 + 0.5, 0.0, 1.0), 0.5, 0.0);
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
    float highs = clamp((syn_HighLevel * response) + 0.65 * (syn_HighHits * response), 0.0, 1.5);

#if VARIANT == 1
    vec3 ro = vec3(0.25, 0.35, -4.6);
    vec3 ta = vec3(0.0, -0.05, 0.2);
#else
    vec3 ro = vec3(-0.35, 0.65, -4.4);
    vec3 ta = vec3(0.15, 0.08, 0.1);
#endif

    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    // Warm dark brick workshop with glowing furnace hearth behind
    vec3 bgCol = mix(vec3(0.05, 0.03, 0.03), vec3(0.18, 0.07, 0.04), clamp(1.0 - length(rd.xy) * 1.1, 0.0, 1.0));
    float furnaceBloom = exp(-length(rd.xy - vec2(0.0, 0.05)) * 3.8);
    bgCol += vec3(1.0, 0.42, 0.10) * furnaceBloom * (0.45 + 0.35 * mids);

    float t = 0.0;
    float maxT = 20.0;
    vec2 hit = vec2(maxT, -1.0);
    vec4 hitInfo = vec4(0.0);

    for (int i = 0; i < 68; i++) {
        vec3 pos = ro + rd * t;
        vec4 info;
        vec2 res = mapScene(pos, info);
        if (res.x < 0.002 * (1.0 + 0.25 * t)) {
            hit = vec2(t, res.y);
            hitInfo = info;
            break;
        }
        t += res.x * 0.70;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(-0.45, 0.82, -0.35));
        float diff = max(dot(nor, keyDir), 0.0);
        float specPow = mix(48.0, 120.0, clamp(highs, 0.0, 1.0));
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), specPow);
        float fres = pow(1.0 - max(dot(nor, viewDir), 0.0), 3.5);

        vec3 baseCol = vec3(0.15);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            baseCol = vec3(0.11, 0.10, 0.11) * (0.8 + 0.3 * noise2(pos.xz * 4.0));
        } else if (hit.y < 1.5) {
            // Steel blowpipe
            baseCol = vec3(0.42, 0.44, 0.48);
        } else if (hit.y < 2.5) {
            // Firebrick glory-hole wall
            baseCol = vec3(0.22, 0.12, 0.09);
            emissive = vec3(0.85, 0.28, 0.05) * 0.25;
        } else {
            // Incandescent molten glass picking up color from sprite frit
            vec4 frit = sampleGlassFrit(hitInfo.xy);
            float coreHeat = clamp(0.65 + 0.35 * sin(TIME * 1.5 + hitInfo.y * 4.0) + 0.3 * bass, 0.0, 1.2);
            vec3 moltenGlow = mix(vec3(0.95, 0.24, 0.04), vec3(1.0, 0.72, 0.22), coreHeat * 0.7);
            vec3 glassTint = mix(moltenGlow, frit.rgb, clamp(frit.w * imprint, 0.0, 1.0));
            baseCol = glassTint * 0.55;
            emissive = glassTint * (0.55 + 0.45 * mids + 0.35 * (syn_Hits * response)) * hotGlassGlow + vec3(1.0, 0.92, 0.80) * fres * (0.35 + 0.75 * highs);
            spec *= (1.6 + 1.4 * highs);
        }

        vec3 lit = baseCol * (vec3(1.1, 0.92, 0.78) * diff + vec3(0.15, 0.12, 0.14));
        lit += vec3(1.0, 0.95, 0.90) * spec * (0.45 + 0.85 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.045), 0.0, 1.0));
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

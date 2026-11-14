#version 330 core
// Braided Loom / Tubular Sleeve
#define VARIANT 1

out vec4 fragColor;

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_HighHits;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_MidHits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;
uniform float motion;
uniform float imprint;
uniform float response;
uniform float braidPitch;

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

vec4 sampleWeaveArt(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return vec4(tex.rgb, tex.a * (0.25 + 0.75 * mediaLuma(tex.rgb)));
    }
    // Herringbone twill brocade fallback
    float chev = step(0.48, fract((cuv.x * 4.0 + abs(cuv.y - 0.5) * 4.0)));
    vec3 col = mix(vec3(0.84, 0.34, 0.18), vec3(0.18, 0.52, 0.78), chev);
    return vec4(col, chev * 0.85);
}

// Evaluates interlaced warp & weft cylindrical cords in local (u, v, height) space
vec2 sdWeavePlane(vec3 q, float liftState, float bassTight, float clk, out vec4 wInfo) {
    wInfo = vec4(0.0);
    float pitch = 0.42 * braidPitch;
    float cordR = 0.105;

    float uId = floor(q.x / pitch + 0.5);
    float vId = floor(q.z / pitch + 0.5);
    float ux = q.x - uId * pitch;
    float vz = q.z - vId * pitch;

    vec2 wuv = clamp(vec2(uId / 12.0 + 0.5, vId / 12.0 + 0.5), 0.0, 1.0);
    vec4 art = sampleWeaveArt(wuv);

    // Artwork flips weave parity & raises selected brocade threads; percussion lifts alternate sets
    float baseParity = mod(uId + vId, 2.0) * 2.0 - 1.0;
    float artParity = mix(1.0, -1.0, step(0.42, art.w) * imprint);
    float shedLift = (mod(uId, 2.0) < 0.5 ? 1.0 : -1.0) * (0.16 * liftState);
    float amp = (0.13 - 0.03 * bassTight) * baseParity * artParity;

    float yWarp = amp * cos(q.z * PI / pitch) + shedLift;
    float yWeft = -amp * cos(q.x * PI / pitch);

    float dWarp = length(vec2(ux, q.y - yWarp)) - cordR;
    float dWeft = length(vec2(vz, q.y - yWeft)) - cordR;

    if (dWarp < dWeft) {
        wInfo = vec4(wuv, art.w, 1.0);
        return vec2(dWarp, 2.0);
    }
    wInfo = vec4(wuv, art.w, 0.0);
    return vec2(dWeft, 3.0);
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.4 * (syn_BassHits * response), 0.0, 1.5);
    float hits = clamp((syn_Hits * response) + 0.7 * (syn_MidHits * response) + 0.5 * (syn_BassHits * response), 0.0, 1.5);
    float clk = motion;
    float liftState = sin(clk * 2.2) * (0.5 + 0.8 * hits);

    vec2 res = vec2(p.y + 2.6, 0.0);
    vec4 wInfo = vec4(0.0);

#if VARIANT == 0
    // 01: Shuttle Weave — horizontal flatbed loom with gliding brass shuttle
    vec3 q = p - vec3(0.0, 0.0, -clk * 0.8);
    vec2 weave = sdWeavePlane(p - vec3(0.0, 0.0, 0.5), liftState, bass, clk, wInfo);
    if (weave.x < res.x && abs(p.x) < 3.2 && abs(p.z - 0.5) < 3.2) {
        res = weave;
        info = wInfo;
    }
    // Steel heddle frame bars & sliding shuttle
    float dFrame = sdBox(vec3(abs(p.x) - 3.3, p.y, p.z - 0.5), vec3(0.16, 0.25, 3.4));
    float shutX = 2.4 * sin(clk * 2.1);
    float dShuttle = sdRoundBox(p - vec3(shutX, 0.18, -0.4), vec3(0.45, 0.09, 0.14), 0.04);
    res = minMat(res, vec2(min(dFrame, dShuttle), 1.0));

#elif VARIANT == 1
    // 02: Tubular Sleeve — helical braided sleeve wrapping a brushed steel core mandrel
    vec3 q = p - vec3(0.0, 0.1, 0.4);
    q.yz *= rot2(0.32);
    float r = length(q.xy);
    float ang = atan(q.y, q.x);
    float rCore = 1.15;
    // Map cylindrical tube coordinates onto weave plane
    vec3 tubeQ = vec3(ang * rCore, r - (rCore + 0.14), q.z - clk * 0.9);
    vec2 weave = sdWeavePlane(tubeQ, liftState, bass, clk, wInfo);
    float dMandrel = max(r - rCore, abs(q.z) - 3.6);
    res = minMat(res, vec2(dMandrel, 1.0));
    if (weave.x < res.x && abs(q.z) < 3.2) {
        res = weave;
        info = wInfo;
    }

#elif VARIANT == 2
    // 03: Diagonal Braid — 45-degree bias-angled tension braid frame
    vec3 q = p - vec3(0.0, 0.15, 0.5);
    q.yz *= rot2(-0.52);
    q.xz *= rot2(0.7854);
    vec2 weave = sdWeavePlane(q, liftState, bass, clk, wInfo);
    if (weave.x < res.x && abs(q.x) < 2.9 && abs(q.z) < 2.9) {
        res = weave;
        info = wInfo;
    }
    res = minMat(res, vec2(sdBox(vec3(abs(q.x) - 3.0, q.y, q.z), vec3(0.14, 0.2, 3.1)), 1.0));

#else
    // 04: Woven Basket — flared curved basket bowl woven around radial vertical stakes
    vec3 q = p - vec3(0.0, 0.15, 0.4);
    q.xz *= rot2(clk * 0.35);
    float r = length(q.xz);
    float ang = atan(q.z, q.x);
    float bowlY = 0.26 * r * r - 0.65;
    vec3 bq = vec3(ang * max(r, 0.4), q.y - bowlY, r);
    vec2 weave = sdWeavePlane(bq, liftState, bass, clk, wInfo);
    if (weave.x < res.x && r > 0.35 && r < 2.65) {
        res = weave;
        info = wInfo;
    }
    res = minMat(res, vec2(sdCappedCylinder(q - vec3(0.0, -0.65, 0.0), 0.15, 0.45), 1.0));
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

    vec3 ro = vec3(0.35, 2.35, -4.4);
    vec3 ta = vec3(0.0, 0.05, 0.45);
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.5 * cw);

    vec3 bgCol = mix(vec3(0.06, 0.07, 0.09), vec3(0.16, 0.18, 0.22), clamp(rd.y + 0.4, 0.0, 1.0));

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
        t += res.x * 0.62;
        if (t > maxT) break;
    }

    vec3 col = bgCol;
    if (hit.y >= 0.0) {
        vec3 pos = ro + rd * hit.x;
        vec3 nor = calcNormal(pos);
        vec3 viewDir = -rd;
        vec3 keyDir = normalize(vec3(0.45, 0.85, -0.35));
        float diff = max(dot(nor, keyDir), 0.0);
        float spec = pow(max(dot(nor, normalize(keyDir + viewDir)), 0.0), 32.0);
        float fres = pow(1.0 - max(dot(nor, viewDir), 0.0), 3.0);

        vec3 baseCol = vec3(0.2);
        vec3 emissive = vec3(0.0);

        if (hit.y < 0.5) {
            baseCol = vec3(0.09, 0.10, 0.12);
        } else if (hit.y < 1.5) {
            // Brushed steel loom heddle bars & shuttle
            baseCol = vec3(0.42, 0.44, 0.48);
        } else {
            // Thick twisted coloured cords picking up sprite weave & colour
            vec4 art = sampleWeaveArt(hitInfo.xy);
            float twistTex = 0.82 + 0.18 * sin((pos.x + pos.z) * 42.0 + TIME * 0.1);
            vec3 defaultThread = (hit.y < 2.5) ? vec3(0.82, 0.38, 0.18) : vec3(0.20, 0.46, 0.68);
            baseCol = mix(defaultThread, art.rgb, clamp(art.w * imprint, 0.0, 1.0)) * twistTex;
            emissive = baseCol * fres * (0.14 + 0.45 * highs + 0.2 * mids);
        }

        vec3 lit = baseCol * (vec3(1.12, 1.04, 0.94) * diff + vec3(0.18, 0.20, 0.24));
        lit += vec3(1.0, 0.95, 0.88) * spec * (0.25 + 0.65 * highs);
        lit += emissive;
        col = mix(lit, bgCol, clamp(1.0 - exp(-hit.x * 0.045), 0.0, 1.0));
    }

    col = acesTone(col * 1.12);
    col = pow(col, vec3(0.4545));
    col *= 1.0 - 0.22 * dot(uv - 0.5, uv - 0.5);
    fragColor = vec4(col, 1.0);
}

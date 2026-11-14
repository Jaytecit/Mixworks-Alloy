#version 330 core
// Spore Pressure Pods / Radial Release
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
uniform float plumeSpread;
uniform float releaseIntensity;

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


float samplePodCluster(vec2 uv) {
    vec2 cuv = clamp(uv, 0.0, 1.0);
    if (syn_MediaType > 0.5) {
        vec4 tex = texture(syn_Media, cuv);
        return (tex.a < 0.96) ? tex.a : mediaLuma(tex.rgb);
    }
    return 1.0 - smoothstep(0.15, 0.46, length(cuv - 0.5));
}

vec2 mapScene(vec3 p, out vec4 info) {
    info = vec4(0.0);
    float bass = clamp((syn_BassLevel * response) + 0.45 * (syn_BassHits * response), 0.0, 1.5);
    float hits = clamp((syn_Hits * response) + 0.65 * (syn_HighHits * response), 0.0, 1.5);
    float dBed = p.y + 1.35;
    vec2 res = vec2(dBed, 0.0);

    vec2 cell = (VARIANT == 3) ? vec2(0.0) : floor(p.xz / 1.45 + 0.5);
    vec2 puv = clamp(cell / 4.0 + 0.5, 0.0, 1.0);
    float clust = samplePodCluster(puv) * imprint;

    if (clust > 0.18 || VARIANT == 3) {
        float cy = (VARIANT == 1) ? 0.65 : -0.45;
        vec3 q = (VARIANT == 3) ? (p - vec3(0.0, -0.15, 0.2)) : (p - vec3(cell.x * 1.45, cy, cell.y * 1.45));
        float rPod = ((VARIANT == 3) ? 1.15 : 0.48) * (0.85 + 0.28 * bass);
        float rib = 0.06 * cos(atan(q.z, q.x) * 6.0);
        float split = 0.08 * hits * smoothstep(0.1, 0.8, q.y + 0.2);
        float dPod = length(vec3(q.x, q.y * 0.78, q.z)) - (rPod + rib - split);
        vec2 skinUV = vec2(atan(q.x, -q.z) / PI + 0.5, q.y / (rPod * 2.35) + 0.5);
        if (dPod < res.x) {
            res = vec2(dPod * 0.75, 1.0);
            info = vec4((VARIANT == 3) ? skinUV : puv, hits, 0.0);
        }
    }
    return res;
}

void getCamera(out vec3 ro, out vec3 ta) {
    ro = vec3(0.3, 1.15, (VARIANT == 3) ? -5.5 : -4.6);
    ta = vec3(0.0, 0.05, 0.4);
}

vec3 getSky(vec3 rd, out vec3 sunDir) {
    sunDir = normalize(vec3(0.35, 0.85, 0.40));
    return mix(vec3(0.04, 0.07, 0.05), vec3(0.12, 0.22, 0.16), clamp(rd.y + 0.4, 0.0, 1.0));
}

vec3 shadeSurface(vec3 pos, vec3 nor, vec3 rd, float dist, float matId, vec4 info, vec3 sunDir, vec3 skyCol, vec2 p) {
    float diff = max(dot(nor, sunDir), 0.0);
    if (matId < 0.5) return vec3(0.07, 0.10, 0.07) * (diff + 0.22);
    float fres = pow(1.0 - max(dot(nor, -rd), 0.0), 2.5);
    vec3 podCol = mix(vec3(0.35, 0.56, 0.22), vec3(0.95, 0.62, 0.18), fres);
#if VARIANT == 3
    if (syn_MediaType > 0.5) {
        vec4 art = texture(syn_Media, clamp(info.xy, 0.0, 1.0));
        float veins = 0.72 + 0.28 * cos(info.xy.x * 45.0 + info.xy.y * 16.0);
        podCol = mix(podCol, art.rgb * veins, art.a * imprint * 0.85);
    }
#endif
    return podCol * (diff + 0.30) + vec3(1.0, 0.85, 0.35) * fres * (0.35 + 0.75 * (syn_Hits * response) + 0.3 * (syn_HighLevel * response));
}

vec3 postMissGlow(vec3 rd, vec2 p) {
    // Backlit ejected spore plume clouds on transients
    float plume = 0.0;
    for (int i = 0; i < 10; i++) {
        float fi = float(i);
        float life = fract(motion * 0.85 + TIME * 0.4 + fi * 0.1);
        vec2 sp = vec2((0.35 * plumeSpread) * sin(fi * 4.2 + motion), -0.15 + life * 0.85);
        plume += exp(-length(p - sp) * 14.0) * (1.0 - life);
    }
    return vec3(0.98, 0.86, 0.42) * plume * releaseIntensity * (0.25 + 0.9 * clamp((syn_Hits * response) + (syn_HighHits * response), 0.0, 1.5) + 0.3 * (syn_MidLevel * response));
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

    vec3 ro, ta;
    getCamera(ro, ta);
    vec3 cw = normalize(ta - ro);
    vec3 cu = normalize(cross(cw, vec3(0.0, 1.0, 0.0)));
    vec3 cv = cross(cu, cw);
    vec3 rd = normalize(p.x * cu + p.y * cv + 1.52 * cw);

    vec3 sunDir;
    vec3 skyCol = getSky(rd, sunDir);

    float t = 0.0;
    float maxT = 30.0;
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

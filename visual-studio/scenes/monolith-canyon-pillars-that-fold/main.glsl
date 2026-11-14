// Family 08: Monolith Canyon — Pillars That Fold
#version 330 core

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_Level;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_OnBeat;
uniform float syn_BPMSin;
uniform sampler2D syn_Media;
uniform float syn_MediaType;
uniform float motion;
uniform float response;
uniform float scenePillarHeight;
uniform float sceneDistanceMist;

out vec4 fragColor;

const int VARIANT = 3;

// Keep both ascending and descending fades defined in desktop GLSL.
float sceneRamp(float start, float end, float value) {
    if (start < end) return smoothstep(start, end, value);
    if (start > end) return 1.0 - smoothstep(end, start, value);
    return step(start, value);
}
vec2 sceneRamp(float start, float end, vec2 value) {
    return vec2(sceneRamp(start, end, value.x), sceneRamp(start, end, value.y));
}

mat2 rot2(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

vec3 pal3(float t, vec3 a, vec3 b, vec3 d) {
    return clamp(a + b * cos(6.2831853 * (t + d)), 0.0, 1.0);
}
vec3 pal(float t) {
    return pal3(t + 0.14 * (syn_MidLevel * response), vec3(0.48, 0.44, 0.54), vec3(0.50, 0.48, 0.45), vec3(0.55, 0.08, 0.85));
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
vec2 hash22(vec2 p) {
    vec3 a = fract(p.xyx * vec3(123.34, 234.34, 345.65));
    a += dot(a, a + 34.45);
    return fract(vec2(a.x * a.y, a.y * a.z));
}

float sdBox2(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdBox3(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = (clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0)) * scenePillarHeight;
    return length(pa - ba * h);
}

vec4 sampleMedia(vec2 uv) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = sceneRamp(0.0, 0.008, uv) * sceneRamp(1.0, 0.992, uv);
    vec4 c = texture(syn_Media, clamp(uv, 0.0, 1.0));
    c.a *= m.x * m.y;
    return c;
}
vec4 sampleMediaTile(vec2 uv) {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    if (syn_MediaType <= 0.0) return vec4(0.0);
    return textureGrad(syn_Media, fract(uv), dx, dy);
}
vec4 sampleMediaChroma(vec2 uv, float amt) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = sceneRamp(0.0, 0.008, uv) * sceneRamp(1.0, 0.992, uv);
    vec4 c = texture(syn_Media, clamp(uv, 0.0, 1.0));
    float r = texture(syn_Media, clamp(uv + vec2(amt, 0.0), 0.0, 1.0)).r;
    float b = texture(syn_Media, clamp(uv - vec2(amt, 0.0), 0.0, 1.0)).b;
    return vec4(r, c.g, b, c.a * m.x * m.y);
}

vec2 mapCanyon(vec3 q, float t, out vec2 faceUV) {
    float floorD = q.y + 1.4;
    vec3 p = q;
    float id = 0.0;
    if (VARIANT == 0) {
        p.x = abs(p.x) - 1.75;
        id = floor(p.z * 0.5);
        p.z = mod(p.z, 2.0) - 1.0;
    } else if (VARIANT == 1) {
        vec2 cell = floor(p.xz / 2.6);
        id = hash21(cell) * 10.0;
        p.xz = mod(p.xz, 2.6) - 1.3;
    } else if (VARIANT == 2) {
        float ang = p.z * 0.38;
        p.xy = rot2(ang) * p.xy;
        p.x = abs(p.x) - 1.65;
        id = floor(p.z * 0.5);
        p.z = mod(p.z, 2.0) - 1.0;
    } else {
        float fold = sin(p.z * 0.28 + t * 1.2) * (0.35 + 0.35 * (syn_OnBeat * response));
        p.xy = rot2(fold * sign(p.x + 1e-4)) * p.xy;
        p.x = abs(p.x) - 1.55;
        id = floor(p.z * 0.5);
        p.z = mod(p.z, 2.0) - 1.0;
    }

    float h = (0.6 + 1.2 * hash21(vec2(id, 3.1)) + (1.4 * (syn_BassLevel * response) + 0.8 * (syn_BassHits * response)) * (0.4 + 0.6 * hash21(vec2(id, 9.7)))) * scenePillarHeight;
    float boxD = sdBox3(p - vec3(0.0, h - 1.4, 0.0), vec3(0.48, h, 0.48));
    faceUV = abs(p.z) > abs(p.x) ? p.xy * vec2(1.0, 0.5) + 0.5 : p.zy * vec2(1.0, 0.5) + 0.5;
    float d = min(floorD, boxD);
    return vec2(d, floorD < boxD ? 0.0 : 1.0 + fract(id * 0.37));
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.25 * sin(t * 0.6), 0.15 + 0.15 * (syn_BassLevel * response), t * 2.8);
    vec3 rd = normalize(vec3(rot2(0.06 * sin(t * 0.4) * (syn_MidLevel * response)) * p, 1.15));
    float d = 0.0;
    vec2 hit = vec2(0.0);
    vec2 fuv = vec2(0.5);
    for (int i = 0; i < 42; ++i) {
        hit = mapCanyon(ro + rd * d, t, fuv);
        if (hit.x < 0.004 || d > 28.0) break;
        d += hit.x * 0.85;
    }
    vec3 sky = pal(rd.y * 0.6 + 0.15) * (0.28 + 0.35 * (syn_OnBeat * response));
    if (d > 28.0) return sky;

    vec3 pos = ro + rd * d;
    vec2 dummyUV = vec2(0.0);
    float e = 0.01;
    vec3 n = normalize(vec3(
        mapCanyon(pos + vec3(e, 0.0, 0.0), t, dummyUV).x - mapCanyon(pos - vec3(e, 0.0, 0.0), t, dummyUV).x,
        mapCanyon(pos + vec3(0.0, e, 0.0), t, dummyUV).x - mapCanyon(pos - vec3(0.0, e, 0.0), t, dummyUV).x,
        mapCanyon(pos + vec3(0.0, 0.0, e), t, dummyUV).x - mapCanyon(pos - vec3(0.0, 0.0, e), t, dummyUV).x
    ));
    float diff = max(dot(n, normalize(vec3(0.4, 0.8, -0.5))), 0.12);
    vec3 mat = hit.y < 0.5
        ? pal(0.1) * (0.15 + 0.35 * sceneRamp(0.95, 0.99, max(abs(fract(pos.x) - 0.5), abs(fract(pos.z) - 0.5)) * 2.0))
        : pal(hit.y * 0.7) * diff * (1.0 + 0.6 * (syn_OnBeat * response));

    if (hit.y >= 1.0) {
        vec4 img = sampleMediaTile(fuv);
        mat = mix(mat, img.rgb * (0.65 + 0.65 * diff), img.a);
    }
    float fog = (exp(-0.075 * d)) * sceneDistanceMist;
    return mix(sky, mat, fog);
}

void main() {
    vec2 res = max(RENDERSIZE, vec2(1.0));
    vec2 uv = gl_FragCoord.xy / res;
    vec2 p = (gl_FragCoord.xy - 0.5 * res) / res.y;
    float t = motion * 1.45;
    vec3 col = renderScene(uv, p, t);
    col *= clamp(1.0 - 0.25 * dot(p, p), 0.28, 1.0);
    col = col / (vec3(1.0) + 0.18 * col);
    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}

// Family 31: Sound Terrain — Ocean Swell
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
uniform float sceneTerrainHeight;
uniform float sceneRidgeScale;

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

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0)), d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    mat2 m = mat2(0.80, -0.60, 0.60, 0.80);
    for (int i = 0; i < 4; ++i) {
        v += a * vnoise(p);
        p = m * p * 2.02 + vec2(17.1, 31.7);
        a *= 0.5;
    }
    return v;
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

float synthSpec(float x) {
    float b = clamp(x, 0.0, 1.0);
    float dm = (b - 0.45) * 3.4;
    float dh = (b - 0.85) * 4.2;
    float low = (syn_BassLevel * response) * exp(-b * 4.5);
    float mid = (syn_MidLevel * response) * exp(-(dm * dm));
    float hi = (syn_HighLevel * response) * exp(-(dh * dh));
    float rip = 0.5 + 0.5 * sin(b * 26.0 - TIME * 4.2 + (syn_MidLevel * response) * 3.0);
    return clamp((low + mid + hi) * (0.65 + 0.35 * rip) + 0.14 * (syn_Level * response) + 0.18 * (syn_OnBeat * response) * (1.0 - b * 0.5), 0.04, 1.25);
}

float terrainH(vec2 xz, float t) {
    xz *= sceneRidgeScale;
    float n = fbm(xz * 0.32 + vec2(0.0, t * 0.25));
    float s = synthSpec(clamp(abs(xz.x) * 0.18, 0.0, 1.0));
    if (VARIANT == 2) {
        float valley = sceneRamp(0.4, 2.8, abs(xz.x));
        return (n * 1.8 + 0.6 * s) * valley - 0.4;
    } else if (VARIANT == 3) {
        return (0.45 * sin(xz.y * 0.9 - t * 2.4) * cos(xz.x * 0.6) * (0.6 + (syn_BassLevel * response)) + n * 0.4) * sceneTerrainHeight;
    }
    return (n * 1.45 * (0.6 + 0.8 * s) + 0.35 * sin(xz.y * 0.8) * (syn_BassLevel * response)) * sceneTerrainHeight;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.0, 1.35 + 0.2 * (syn_BassLevel * response), t * 2.2);
    vec3 rd = normalize(vec3(p.x, p.y - 0.14, 1.15));

    vec3 sky = pal(rd.y * 0.8 + 0.1) * (0.25 + 0.25 * (syn_OnBeat * response));
    float sunD = length(p - vec2(0.0, 0.16));
    vec3 sunCol = mix(vec3(1.0, 0.82, 0.28), pal(0.4), 0.35);
    sky = mix(sky, sunCol, sceneRamp(0.24, 0.22, sunD));
    vec4 img = sampleMedia((p - vec2(0.0, 0.16)) * 2.1 + 0.5);
    sky = mix(sky, img.rgb, img.a * sceneRamp(0.30, 0.20, sunD));

    if (rd.y > 0.08) return sky;
    float d = 1.0;
    float hit = 0.0;
    for (int i = 0; i < 42; ++i) {
        vec3 pos = ro + rd * d;
        float h = terrainH(pos.xz, t);
        if (pos.y < h) { hit = 1.0; break; }
        d += max((pos.y - h) * 0.55, 0.14);
        if (d > 22.0) break;
    }
    if (hit < 0.5) return sky;

    vec3 pos = ro + rd * d;
    if (VARIANT == 0) {
        vec2 g = abs(fract(pos.xz * 1.2 - 0.5) - 0.5);
        float wire = sceneRamp(0.08, 0.02, min(g.x, g.y));
        vec3 m = mix(vec3(0.02, 0.02, 0.06), pal(pos.y * 0.5 + 0.2) * (1.1 + 0.6 * (syn_OnBeat * response)), wire);
        return mix(sky, m, exp(-0.085 * d));
    } else {
        float e = 0.06;
        vec3 n = normalize(vec3(terrainH(pos.xz - vec2(e, 0.0), t) - terrainH(pos.xz + vec2(e, 0.0), t), 2.0 * e, terrainH(pos.xz - vec2(0.0, e), t) - terrainH(pos.xz + vec2(0.0, e), t)));
        float diff = max(dot(n, normalize(vec3(0.3, 0.8, 0.5))), 0.15);
        vec3 m = pal(pos.y * 0.45 + 0.15) * diff * (0.9 + 0.5 * (syn_OnBeat * response));
        return mix(sky, m, exp(-0.085 * d));
    }
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

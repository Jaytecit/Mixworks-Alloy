// Family 11: Pixel Sort Cascade — Sideways
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
uniform float sceneStreakLength;
uniform float sceneLuminanceThreshold;

out vec4 fragColor;

const int VARIANT = 1;

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

vec3 srcField(vec2 u, float t) {
    float wave = vnoise(u * 6.0 + vec2(0.0, t * 0.7));
    vec3 bg = pal(wave + u.x * 0.35 + t * 0.06) * sceneRamp(0.25, 0.88, wave + 0.25 * (syn_BassLevel * response));
    vec4 img = sampleMedia(u);
    return mix(bg, img.rgb, img.a);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 dir = vec2(0.0, 1.0);
    if (VARIANT == 1) dir = vec2(sign(hash21(vec2(floor(uv.y * 28.0), 1.0)) - 0.5), 0.0);
    else if (VARIANT == 2) dir = (uv - 0.5) / max(length(uv - 0.5), 1e-4);
    else if (VARIANT == 3) dir = normalize(vec2(-1.0, 1.0));

    float streakLen = (0.05 + 0.18 * ((syn_BassLevel * response) + 0.7 * (syn_OnBeat * response))) * sceneStreakLength;
    float thresh = (0.48 - 0.22 * (syn_Level * response)) * sceneLuminanceThreshold;
    vec3 best = srcField(uv, t);
    float bestLum = dot(best, vec3(0.299, 0.587, 0.114));

    for (int i = 1; i <= 10; ++i) {
        float fi = float(i) / 10.0;
        vec2 sUV = clamp(uv + dir * fi * streakLen, 0.0, 1.0);
        vec3 sCol = srcField(sUV, t);
        float lum = dot(sCol, vec3(0.299, 0.587, 0.114));
        if (lum > thresh && lum > bestLum) {
            best = mix(best, sCol, 0.85);
            bestLum = lum;
        }
    }
    best += vec3(0.04, 0.02, 0.06) * (syn_HighLevel * response) * step(thresh + 0.15, bestLum);
    return best;
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

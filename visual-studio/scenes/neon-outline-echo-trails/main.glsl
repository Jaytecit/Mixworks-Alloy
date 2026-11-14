// Family 49: Neon Outline — Echo Trails
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
uniform float sceneLineThickness;
uniform float sceneEdgeSensitivity;

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

float sceneLuma(vec2 u, float t) {
    vec2 p = u - 0.5;
    float s = sceneRamp(0.28, 0.26, abs(length(p) - 0.22 - 0.04 * sin(atan(p.y, p.x + 1e-6) * 5.0 + t * 2.0)));
    vec4 img = sampleMedia(u);
    return mix(s, dot(img.rgb, vec3(0.299, 0.587, 0.114)) * img.a, img.a);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float stepPx = ((VARIANT == 1) ? 0.009 : 0.003) * sceneLineThickness;
    float c = sceneLuma(uv, t);
    float gx = sceneLuma(clamp(uv + vec2(stepPx, 0.0), 0.0, 1.0), t) - sceneLuma(clamp(uv - vec2(stepPx, 0.0), 0.0, 1.0), t);
    float gy = sceneLuma(clamp(uv + vec2(0.0, stepPx), 0.0, 1.0), t) - sceneLuma(clamp(uv - vec2(0.0, stepPx), 0.0, 1.0), t);
    float edge = clamp(length(vec2(gx, gy)) / sceneEdgeSensitivity * (VARIANT == 1 ? 4.5 : 3.2), 0.0, 1.4);

    if (VARIANT == 2) {
        edge *= step(0.45, fract((uv.x + uv.y) * 26.0 - t * 2.5));
    }

    vec3 col = vec3(0.015, 0.02, 0.05) + pal(c * 0.4 + t * 0.05) * 0.10;
    col += pal(uv.x * 0.5 + t * 0.12) * edge * (1.0 + 0.8 * (syn_OnBeat * response) + 0.5 * (syn_HighLevel * response));

    if (VARIANT == 3) {
        for (int i = 1; i <= 3; ++i) {
            float fi = float(i);
            vec2 eUV = (uv - 0.5) / (1.0 + fi * 0.08 * (0.5 + (syn_OnBeat * response))) + 0.5;
            float ec = abs(sceneLuma(clamp(eUV + vec2(0.004, 0.0), 0.0, 1.0), t) - sceneLuma(clamp(eUV - vec2(0.004, 0.0), 0.0, 1.0), t));
            col += pal(fi * 0.25 + t * 0.1) * clamp(ec * 3.0, 0.0, 1.0) * (0.55 / fi);
        }
    }
    return col;
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

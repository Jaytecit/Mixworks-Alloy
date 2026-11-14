// Family 34: Halftone Pop — Line
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
uniform float sceneDotDensity;
uniform float sceneInkCoverage;

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

vec3 samplePop(vec2 uv, float t) {
    vec2 p = uv - 0.5;
    float grad = 0.5 + 0.5 * sin(p.x * 6.0 + p.y * 4.0 - t * 2.0);
    vec3 bg = pal(grad * 0.6 + length(p) * 0.4 + t * 0.05);
    vec4 img = sampleMedia(uv);
    return mix(bg, img.rgb, img.a);
}

float screenPattern(vec2 q, float lum) {
    vec2 gv = fract(q) - 0.5;
    float r = clamp(sqrt(max(lum, 0.0)) * 0.52, 0.04, 0.56);
    if (VARIANT == 0 || VARIANT == 3) return sceneRamp(r + 0.06, r - 0.06, length(gv));
    else if (VARIANT == 1) return sceneRamp(r + 0.06, r - 0.06, abs(gv.y));
    else return sceneRamp(r * 0.8 + 0.05, r * 0.8 - 0.05, min(abs(gv.x), abs(gv.y)));
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float density = (36.0 - 10.0 * (syn_BassLevel * response)) * sceneDotDensity;
    vec3 base = samplePop(uv, t);
    float lum = (dot(base, vec3(0.299, 0.587, 0.114))) * sceneInkCoverage;

    if (VARIANT == 3) {
        float off = 0.008 + 0.016 * (syn_HighLevel * response);
        vec3 cR = samplePop(clamp(uv + vec2(off, 0.0), 0.0, 1.0), t);
        vec3 cG = base;
        vec3 cB = samplePop(clamp(uv - vec2(off, 0.0), 0.0, 1.0), t);
        float dR = screenPattern(rot2(0.26) * p * density, cR.r);
        float dG = screenPattern(rot2(0.78) * p * density, cG.g);
        float dB = screenPattern(rot2(1.31) * p * density, cB.b);
        return vec3(dR, dG, dB) * (0.95 + 0.35 * (syn_OnBeat * response));
    }

    float dotMask = screenPattern(rot2(0.7854) * p * density, lum);
    return mix(vec3(0.03, 0.02, 0.06), base * (1.15 + 0.35 * (syn_OnBeat * response)), dotMask);
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

// Family 30: Droste Recursion — Log Spiral
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
uniform float sceneZoomDepth;
uniform float sceneRepetitionSpacing;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 z = p;
    if (VARIANT == 3) {
        vec2 a = p - vec2(0.25, 0.0), b = p + vec2(0.25, 0.0);
        float d = max(dot(b, b), 1e-4);
        z = vec2(dot(a, b), a.y * b.x - a.x * b.y) / d;
    }

    float r = (VARIANT == 2) ? max(abs(z.x), abs(z.y)) : length(z);
    r = max(r, 1e-4);
    float ang = atan(z.y, z.x + 1e-6);

    float logR = log2(r) / sceneRepetitionSpacing;
    if (VARIANT == 1) logR += ang / 6.2831853;
    float fR = fract(logR - t * (0.55 * sceneZoomDepth) - 0.18 * (syn_BassLevel * response));
    float ringScale = exp2(fR - 0.5);

    vec2 recP = (VARIANT == 2)
        ? z / max(r, 1e-4) * ringScale * 0.45
        : vec2(cos(ang), sin(ang)) * ringScale * 0.45;
    vec2 recUV = recP + 0.5;

    float border = exp(-abs(fR - 0.5) * 26.0);
    vec3 col = pal(fR + ang / 6.2831853 + t * 0.08) * (0.28 + 0.72 * border);
    vec4 img = sampleMediaTile(recUV);
    col = mix(col, img.rgb * (0.9 + 0.35 * (syn_OnBeat * response)), img.a);
    col += pal(0.25) * border * (0.5 + 0.7 * (syn_OnBeat * response));
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

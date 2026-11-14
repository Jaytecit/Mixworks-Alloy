// Family 22: Moiré Interference — Linear
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
uniform float scenePatternFrequency;
uniform float sceneInterferenceContrast;

out vec4 fragColor;

const int VARIANT = 0;

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
    float freq = (34.0 + 14.0 * (syn_BassLevel * response)) * scenePatternFrequency;
    float ang = 0.18 + 0.35 * sin(t * 0.6 + (syn_MidLevel * response) * 1.5);
    float g1 = 0.0, g2 = 0.0;

    if (VARIANT == 0) {
        g1 = 0.5 + 0.5 * cos(p.x * freq - t * 2.0);
        g2 = 0.5 + 0.5 * cos((rot2(ang) * p).x * freq * 1.06 + t * 1.5);
    } else if (VARIANT == 1) {
        vec2 off = vec2(0.18 * cos(t * 0.8), 0.12 * sin(t * 0.7));
        g1 = 0.5 + 0.5 * cos(length(p - off) * freq - t * 2.4);
        g2 = 0.5 + 0.5 * cos(length(p + off) * freq * 1.04 - t * 1.9);
    } else if (VARIANT == 2) {
        vec2 p1 = p * freq * 0.45;
        vec2 p2 = (rot2(ang) * p) * freq * 0.48;
        g1 = sceneRamp(0.45, 0.15, length(fract(p1) - 0.5));
        g2 = sceneRamp(0.45, 0.15, length(fract(p2) - 0.5));
    } else {
        vec2 p1 = rot2(t * 0.45) * p;
        vec2 p2 = rot2(-t * 0.45 - 0.35 * (syn_MidLevel * response)) * p;
        g1 = 0.5 + 0.5 * cos(p1.x * freq) * cos(p1.y * freq);
        g2 = 0.5 + 0.5 * cos(p2.x * freq * 1.05) * cos(p2.y * freq * 1.05);
    }

    float beatMask = abs(g1 - g2);
    float inter = clamp(g1 * g2 * (1.4 * sceneInterferenceContrast) + beatMask * 0.4, 0.0, 1.0);
    vec3 col = pal(inter * 0.8 + beatMask * 0.4 + t * 0.08) * (0.25 + 0.95 * inter);
    vec4 img = sampleMedia(uv);
    col = mix(col, img.rgb * (0.45 + 0.95 * beatMask), img.a);
    col += pal(0.2) * pow(beatMask, 3.0) * 0.5 * (syn_OnBeat * response);
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

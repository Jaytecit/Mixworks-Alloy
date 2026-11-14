// Family 48: Screenprint Layers — Glitch Overprint
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
uniform float sceneRegistrationOffset;
uniform float sceneInkCoverage;

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

float plateVal(vec2 u, float t) {
    vec2 p = u - 0.5;
    float d = length(p) * 3.0 + 0.4 * sin(p.x * 8.0 + t) * cos(p.y * 8.0 - t);
    float proc = step(0.45, fract(d));
    vec4 img = sampleMedia(u);
    float l = dot(img.rgb, vec3(0.299, 0.587, 0.114));
    return mix(proc, step(0.42, l), img.a);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 paper = vec3(0.93, 0.90, 0.84) - 0.05 * hash21(floor(uv * 240.0));
    float drift = (0.012 + 0.025 * (syn_BassLevel * response) + 0.02 * (syn_OnBeat * response)) * sceneRegistrationOffset;
    vec2 off1 = vec2(drift, -drift * 0.6);
    vec2 off2 = vec2(-drift * 0.8, drift);

    if (VARIANT == 3 && (syn_OnBeat * response) > 0.3) {
        off1.x += (hash21(vec2(floor(uv.y * 16.0), floor(t * 8.0))) - 0.5) * 0.09;
    }

    float p1 = plateVal(clamp(uv + off1, 0.0, 1.0), t);
    float p2 = plateVal(clamp(uv + off2, 0.0, 1.0), t);
    if (VARIANT == 2) {
        float tear = 0.04 * (vnoise(uv * 16.0) - 0.5);
        p1 *= step(0.08 + tear, uv.x) * step(uv.x, 0.92 + tear);
        p2 *= step(0.08 - tear, uv.y) * step(uv.y, 0.92 - tear);
    }

    vec3 col = mix(paper, pal(0.15) * 0.85, clamp(p1 * (0.82 * sceneInkCoverage), 0.0, 1.0));
    col = mix(col, pal(0.55) * 0.85, clamp(p2 * (0.72 * sceneInkCoverage), 0.0, 1.0));
    if (VARIANT == 1) {
        float p3 = plateVal(clamp(uv - off1 * 0.7, 0.0, 1.0), t);
        float p4 = plateVal(clamp(uv - off2 * 0.7, 0.0, 1.0), t);
        col = mix(col, vec3(0.95, 0.82, 0.12), clamp(p3 * (0.55 * sceneInkCoverage), 0.0, 1.0));
        col = mix(col, vec3(0.08, 0.08, 0.12), clamp(p4 * (0.50 * sceneInkCoverage), 0.0, 1.0));
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

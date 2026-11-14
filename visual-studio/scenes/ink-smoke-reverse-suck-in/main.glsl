// Family 28: Ink Smoke — Reverse Suck-In
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
uniform float sceneInkDensity;
uniform float scenePlumeSpread;

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

vec2 curl2D(vec2 p) {
    float e = 0.03;
    float n1 = fbm(p + vec2(0.0, e)) - fbm(p - vec2(0.0, e));
    float n2 = fbm(p + vec2(e, 0.0)) - fbm(p - vec2(e, 0.0));
    return vec2(n1, -n2) / (2.0 * e);
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
    vec2 q = p * (2.2 / scenePlumeSpread);
    for (int i = 0; i < 3; ++i) {
        vec2 c = curl2D(q * 0.8 + vec2(0.0, -t * 0.25));
        if (VARIANT == 3) c -= normalize(q + 1e-4) * (0.65 + 0.6 * (syn_BassLevel * response));
        q += c * (0.14 + 0.08 * (syn_BassLevel * response));
    }

    float n = fbm(q + vec2(t * 0.15, -t * 0.2)) * sceneInkDensity;
    vec4 img = sampleMedia(clamp(uv +  (q - p * (2.2 / scenePlumeSpread)) * 0.18, 0.0, 1.0));

    if (VARIANT == 0) {
        vec3 paper = mix(vec3(0.92, 0.90, 0.86), pal(n * 0.4) * 0.85 + 0.2, 0.35);
        float ink = sceneRamp(0.32, 0.72, n + 0.15 * (syn_OnBeat * response));
        vec3 col = mix(paper, vec3(0.03, 0.03, 0.05), ink);
        return mix(col, img.rgb * (1.0 - 0.5 * ink), img.a);
    } else if (VARIANT == 2) {
        float drop = sceneRamp(0.45, 0.78, sin(length(q) * 4.5 - t * 2.5 + n * 4.0) * 0.5 + 0.5);
        vec3 col = pal(n * 1.4 + drop * 0.4 + t * 0.08) * (0.25 + 0.85 * drop);
        return mix(col, img.rgb, img.a * (0.5 + 0.5 * drop));
    } else {
        float plume = pow(clamp(n * 1.35, 0.0, 1.0), 2.0);
        vec3 col = pal(n * 0.9 + t * 0.08) * plume * (0.9 + 0.6 * (syn_OnBeat * response));
        return mix(col, img.rgb * (0.5 + 0.8 * plume), img.a);
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

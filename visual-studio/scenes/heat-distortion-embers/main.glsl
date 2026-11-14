// Family 20: Heat Distortion — Embers
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
uniform float sceneHeatShimmer;
uniform float sceneFireIntensity;

out vec4 fragColor;

const int VARIANT = 2;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float n = fbm(vec2(p.x * 4.5, p.y * 3.5 - t * 2.4));
    vec2 heatUV = clamp(uv + vec2((n - 0.5) * (0.04 + 0.07 * (syn_BassLevel * response)), 0.015 * sin(p.y * 18.0 - t * 5.0)) * sceneHeatShimmer, 0.0, 1.0);
    vec4 img = sampleMedia(heatUV);

    float flame = 0.0;
    if (VARIANT == 0) {
        float shape = 1.0 - sceneRamp(0.0, 0.42 - 0.35 * uv.y, abs(p.x + 0.12 * (n - 0.5)));
        flame = clamp((shape * (1.25 - uv.y * 1.1) + n * 0.65) - 0.48, 0.0, 1.2);
    } else if (VARIANT == 1) {
        flame = clamp((1.05 - uv.y * 1.15 + (n - 0.3) * 0.85 + 0.25 * (syn_BassLevel * response)) - 0.35, 0.0, 1.2);
    } else if (VARIANT == 2) {
        flame = clamp((n - 0.45) * 0.9 * (1.0 - uv.y * 0.7), 0.0, 1.0);
        vec2 ep = uv * vec2(22.0, 14.0) - vec2(0.0, t * 3.2);
        vec2 id = floor(ep), gv = fract(ep) - 0.5;
        float r = hash21(id);
        float spark = sceneRamp(0.14, 0.01, length(gv - (hash22(id) - 0.5) * 0.5)) * step(0.68, r);
        flame += spark * (1.2 + 0.8 * (syn_OnBeat * response));
    } else {
        vec2 tp = rot2(-0.25) * (p - vec2(0.0, -0.35));
        float cone = 1.0 - sceneRamp(0.02, 0.16 + 0.22 * (tp.y + 0.2), abs(tp.x + 0.08 * (n - 0.5)));
        flame = clamp(cone * sceneRamp(-0.1, 0.1, tp.y) * (1.2 - tp.y) + n * 0.35, 0.0, 1.4);
    }

    vec3 fireCol = (VARIANT == 3)
        ? mix(vec3(0.03, 0.06, 0.25), vec3(0.45, 0.90, 1.0), clamp(flame, 0.0, 1.0)) + vec3(1.0) * pow(clamp(flame, 0.0, 1.0), 3.0)
        : mix(vec3(0.05, 0.01, 0.01), vec3(1.0, 0.32, 0.04), clamp(flame * 1.2, 0.0, 1.0)) + vec3(1.0, 0.88, 0.35) * pow(clamp(flame, 0.0, 1.0), 2.4);

    fireCol *= sceneFireIntensity;
    vec3 col = mix(fireCol * 0.35, img.rgb, img.a) + fireCol * (0.75 + 0.45 * (syn_OnBeat * response));
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

// Family 33: CRT Overdrive — Phosphor Trails
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
uniform float sceneScanlineStrength;
uniform float sceneScreenCurvature;

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

vec3 crtSample(vec2 cUV, float t, float chroma) {
    vec2 p = cUV - 0.5;
    float rings = 0.5 + 0.5 * cos(length(p) * 26.0 - t * 3.0);
    vec3 bg = pal(cUV.x * 0.5 + rings * 0.3 + t * 0.06) * (0.35 + 0.65 * rings);
    vec4 img = sampleMediaChroma(cUV, chroma);
    return mix(bg, img.rgb, img.a);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 cp = uv - 0.5;
    cp *= 1.0 + 0.18 * sceneScreenCurvature * dot(cp, cp);
    vec2 cUV = cp + 0.5;

    if (VARIANT == 1) {
        float band = exp(-abs(cUV.y - fract(t * 0.25)) * 18.0);
        cUV.x += band * 0.03 * sin(cUV.y * 80.0 + t * 12.0) * (0.6 + (syn_OnBeat * response));
    } else if (VARIANT == 2) {
        cUV.y = fract(cUV.y + t * 0.35 * (0.3 + (syn_OnBeat * response)));
        cUV.x += 0.04 * sin(cUV.y * 24.0 + t * 18.0) * (0.5 + (syn_BassLevel * response));
    }

    float chroma = 0.005 + 0.018 * (syn_HighLevel * response) + (VARIANT == 2 ? 0.015 : 0.0);
    vec3 col = crtSample(cUV, t, chroma);

    if (VARIANT == 3) {
        for (int i = 1; i <= 4; ++i) {
            float fi = float(i);
            col = max(col, crtSample(cUV + vec2(-0.012 * fi, 0.0), t - fi * 0.08, chroma) * (0.85 - 0.16 * fi));
        }
    }

    float scan = 0.72 + 0.28 * sin(cUV.y * 420.0);
    float triad = 0.85 + 0.15 * sin(cUV.x * 640.0);
    col *= mix(1.0, scan, sceneScanlineStrength) * triad;
    vec2 edge = sceneRamp(0.0, 0.03, cUV) * sceneRamp(1.0, 0.97, cUV);
    return col * edge.x * edge.y * (0.9 + 0.25 * (syn_OnBeat * response));
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

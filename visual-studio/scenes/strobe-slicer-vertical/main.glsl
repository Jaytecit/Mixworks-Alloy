// Family 15: Strobe Slicer — Vertical
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
uniform float sceneSliceCount;
uniform float sceneShiftDistance;
uniform float sceneFlashStrength;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float beatStep = floor(t * 2.2) + floor((syn_OnBeat * response) * 2.0);
    float slices = (10.0 + 4.0 * floor((syn_BassLevel * response) * 2.0)) * sceneSliceCount;
    vec2 coord = uv;
    float idx = 0.0;
    vec2 slideDir = vec2(1.0, 0.0);

    if (VARIANT == 0) {
        idx = floor(uv.y * slices);
        slideDir = vec2(1.0, 0.0);
    } else if (VARIANT == 1) {
        idx = floor(uv.x * slices * 1.5);
        slideDir = vec2(0.0, 1.0);
    } else if (VARIANT == 2) {
        vec2 dp = rot2(0.7854) * p;
        idx = floor(dp.y * slices);
        slideDir = vec2(0.7071, 0.7071);
    } else {
        float xStep = floor(uv.x * 8.0);
        idx = floor(uv.y * slices + xStep * 0.5);
        slideDir = vec2(1.0, 0.35);
    }

    float parity = mod(idx, 2.0) * 2.0 - 1.0;
    float rnd = hash21(vec2(idx, beatStep));
    float shift = (parity * (0.04 + 0.24 * (syn_OnBeat * response) + 0.14 * (syn_Hits * response)) * (0.4 + 0.6 * rnd)) * sceneShiftDistance;
    vec2 sUV = fract(coord + slideDir * shift);

    float stripe = 0.5 + 0.5 * cos((sUV.x * 14.0 + sUV.y * 8.0) - t * 2.0);
    vec3 bg = pal(idx / slices + stripe * 0.25 + t * 0.08) * (0.35 + 0.65 * stripe);
    vec4 img = sampleMediaChroma(sUV, 0.02 * (syn_HighLevel * response) + 0.015 * (syn_OnBeat * response));
    vec3 col = mix(bg, img.rgb, img.a);

    float seam = sceneRamp(0.04, 0.0, abs(fract((VARIANT == 1 ? uv.x * slices * 1.5 : (VARIANT == 2 ? (rot2(0.7854) * p).y * slices : uv.y * slices)) ) - 0.5) - 0.46);
    col += vec3(1.0) * seam * sceneFlashStrength * (0.25 + 0.85 * (syn_OnBeat * response));
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

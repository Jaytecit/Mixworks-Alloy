// Family 13: Radial Spectrum Burst — Mirrored Halves
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
uniform float sceneSpectrumExtent;
uniform float sceneBarOrRayWidth;

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

float synthSpec(float x) {
    float b = clamp(x, 0.0, 1.0);
    float dm = (b - 0.45) * 3.4;
    float dh = (b - 0.85) * 4.2;
    float low = (syn_BassLevel * response) * exp(-b * 4.5);
    float mid = (syn_MidLevel * response) * exp(-(dm * dm));
    float hi = (syn_HighLevel * response) * exp(-(dh * dh));
    float rip = 0.5 + 0.5 * sin(b * 26.0 - TIME * 4.2 + (syn_MidLevel * response) * 3.0);
    return clamp((low + mid + hi) * (0.65 + 0.35 * rip) + 0.14 * (syn_Level * response) + 0.18 * (syn_OnBeat * response) * (1.0 - b * 0.5), 0.04, 1.25);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 pr = rot2(0.12 * t + 0.35 * (syn_MidLevel * response)) * p;
    float r = length(pr);
    float ang = atan(pr.y, pr.x + 1e-6);
    float normAng = fract(ang / 6.2831853 + 0.5);
    if (VARIANT == 2) normAng = abs(normAng - 0.5) * 2.0;

    float barCount = 48.0;
    float cellA = floor(normAng * barCount) / barCount;
    float fA = (fract(normAng * barCount) - 0.5) / sceneBarOrRayWidth + 0.5;
    float amp = (synthSpec(VARIANT == 1 || VARIANT == 3 ? cellA : normAng)) * sceneSpectrumExtent;

    vec3 col = pal(r * 0.4 + t * 0.05) * 0.12;
    float innerR = (VARIANT == 0) ? 0.06 : 0.18;
    float outerR = innerR + 0.34 * amp;

    float mask = 0.0;
    if (VARIANT == 0) {
        mask = sceneRamp(0.02, 0.0,  (r - outerR) / sceneBarOrRayWidth) * sceneRamp(0.03, 0.07, r) * (0.65 + 0.35 * cos(normAng * 180.0));
    } else if (VARIANT == 1 || VARIANT == 2) {
        float barW = sceneRamp(0.10, 0.20, fA) * sceneRamp(0.90, 0.80, fA);
        mask = barW * sceneRamp(innerR - 0.01, innerR + 0.01, r) * sceneRamp(outerR + 0.01, outerR - 0.01, r);
    } else {
        float barW = sceneRamp(0.12, 0.22, fA) * sceneRamp(0.88, 0.78, fA);
        float r1 = barW * sceneRamp(0.13, 0.15, r) * sceneRamp(0.15 + 0.18 * amp + 0.01, 0.15 + 0.18 * amp - 0.01, r);
        float amp2 = (synthSpec(fract(cellA + 0.5))) * sceneSpectrumExtent;
        float r2 = barW * sceneRamp(0.34, 0.36, r) * sceneRamp(0.36 + 0.14 * amp2 + 0.01, 0.36 + 0.14 * amp2 - 0.01, r);
        mask = max(r1, r2);
    }

    vec3 barCol = pal(normAng + r * 0.4 + t * 0.1) * (0.9 + 0.7 * (syn_OnBeat * response));
    vec4 img = sampleMedia(uv);
    barCol = mix(barCol, img.rgb * 1.15, img.a);
    col = mix(col, barCol, clamp(mask, 0.0, 1.0));
    col += pal(0.2) * exp(-abs(r - innerR) * 32.0) * (0.4 + 0.8 * (syn_OnBeat * response));
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

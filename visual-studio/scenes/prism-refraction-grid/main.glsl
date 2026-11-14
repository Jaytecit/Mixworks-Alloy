// Family 35: Prism Refraction — Grid
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
uniform float sceneDispersion;
uniform float scenePrismScale;

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
    float r = (texture(syn_Media, clamp(uv + vec2(amt, 0.0), 0.0, 1.0)).r) * scenePrismScale;
    float b = texture(syn_Media, clamp(uv - vec2(amt, 0.0), 0.0, 1.0)).b;
    return vec4(r, c.g, b, c.a * m.x * m.y);
}

float sdTri(vec2 p, float r) {
    const float k = 1.7320508;
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0) p = vec2(p.x - k * p.y, -k * p.x - p.y) * 0.5;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 qp = p;
    float ang = 0.0;
    if (VARIANT == 1) qp = fract(p * 3.5 + 0.5) - 0.5;
    else if (VARIANT == 2) { ang = t * 0.9 + (syn_MidLevel * response); qp = rot2(ang) * p; }
    else if (VARIANT == 3) {
        float a = atan(p.y, p.x + 1e-6);
        float sec = 6.2831853 / 8.0;
        a = abs(mod(a, sec) - sec * 0.5);
        qp = vec2(cos(a), sin(a)) * length(p) - vec2(0.25, 0.0);
    }

    float r = ((VARIANT == 1) ? 0.36 : 0.28 + 0.05 * (syn_BassLevel * response)) * scenePrismScale;
    float d = sdTri(qp, r);
    float inside = sceneRamp(0.015, -0.015, d);
    float edge = exp(-abs(d) * 55.0);

    vec2 disp = (normalize(qp + 1e-4) * (0.04 + 0.07 * (syn_HighLevel * response) + 0.05 * (syn_OnBeat * response)) * inside) * sceneDispersion;
    vec3 col = pal(p.x * 0.4 + t * 0.05) * 0.22;
    vec4 imgR = sampleMedia(clamp(uv + disp * 1.4, 0.0, 1.0));
    vec4 imgG = sampleMedia(clamp(uv + disp * 0.7, 0.0, 1.0));
    vec4 imgB = sampleMedia(clamp(uv - disp * 1.2, 0.0, 1.0));
    vec3 bg = mix(col, vec3(imgR.r, imgG.g, imgB.b), max(imgG.a, inside * 0.4));

    vec3 rainbow = pal(dot(qp, vec2(1.2, 0.8)) * 1.6 + t * 0.15);
    bg = mix(bg, mix(bg, rainbow, 0.55), inside);
    return bg + mix(rainbow, vec3(1.0), 0.5) * edge * (0.9 + 0.7 * (syn_OnBeat * response));
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

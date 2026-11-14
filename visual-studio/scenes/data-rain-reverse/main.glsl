// Family 36: Data Rain — Reverse
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
uniform float sceneColumnDensity;
uniform float sceneTrailLength;

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
    vec2 rp = p;
    if (VARIANT == 1) rp = rot2(0.32) * p;
    else if (VARIANT == 2) {
        float r = max(length(p), 0.04);
        rp = vec2(atan(p.y, p.x + 1e-6) * 0.55, -log(r) * 0.45);
    }

    vec2 grid = rp * vec2(26.0 * sceneColumnDensity, 18.0);
    float colID = floor(grid.x);
    float spd = (1.8 + 2.5 * hash21(vec2(colID, 3.7))) * (VARIANT == 3 ? -1.0 : 1.0);
    float yScroll = grid.y + t * spd;
    vec2 cell = vec2(colID, floor(yScroll));
    vec2 gv = fract(vec2(grid.x, yScroll)) - 0.5;

    float trail = fract((yScroll) * 0.08 + hash21(vec2(colID, 9.1)));
    float bright = pow(1.0 - trail, 3.0 / sceneTrailLength) * (0.7 + 0.7 * (syn_OnBeat * response));
    float glyphSeed = floor(hash21(cell + floor(t * 4.0) * 0.1) * 32.0);
    vec2 sub = floor((gv + 0.5) * 4.0);
    float bit = step(0.45, hash21(sub + glyphSeed));
    float glyph = bit * sceneRamp(0.45, 0.35, max(abs(gv.x), abs(gv.y)));

    vec3 bg = vec3(0.01, 0.025, 0.02);
    vec4 img = sampleMedia(uv);
    bg = mix(bg, img.rgb * 0.45, img.a);

    vec3 rainCol = mix(pal(0.05 + colID * 0.03), vec3(0.85, 1.0, 0.9), step(0.92, 1.0 - trail));
    return bg + rainCol * glyph * bright * (1.1 + 0.6 * (syn_HighLevel * response));
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

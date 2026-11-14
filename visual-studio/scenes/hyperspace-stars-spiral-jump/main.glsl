// Family 18: Hyperspace Stars — Spiral Jump
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
uniform float sceneStarfieldScale;
uniform float sceneStreakLength;

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
    vec3 col = vec3(0.01, 0.012, 0.035);
    vec2 wp = p * sceneStarfieldScale;
    if (VARIANT == 1) wp += vec2(0.16 * sin(t * 0.8), 0.10 * cos(t * 0.6));

    for (int l = 0; l < 5; ++l) {
        float fl = float(l);
        float z = fract(fl * 0.2 - t * 0.55 - 0.15 * (syn_BassLevel * response));
        float depth = mix(8.0, 0.35, z);
        vec2 lp = wp * depth;
        if (VARIANT == 3) lp = rot2((1.0 - z) * (1.8 + (syn_MidLevel * response))) * lp;
        vec2 id = floor(lp);
        vec2 gv = fract(lp) - 0.5;
        vec2 rnd = hash22(id + fl * 17.3);
        vec2 sp = gv - (rnd - 0.5) * 0.65;
        float stretch = ((VARIANT == 2) ? (1.0 + 3.5 * (syn_OnBeat * response)) : (1.5 + 1.8 * z + 1.5 * (syn_BassLevel * response))) * sceneStreakLength;
        vec2 dir = lp / max(length(lp), 1e-4);
        float par = dot(sp, dir);
        float perp = length(sp - dir * clamp(par, -0.06 * stretch, 0.06 * stretch));
        float star = sceneRamp(0.055, 0.005, perp) * sceneRamp(0.0, 0.25, z) * sceneRamp(1.0, 0.82, z);
        col += pal(rnd.x + fl * 0.2) * star * (0.9 + 0.7 * (syn_OnBeat * response));

        if (syn_MediaType > 0.0 && rnd.y > 0.84) {
            vec2 mUV = (sp * 2.2) + 0.5;
            vec4 img = sampleMedia(mUV);
            col = mix(col, img.rgb, img.a * sceneRamp(0.0, 0.25, z) * sceneRamp(1.0, 0.75, z));
        }
    }
    if (VARIANT == 2) {
        float ring = exp(-abs(length(p) - fract(t * 0.9) * 0.85) * 28.0) * (syn_OnBeat * response);
        col += pal(0.25) * ring;
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

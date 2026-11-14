// Family 46: Black Hole Lens — Static
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
uniform float sceneLensStrength;
uniform float sceneAccretionGlow;

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

float starLayer(vec2 uv, float thresh) {
    vec2 id = floor(uv), gv = fract(uv) - 0.5;
    float r = hash21(id);
    if (r < thresh) return 0.0;
    vec2 off = vec2(hash21(id + 11.3), hash21(id + 57.9)) - 0.5;
    float d = length(gv - off * 0.55);
    float rad = 0.04 + 0.06 * fract(r * 41.0);
    return sceneRamp(rad, 0.0, d) * (0.65 + 0.35 * sin(TIME * 2.5 + r * 62.83));
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 bh1 = (VARIANT == 1) ? 0.14 * vec2(cos(t * 0.9), sin(t * 0.7)) : (VARIANT == 2 ? 0.20 * vec2(cos(t * 1.2), sin(t * 1.2)) : vec2(0.0));
    vec2 bh2 = (VARIANT == 2) ? -bh1 : vec2(99.0);

    vec2 d1 = p - bh1;
    float r1 = max(length(d1), 0.02);
    vec2 warp = d1 / (r1 * r1 + 0.02) * (0.018 + 0.012 * (syn_BassLevel * response));
    if (VARIANT == 2) {
        vec2 d2 = p - bh2;
        float r2 = max(length(d2), 0.02);
        warp += d2 / (r2 * r2 + 0.02) * 0.016;
    }

    warp *= sceneLensStrength;
    vec2 lensedUV = fract(uv - warp);
    vec3 bg = vec3(0.01, 0.01, 0.03) + vec3(0.85, 0.92, 1.0) * starLayer((p - warp) * 22.0, 0.87);
    vec4 img = sampleMedia(lensedUV);
    bg = mix(bg, img.rgb, img.a);

    vec2 dp = rot2(0.18) * d1;
    float disc = exp(-abs(dp.y) * 26.0) * sceneRamp(0.45, 0.14, abs(dp.x)) * sceneRamp(0.09, 0.16, r1);
    float swirl = 0.45 + 0.55 * fbm(vec2(atan(dp.y, dp.x + 1e-6) * 3.0, r1 * 10.0 - t * 4.6));
    vec3 accCol = (mix(vec3(1.0, 0.35, 0.06), vec3(1.0, 0.92, 0.55), swirl) * disc * (1.2 + 0.6 * (syn_OnBeat * response))) * sceneAccretionGlow;

    float photon = exp(-abs(r1 - 0.13) * 55.0)
                 * (0.82 + 0.18 * cos(5.0 * atan(dp.y, dp.x + 1e-6) - t * 2.4));
    vec3 col = bg + accCol + pal(0.2) * photon * (1.1 + (syn_OnBeat * response));
    if (VARIANT == 3) {
        float jet = exp(-abs(d1.x) * (32.0 - 12.0 * abs(d1.y))) * exp(-abs(d1.y) * 1.4) * (0.6 + 0.9 * (syn_OnBeat * response));
        col += vec3(0.45, 0.88, 1.0) * jet;
    }
    float horizonMask = sceneRamp(0.105, 0.12, r1) * (VARIANT == 2 ? sceneRamp(0.085, 0.10, length(p - bh2)) : 1.0);
    return col * horizonMask;
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

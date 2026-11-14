// Family 27: Vortex Galaxy — Four Arms
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
uniform float sceneArmWinding;
uniform float sceneCoreGlow;

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
    vec3 col = vec3(0.01, 0.01, 0.03);
    col += vec3(0.8, 0.9, 1.0) * starLayer(p * 24.0, 0.88);

    float r = length(p);
    float a = atan(p.y, p.x + 1e-6);
    float arms = (VARIANT == 1) ? 4.0 : 2.0;
    float pull = (VARIANT == 3) ? (1.0 + 0.55 * (syn_BassLevel * response)) : 1.0;
    float twist = (log(max(r * pull, 0.02)) * 3.8 - t * (1.4 + 0.6 * (syn_MidLevel * response))) * sceneArmWinding;
    float armWave = 0.5 + 0.5 * cos(arms * (a + twist) + fbm(p * 4.0) * 1.6);

    if (VARIANT == 2) {
        vec2 bp = rot2(-t * 0.6) * p;
        float bar = exp(-abs(bp.y) * 18.0) * sceneRamp(0.24, 0.05, abs(bp.x));
        armWave = max(armWave, bar * 1.2);
    }

    float radialEnv = exp(-r * (2.2 + (VARIANT == 3 ? 1.5 * (syn_BassLevel * response) : 0.0)));
    float dust = fbm(rot2(twist * 0.35) * p * 5.5);
    vec3 gal = pal(r * 0.8 + armWave * 0.35 + t * 0.06) * pow(armWave, 1.8) * radialEnv * (0.9 + 0.7 * dust);
    gal += vec3(1.0, 0.92, 0.80) * sceneCoreGlow * exp(-r * 9.0) * (0.8 + 0.5 * (syn_OnBeat * response));

    vec2 coreUV = rot2(twist * 0.25) * p * 1.8 + 0.5;
    vec4 img = sampleMedia(coreUV);
    col = mix(col + gal, gal + img.rgb * (0.8 + 0.5 * (syn_OnBeat * response)), img.a * sceneRamp(0.45, 0.08, r));
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

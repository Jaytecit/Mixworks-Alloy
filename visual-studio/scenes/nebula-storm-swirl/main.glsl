// Family 25: Nebula Storm — Swirl
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
uniform float sceneCloudDensity;
uniform float sceneDustContrast;

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
    col += vec3(0.8, 0.9, 1.0) * (starLayer(p * 18.0, 0.87) + starLayer(p * 34.0 + 4.1, 0.92));

    vec2 qp = p;
    if (VARIANT == 1) qp = rot2(0.8 * exp(-length(p) * 1.3) + t * 0.15) * p;
    else if (VARIANT == 0) qp = vec2(p.x * 1.8, p.y * 0.65 - t * 0.12);

    float dens = 0.0;
    vec3 neb = vec3(0.0);
    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        vec2 np = qp * (1.8 + fi * 0.85) + vec2(t * 0.08 * (fi + 1.0), -t * 0.05);
        float n = fbm(np);
        dens += n * 0.35;
        neb += pal(n + fi * 0.18 + t * 0.04) * pow(n, 1.0 + 1.2 * sceneDustContrast) * 0.55;
    }

    if (VARIANT == 2) {
        float r = length(p);
        float ring = exp(-abs(r - fract(t * 0.35) * 0.9) * 16.0) * (0.5 + 0.9 * (syn_BassHits * response));
        neb += pal(r + 0.2) * (exp(-r * 3.5) + ring);
    } else if (VARIANT == 3) {
        float lane = sceneRamp(0.38, 0.68, fbm(p * 2.4 + vec2(t * 0.1, 0.0)));
        neb *= (1.0 - 0.72 * lane);
    }

    vec4 img = sampleMedia(uv);
    neb = mix(neb, neb + img.rgb * clamp(dens * 0.95, 0.15, 1.1), img.a);
    return col + neb * sceneCloudDensity * (0.85 + 0.45 * (syn_BassLevel * response) + 0.35 * (syn_OnBeat * response));
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

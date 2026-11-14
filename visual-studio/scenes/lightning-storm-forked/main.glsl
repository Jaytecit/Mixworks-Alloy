// Family 19: Lightning Storm — Forked
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
uniform float sceneDischargeBrightness;
uniform float sceneBoltWidth;

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

float boltDist(vec2 p, float seed, float t) {
    float n = fbm(vec2(p.y * 4.5, floor(t * 14.0) * 1.7 + seed)) - 0.5;
    return abs(p.x - n * 0.42);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float flash = (0.25 + 1.25 * max((syn_OnBeat * response), (syn_Hits * response))) * sceneDischargeBrightness;
    float cloud = fbm(p * 3.0 + vec2(t * 0.2, -t * 0.1));
    vec3 sky = pal(cloud * 0.5 + 0.45) * (0.12 + 0.22 * cloud * flash);
    vec4 img = sampleMedia(uv);
    sky = mix(sky, img.rgb * (0.28 + 0.85 * flash), img.a);

    float arc = 0.0;
    if (VARIANT == 0) {
        arc = exp(-boltDist(p, 1.0, t) * (48.0 / sceneBoltWidth));
    } else if (VARIANT == 1) {
        arc = exp(-boltDist(p, 1.0, t) * (48.0 / sceneBoltWidth));
        arc += 0.7 * exp(-boltDist(rot2(0.45) * (p - vec2(0.0, 0.1)), 3.7, t) * (58.0 / sceneBoltWidth)) * step(p.y, 0.12);
        arc += 0.7 * exp(-boltDist(rot2(-0.52) * (p + vec2(0.0, 0.05)), 7.2, t) * (58.0 / sceneBoltWidth)) * step(p.y, -0.04);
    } else if (VARIANT == 2) {
        for (int i = 0; i < 4; ++i) {
            float fi = float(i);
            vec2 rp = rot2(fi * 0.7854 + 0.2 * sin(t)) * p;
            arc += 0.65 * exp(-boltDist(rp - vec2(0.22 * (fi - 1.5), 0.0), fi * 2.3, t) * (52.0 / sceneBoltWidth));
        }
    } else {
        float r = length(p);
        float a = atan(p.y, p.x + 1e-6);
        float orb = exp(-abs(r - (0.14 + 0.05 * (syn_BassLevel * response))) * 42.0);
        float tendril = exp(-abs(sin(a * 5.0 + fbm(p * 8.0 + t * 3.0) * 6.0)) * ((8.0 + 18.0 * r) / sceneBoltWidth)) * exp(-r * 2.5);
        arc = orb * 1.4 + tendril;
    }

    vec3 boltCol = mix(pal(0.15), vec3(0.85, 0.96, 1.0), 0.75);
    return sky + boltCol * arc * flash;
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

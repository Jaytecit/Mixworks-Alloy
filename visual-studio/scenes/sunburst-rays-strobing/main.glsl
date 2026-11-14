// Family 40: Sunburst Rays — Strobing
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
uniform float sceneRayDensity;
uniform float sceneHaloWidth;

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
    vec2 sunPos = vec2(0.0, VARIANT == 0 ? -0.35 : (VARIANT == 1 ? 0.35 : 0.0));
    vec2 dp = p - sunPos;
    if (VARIANT == 3) dp = vec2(abs(p.x) - 0.55, p.y);

    float ang = atan(dp.y, dp.x + 1e-6);
    float r = length(dp);
    float ray = pow(0.5 + 0.5 * cos(ang * (14.0 * sceneRayDensity) + sin(t * 0.8) * 1.5), 2.5);
    float fogRay = 0.5 + 0.5 * vnoise(vec2(ang * 5.0, t * 0.6));
    float strobe = (VARIANT == 2) ? (0.35 + 1.25 * (syn_OnBeat * response) * step(0.45, fract(ang * 2.2 + t * 2.0))) : (0.75 + 0.65 * (syn_OnBeat * response));

    vec3 rayCol = (VARIANT == 0)
        ? mix(vec3(1.0, 0.45, 0.12), vec3(1.0, 0.92, 0.48), exp(-r * 1.8))
        : (VARIANT == 1 ? mix(vec3(0.45, 0.06, 0.28), vec3(1.0, 0.32, 0.15), exp(-r * 1.5)) : pal(ang / 6.2831853 + t * 0.08));

    vec3 col = rayCol * (ray * 0.8 + fogRay * 0.3) * exp(-r * 0.9) * strobe;
    col += rayCol * exp(-r * (5.5 / sceneHaloWidth)) * (0.9 + 0.5 * (syn_BassLevel * response));

    vec4 img = sampleMedia(uv);
    return mix(col, img.rgb * (0.55 + 0.75 * ray * strobe), img.a);
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

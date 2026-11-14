// Family 43: Mandala Spirograph — Hypotrochoid
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
uniform float scenePetalScale;
uniform float sceneStrokeWidth;

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
    float r = texture(syn_Media, clamp(uv + vec2(amt, 0.0), 0.0, 1.0)).r;
    float b = texture(syn_Media, clamp(uv - vec2(amt, 0.0), 0.0, 1.0)).b;
    return vec4(r, c.g, b, c.a * m.x * m.y);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 pr = rot2(0.15 * t + 0.4 * (syn_MidLevel * response)) * p;
    float r = length(pr) / scenePetalScale;
    float a = atan(pr.y, pr.x + 1e-6);

    float petals = (VARIANT == 2) ? 12.0 : (VARIANT == 3 ? 8.0 : 6.0);
    float foldA = abs(mod(a, 6.2831853 / petals) - 3.1415926 / petals);
    vec2 mp = vec2(cos(foldA), sin(foldA)) * r;

    float d = 1.0;
    if (VARIANT == 0) {
        d = abs(r - (0.28 + 0.14 * abs(sin(a * 5.0 + t)) + 0.06 * (syn_BassLevel * response)));
    } else if (VARIANT == 1) {
        d = min(abs(r - (0.30 + 0.12 * cos(a * 7.0 - t * 1.5))), abs(r - (0.22 + 0.10 * sin(a * 4.0 + t))));
    } else if (VARIANT == 2) {
        d = abs(sin(r * 28.0 - t * 2.0 + cos(a * 12.0) * 1.4)) * 0.04;
    } else {
        d = abs(r - (0.18 + 0.22 * pow(abs(cos(a * 4.0)), 1.5) * (0.8 + 0.4 * (syn_BassLevel * response))));
    }

    float line = exp(-d * (48.0 / sceneStrokeWidth));
    vec3 col = pal(r * 1.4 + foldA + t * 0.08) * (0.15 + 1.15 * line * (0.8 + 0.7 * (syn_OnBeat * response)));
    vec4 img = sampleMedia(mp * 1.8 + 0.2);
    return mix(col, img.rgb * (0.7 + 0.5 * line), img.a);
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

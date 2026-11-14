// Family 17: Plasma Lava — Electric
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
uniform float sceneFlowScale;
uniform float sceneRefractionDepth;

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

float plasmaVal(vec2 p, float t) {
    p *= sceneFlowScale;
    float spd = (VARIANT == 0) ? 0.55 : (VARIANT == 1 ? 1.45 : 0.95);
    float sc = (VARIANT == 1) ? 7.5 : 4.2;
    vec2 q = p * sc;
    float v = sin(q.x + t * spd) + sin(q.y - t * spd * 0.8);
    v += sin(q.x + q.y + t * spd * 1.1) + sin(length(q) * 1.4 - t * spd * 1.5);
    if (VARIANT == 1 || VARIANT == 3) v += 0.6 * sin(q.x * 2.3 - q.y * 1.8 + t * 2.0);
    return v * 0.25;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    p *= 1.0 + 0.12 * (syn_BassLevel * response) + 0.10 * (syn_OnBeat * response);
    float v = plasmaVal(p, t);
    float e = 0.02;
    vec2 grad = vec2(plasmaVal(p + vec2(e, 0.0), t) - v, plasmaVal(p + vec2(0.0, e), t) - v) / e;
    vec2 refrUV = clamp(uv + grad * ((0.015 + 0.025 * (syn_BassLevel * response)) * sceneRefractionDepth), 0.0, 1.0);

    vec3 col = vec3(0.0);
    if (VARIANT == 0) {
        col = pal(v * 0.6 + t * 0.05);
    } else if (VARIANT == 1) {
        col = pal3(v * 1.3 + t * 0.12, vec3(0.5), vec3(0.5), vec3(0.15, 0.65, 0.92));
    } else if (VARIANT == 2) {
        float ridge = sceneRamp(-0.2, 0.65, v);
        col = mix(vec3(0.06, 0.015, 0.01), vec3(1.0, 0.35, 0.03), ridge) + vec3(1.0, 0.85, 0.25) * pow(ridge, 3.0);
    } else {
        float arc = exp(-abs(v) * (9.0 - 4.0 * (syn_HighLevel * response)));
        col = pal(v * 0.5 + 0.2) * 0.35 + vec3(0.45, 0.92, 1.0) * arc * (1.2 + (syn_OnBeat * response));
    }
    vec4 img = sampleMediaChroma(refrUV, 0.012 * (syn_HighLevel * response));
    col = mix(col, img.rgb * (0.9 + 0.3 * (syn_OnBeat * response)), img.a);
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

// Family 16: Hex Pulse Grid — Swirl
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
uniform float sceneCellDensity;
uniform float scenePulseStrength;

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

vec4 hexCoords(vec2 uv) {
    vec2 r = vec2(1.0, 1.7320508);
    vec2 h = r * 0.5;
    vec2 a = mod(uv, r) - h;
    vec2 b = mod(uv - h, r) - h;
    vec2 gv = dot(a, a) < dot(b, b) ? a : b;
    float d = max(abs(gv.x), dot(abs(gv), normalize(r)));
    vec2 id = uv - gv;
    return vec4(gv.x, gv.y, id.x, id.y);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 hp = (p * (6.5 + 1.5 * (syn_BassLevel * response))) * sceneCellDensity;
    if (VARIANT == 3) {
        float r = length(p);
        hp = rot2(exp(-r * 1.6) * sin(t * 1.2) * (1.2 + (syn_MidLevel * response))) * hp;
    }
    vec4 hx = hexCoords(hp);
    vec2 gv = hx.xy;
    vec2 id = hx.zw;
    float edgeD = 0.5 - max(abs(gv.x), dot(abs(gv), vec2(0.5, 0.8660254)));

    float distCenter = length(id) * 0.25;
    float wave = 0.5 + 0.5 * sin(distCenter * 3.8 - t * 3.6 - (syn_OnBeat * response) * 2.0);
    float pulse = (pow(wave, 2.0) * (0.5 + 0.8 * (syn_Hits * response) + 0.6 * (syn_OnBeat * response))) * scenePulseStrength;

    vec3 cellCol = pal(distCenter * 0.25 + t * 0.08);
    vec4 img = sampleMedia(uv);
    vec3 fillCol = mix(cellCol * (0.25 + 0.85 * pulse), img.rgb * (0.6 + 0.6 * pulse), img.a);

    if (VARIANT == 0 || VARIANT == 3) {
        float cap = sceneRamp(0.02, 0.06, edgeD);
        return mix(vec3(0.02, 0.02, 0.05), fillCol, cap) + cellCol * sceneRamp(0.07, 0.0, edgeD) * 0.5 * (syn_OnBeat * response);
    } else if (VARIANT == 1) {
        float wire = sceneRamp(0.055, 0.015, edgeD);
        return mix(fillCol * 0.18, cellCol * (1.1 + 0.9 * (syn_OnBeat * response)), wire);
    } else {
        float ang = atan(gv.y, gv.x + 1e-6);
        float shade = ang > 0.5236 && ang < 2.618 ? 1.15 : (gv.x < 0.0 ? 0.68 : 0.40);
        float cap = sceneRamp(0.01, 0.04, edgeD);
        return mix(vec3(0.02), fillCol * shade, cap) + cellCol * sceneRamp(0.03, 0.0, edgeD) * 0.35;
    }
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

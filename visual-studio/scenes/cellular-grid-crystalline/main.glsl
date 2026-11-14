// Family 45: Cellular Grid — Crystalline
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
uniform float sceneCellBrightness;

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
    vec2 grid = (uv * vec2(32.0, 20.0)) * sceneCellDensity;
    vec2 id = floor(grid), gv = fract(grid) - 0.5;
    float gen = floor(t * 2.5 + (syn_OnBeat * response) * 1.5);
    float alive = 0.0;

    if (VARIANT == 1) {
        uint row = 1u << 16u;
        row |= (uint(gen) * 2654435761u) & 0x00100100u;
        int targetY = int(clamp(19.0 - id.y, 0.0, 23.0));
        uint activeRow = row;
        for (int r = 0; r < 24; ++r) {
            if (r == targetY) activeRow = row;
            uint l = (row >> 1u) | ((row & 1u) << 31u);
            uint ri = (row << 1u) | ((row >> 31u) & 1u);
            row = l ^ (row | ri);
        }
        uint bit = (activeRow >> uint(clamp(id.x, 0.0, 31.0))) & 1u;
        alive = float(bit);
    } else if (VARIANT == 0) {
        float c0 = step(0.55, hash21(id + gen * 0.13));
        float nb = step(0.55, hash21(id + vec2(1.0, 0.0) + gen * 0.13)) +
                   step(0.55, hash21(id + vec2(-1.0, 0.0) + gen * 0.13)) +
                   step(0.55, hash21(id + vec2(0.0, 1.0) + gen * 0.13)) +
                   step(0.55, hash21(id + vec2(0.0, -1.0) + gen * 0.13));
        alive = (c0 > 0.5 && (nb == 2.0 || nb == 3.0)) || (c0 < 0.5 && nb == 3.0) ? 1.0 : 0.0;
    } else if (VARIANT == 2) {
        float h = hash21(id + floor(t * 0.7) * 0.21);
        alive = step(0.38, sin(id.x * 1.5 + h * 3.14) * cos(id.y * 1.5 - h * 3.14));
    } else {
        float d = abs(id.x - 16.0) + abs(id.y - 10.0);
        alive = step(0.45, fract(d * 0.22 - t * 0.65 - 0.3 * (syn_OnBeat * response)));
    }

    float cellBox = sceneRamp(0.46, 0.38, max(abs(gv.x), abs(gv.y)));
    vec3 col = pal((id.x + id.y) * 0.04 + t * 0.06) * alive * cellBox * sceneCellBrightness * (0.85 + 0.65 * (syn_OnBeat * response));
    vec4 img = sampleMedia(uv);
    col = mix(col + vec3(0.015, 0.02, 0.04), img.rgb * (0.25 + 0.95 * alive), img.a);
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

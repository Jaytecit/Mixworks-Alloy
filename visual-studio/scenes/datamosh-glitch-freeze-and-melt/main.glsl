// Family 10: Datamosh Glitch — Freeze and Melt
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
uniform float sceneBlockScale;
uniform float sceneGlitchDepth;

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

vec3 basePattern(vec2 u, float t) {
    vec2 p = u - 0.5;
    float g = 0.5 + 0.5 * cos(length(p) * 28.0 - t * 3.0);
    vec3 col = pal(u.x * 0.5 + u.y * 0.3 + g * 0.25);
    col *= 0.45 + 0.55 * step(0.12, fract(u.x * 8.0 + u.y * 4.0));
    vec4 img = sampleMedia(u);
    return mix(col, img.rgb, img.a);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float drop = (clamp(max((syn_OnBeat * response), (syn_BassHits * response)) + 0.35 * (syn_BassLevel * response), 0.0, 1.3)) * sceneGlitchDepth;
    float qTime = floor(t * 7.0);
    vec3 col = vec3(0.0);

    if (VARIANT == 0) {
        vec2 blk = floor(uv * vec2(18.0, 11.0));
        float rnd = hash21(blk + qTime);
        vec2 mv = (hash22(blk + qTime * 1.3) - 0.5) * 0.22 * drop * step(0.55 - 0.25 * drop, rnd);
        col = basePattern(fract(uv + mv), t);
        if (rnd > 0.88 - 0.15 * (syn_OnBeat * response)) col = pal(rnd + t * 0.2) * 1.1;
    } else if (VARIANT == 1) {
        float row = floor(uv.y * (46.0 * sceneBlockScale));
        float rnd = hash21(vec2(row, qTime));
        float tear = (rnd - 0.5) * 0.28 * drop * step(0.52, rnd);
        vec2 u2 = vec2(fract(uv.x + tear), uv.y);
        col = basePattern(u2, t);
        col.r = basePattern(fract(u2 + vec2(0.025 * (syn_HighLevel * response), 0.0)), t).r;
    } else if (VARIANT == 2) {
        vec2 blk = floor(uv * (vec2(14.0, 9.0) * sceneBlockScale));
        float rnd = hash21(blk + qTime);
        vec2 off = (hash22(blk) - 0.5) * 0.12 * drop;
        col = basePattern(fract(uv + off), t);
        if (rnd > 0.65) col = col.gbr;
        else if (rnd > 0.42 && (syn_OnBeat * response) > 0.3) col = col.brg;
    } else {
        vec2 blk = floor(uv * vec2(22.0 * sceneBlockScale, 1.0));
        float rnd = hash21(blk + floor(t * 2.5));
        float melt = (0.04 + 0.24 * drop) * rnd;
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 6; ++i) {
            float fi = float(i) / 5.0;
            acc += basePattern(vec2(uv.x, clamp(uv.y + fi * melt, 0.0, 1.0)), t - fi * 0.25);
        }
        col = acc / 6.0;
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

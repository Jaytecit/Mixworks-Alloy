// Family 23: Truchet Maze — Triangles
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
uniform float sceneTileScale;
uniform float sceneTraceWidth;

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
    vec2 q = p * ((5.5 + 1.5 * (syn_BassLevel * response)) * sceneTileScale) + vec2(t * 0.4, t * 0.25);
    vec2 id = floor(q);
    vec2 gv = fract(q) - 0.5;
    float flipBeat = floor(t * 1.5 + (syn_OnBeat * response) * 0.6);
    float rnd = hash21(id + flipBeat * 0.37);
    if (rnd > 0.5) gv.x = -gv.x;

    float d = 1.0;
    float d2 = 1.0;
    if (VARIANT == 0 || VARIANT == 3) {
        float s = (gv.x + gv.y > 0.0) ? 1.0 : -1.0;
        d = abs(length(gv - 0.5 * s) - 0.5);
        d2 = abs(length(gv + 0.5 * s) - 0.5);
    } else if (VARIANT == 1) {
        d = min(abs(gv.x + gv.y - 0.5), abs(gv.x + gv.y + 0.5)) * 0.7071;
    } else {
        float tri = step(0.0, gv.x + gv.y);
        d = abs(gv.x + gv.y) * 0.7071;
        d2 = tri;
    }

    float line = sceneRamp(0.11 * sceneTraceWidth, 0.03 * sceneTraceWidth, d);
    if (VARIANT == 3) {
        float under = sceneRamp(0.11 * sceneTraceWidth, 0.03 * sceneTraceWidth, d2) * sceneRamp(0.13, 0.20, d);
        line = max(line, under * 0.85);
    }

    vec3 bg = pal(hash21(id) * 0.4 + t * 0.05) * (VARIANT == 2 ? (0.18 + 0.25 * d2) : 0.14);
    vec4 img = sampleMedia(uv);
    bg = mix(bg, img.rgb * (0.55 + 0.5 * (syn_Level * response)), img.a * 0.85);
    vec3 glow = pal(d * 2.0 + length(id) * 0.1 + t * 0.1) * (1.0 + 0.8 * (syn_OnBeat * response));
    return mix(bg, glow, line) + pal(0.25) * exp(-d * 18.0) * 0.35;
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

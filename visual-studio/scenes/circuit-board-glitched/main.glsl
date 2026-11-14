// Family 24: Circuit Board — Glitched
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
uniform float sceneTraceDensity;
uniform float sceneGlitchDisplacement;

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
    float sc = ((VARIANT == 0) ? 11.0 : (VARIANT == 1 ? 5.0 : 8.0)) * sceneTraceDensity;
    vec2 qp = p;
    if (VARIANT == 3 && (syn_OnBeat * response) > 0.25) {
        qp.x += sceneGlitchDisplacement * (hash21(vec2(floor(p.y * 18.0), floor(t * 10.0))) - 0.5) * 0.12 * (syn_OnBeat * response);
    }
    vec2 q = qp * sc;
    if (VARIANT == 2) {
        float r = max(abs(qp.x), abs(qp.y));
        if (r < 0.22) q = qp * 18.0;
    }
    vec2 id = floor(q), gv = fract(q) - 0.5;
    float r = hash21(id);
    if (r > 0.5) gv.x = -gv.x;

    float trace = min(abs(gv.x), abs(gv.y));
    float diag = abs(gv.x - gv.y) * 0.7071;
    float path = (r > 0.25) ? min(trace, diag + 0.12) : trace;
    float lineW = (VARIANT == 1) ? 0.09 : 0.055;
    float mask = sceneRamp(lineW, lineW * 0.35, path);
    float via = sceneRamp(0.14, 0.08, length(gv));

    float pulse = pow(0.5 + 0.5 * sin((id.x + id.y) * 1.4 - t * 5.0 - (syn_OnBeat * response) * 2.0), 4.0);
    vec3 pcb = vec3(0.015, 0.04, 0.055);
    vec4 img = sampleMedia(uv);
    pcb = mix(pcb, img.rgb * 0.75, img.a);

    vec3 traceCol = pal(r * 0.5 + t * 0.06) * (0.55 + 0.95 * pulse * (0.7 + (syn_OnBeat * response)));
    vec3 col = mix(pcb, traceCol, max(mask, via));
    if (VARIANT == 2) {
        float chipBorder = exp(-abs(max(abs(p.x), abs(p.y)) - 0.22) * 55.0);
        col += pal(0.2) * chipBorder * (0.8 + (syn_OnBeat * response));
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

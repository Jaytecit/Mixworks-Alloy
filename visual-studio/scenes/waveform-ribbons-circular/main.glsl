// Family 14: Waveform Ribbons — Circular
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
uniform float sceneRibbonWidth;
uniform float sceneWaveDepth;

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

float sdBox2(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdBox3(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
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

float synthSpec(float x) {
    float b = clamp(x, 0.0, 1.0);
    float dm = (b - 0.45) * 3.4;
    float dh = (b - 0.85) * 4.2;
    float low = (syn_BassLevel * response) * exp(-b * 4.5);
    float mid = (syn_MidLevel * response) * exp(-(dm * dm));
    float hi = (syn_HighLevel * response) * exp(-(dh * dh));
    float rip = 0.5 + 0.5 * sin(b * 26.0 - TIME * 4.2 + (syn_MidLevel * response) * 3.0);
    return clamp((low + mid + hi) * (0.65 + 0.35 * rip) + 0.14 * (syn_Level * response) + 0.18 * (syn_OnBeat * response) * (1.0 - b * 0.5), 0.04, 1.25);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = vec3(0.02, 0.025, 0.06) + pal(p.x * 0.2 + t * 0.04) * 0.08 * (1.0 - abs(p.y));
    float grid = sceneRamp(0.96, 1.0, max(abs(sin(p.x * 20.0)), abs(sin(p.y * 20.0))));
    col += vec3(0.08, 0.12, 0.22) * grid;

    if (VARIANT == 2) {
        float minD = 1e5;
        float bestS = 0.0;
        vec2 prev = vec2(0.0);
        for (int i = 0; i <= 48; ++i) {
            float s = float(i) / 48.0;
            float a = s * 6.2831853;
            vec2 cur = sceneWaveDepth * vec2(0.62 * sin(3.0 * a + t * 1.2 + (syn_MidLevel * response)), 0.38 * sin(4.0 * a - t * 0.9)) * (0.85 + 0.25 * (syn_BassLevel * response));
            if (i > 0) {
                float d = sdSeg(p, prev, cur);
                if (d < minD) { minD = d; bestS = s; }
            }
            prev = cur;
        }
        float w = (0.032 + 0.02 * (syn_BassLevel * response)) * sceneRibbonWidth;
        float mask = sceneRamp(w, w * 0.35, minD);
        vec4 img = sampleMedia(vec2(bestS, clamp(minD / w, 0.0, 1.0)));
        vec3 rib = mix(pal(bestS + t * 0.1), img.rgb, img.a);
        col = mix(col, rib * (1.0 + 0.6 * (syn_OnBeat * response)), mask);
        col += pal(bestS) * exp(-minD * 30.0) * (0.4 + 0.5 * (syn_OnBeat * response));
        return col;
    }

    if (VARIANT == 3) {
        float ang = atan(p.y, p.x + 1e-6);
        float na = fract(ang / 6.2831853 + 0.5);
        float amp = synthSpec(abs(na - 0.5) * 2.0);
        float targetR = 0.26 + (0.14 * sceneWaveDepth) * sin(ang * 4.0 - t * 3.0) * amp + 0.08 * (syn_BassLevel * response);
        float d = abs(length(p) - targetR);
        float w = (0.038 + 0.02 * (syn_OnBeat * response)) * sceneRibbonWidth;
        vec4 img = sampleMedia(vec2(na, clamp((length(p) - targetR) / (2.0 * w) + 0.5, 0.0, 1.0)));
        vec3 rib = mix(pal(na + t * 0.1), img.rgb, img.a);
        col = mix(col, rib, sceneRamp(w, w * 0.3, d));
        col += pal(na) * exp(-d * 34.0) * (0.5 + 0.6 * (syn_OnBeat * response));
        return col;
    }

    int count = (VARIANT == 0) ? 1 : 3;
    for (int k = 0; k < 3; ++k) {
        if (k >= count) break;
        float fk = float(k);
        float amp = synthSpec(clamp(uv.x, 0.0, 1.0));
        float yWave = sceneWaveDepth * (0.18 + 0.12 * amp) * sin(p.x * (5.5 + fk * 1.6) - t * (2.4 + fk * 0.7) + fk * 2.1);
        float d = abs(p.y - yWave);
        float w = ((VARIANT == 0) ? (0.065 + 0.035 * (syn_BassLevel * response)) : 0.035) * sceneRibbonWidth;
        vec4 img = sampleMedia(vec2(uv.x, clamp((p.y - yWave) / (2.0 * w) + 0.5, 0.0, 1.0)));
        vec3 rib = mix(pal(uv.x * 0.5 + fk * 0.28 + t * 0.1), img.rgb, img.a);
        col = mix(col, rib * (0.95 + 0.55 * (syn_OnBeat * response)), sceneRamp(w, w * 0.25, d));
        col += pal(fk * 0.3 + 0.2) * exp(-d * 38.0) * 0.45;
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

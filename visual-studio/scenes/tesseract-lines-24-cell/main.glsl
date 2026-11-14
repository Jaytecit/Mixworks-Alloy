// Family 32: Tesseract Lines — 24-Cell
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
uniform float scene4dRotationDepth;
uniform float sceneLineGlow;

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

vec2 proj4D(vec4 v, float t, out float wVal) {
    v.xw = rot2(t * 0.65 + 0.6 * (syn_MidLevel * response)) * v.xw;
    v.yz = rot2(t * 0.45) * v.yz;
    v.zw = rot2(t * 0.50) * v.zw;
    wVal = v.w;
    float wScale = 1.65 / max(2.2 - v.w, 0.5);
    vec3 v3 = v.xyz * wScale;
    float zScale = 1.65 / max(3.0 - v3.z, 0.6);
    return v3.xy * zScale * (0.85 + 0.15 * (syn_BassLevel * response));
}

float tesseractDist(vec2 p, float t, out float sliceGlow) {
    float minD = 1e5;
    sliceGlow = 0.0;
    for (int i = 0; i < 32; ++i) {
        int axis = i / 8;
        int m = i - axis * 8;
        float b0 = (m & 1) == 0 ? -0.5 : 0.5;
        float b1 = (m & 2) == 0 ? -0.5 : 0.5;
        float b2 = (m & 4) == 0 ? -0.5 : 0.5;
        vec4 a = vec4(b0, b1, b2, -0.5), b = vec4(b0, b1, b2, 0.5);
        if (axis == 0) { a = a.wxyz; b = b.wxyz; }
        else if (axis == 1) { a = a.xwyz; b = b.xwyz; }
        else if (axis == 2) { a = a.xywz; b = b.xywz; }
        float wa = 0.0, wb = 0.0;
        float d = sdSeg(p, proj4D(a, t, wa), proj4D(b, t, wb));
        minD = min(minD, d);
        if (VARIANT == 2) sliceGlow += exp(-d * 48.0) * exp(-abs(wa + wb) * 3.0);
    }
    if (VARIANT == 1) {
        for (int a = 0; a < 8; ++a) {
            vec4 p16 = vec4(0.0);
            float s = (a % 2 == 0) ? 1.0 : -1.0;
            int ax = a / 2;
            if (ax == 0) p16.x = s; else if (ax == 1) p16.y = s; else if (ax == 2) p16.z = s; else p16.w = s;
            float w1 = 0.0;
            vec2 q1 = proj4D(p16, t, w1);
            for (int c = 0; c < 8; ++c) {
                vec4 h = vec4((c & 1) == 0 ? -0.5 : 0.5, (c & 2) == 0 ? -0.5 : 0.5, (c & 4) == 0 ? -0.5 : 0.5, 0.5 * s);
                if (ax == 0) h = h.wxyz; else if (ax == 1) h = h.xwyz; else if (ax == 2) h = h.xywz;
                float w2 = 0.0;
                minD = min(minD, sdSeg(p, q1, proj4D(h, t, w2)));
            }
        }
    }
    return minD;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = pal(length(p) * 0.35 + t * 0.05) * 0.14;
    vec4 img = sampleMedia(rot2(0.25 * sin(t * 0.5)) * p * 1.5 + 0.5);
    col = mix(col, img.rgb * 0.85, img.a * sceneRamp(0.42, 0.12, length(p)));

    float sliceG = 0.0;
    float d = tesseractDist(p, t * scene4dRotationDepth, sliceG);
    float wire = (sceneRamp(0.015, 0.003, d) + exp(-d * 34.0) * 0.7) * sceneLineGlow;
    col += pal(d * 5.0 + t * 0.15) * wire * (0.95 + 0.75 * (syn_OnBeat * response));

    if (VARIANT == 2) col += vec3(1.0, 0.9, 0.6) * sliceG * 0.55;
    if (VARIANT == 3) {
        for (int k = 1; k <= 3; ++k) {
            float fk = float(k);
            float dEcho = tesseractDist(p, (t - fk * 0.09) * scene4dRotationDepth, sliceG);
            col += pal(0.4 + fk * 0.15) * exp(-dEcho * 38.0) * (0.45 / fk);
        }
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

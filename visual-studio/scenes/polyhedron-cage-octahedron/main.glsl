// Family 26: Polyhedron Cage — Octahedron
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
uniform float sceneCageScale;
uniform float sceneLineWidth;

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

vec2 proj3D(vec3 v, float scale) {
    float z = v.z + 2.9 - 0.35 * (syn_BassLevel * response);
    return (v.xy / max(z, 0.4)) * scale;
}

vec3 rot3D(vec3 v, float t) {
    v.xz = rot2(t * 0.75 + (syn_MidLevel * response) * 0.8) * v.xz;
    v.yz = rot2(t * 0.55) * v.yz;
    return v;
}

float drawCube(vec2 p, float t, float sc) {
    float d = 1e5;
    for (int i = 0; i < 12; ++i) {
        int axis = i / 4;
        int m = i - axis * 4;
        float s1 = (m == 1 || m == 3) ? 1.0 : -1.0;
        float s2 = (m >= 2) ? 1.0 : -1.0;
        vec3 a = vec3(s1, s2, -1.0), b = vec3(s1, s2, 1.0);
        if (axis == 0) { a = a.zxy; b = b.zxy; }
        else if (axis == 1) { a = a.xzy; b = b.xzy; }
        d = min(d, sdSeg(p, proj3D(rot3D(a, t), sc), proj3D(rot3D(b, t), sc)));
    }
    return d;
}

float drawOcta(vec2 p, float t, float sc) {
    const vec3 V[6] = vec3[6](
        vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1)
    );
    float d = 1e5;
    for (int i = 0; i < 2; ++i)
        for (int j = 2; j < 6; ++j)
            d = min(d, sdSeg(p, proj3D(rot3D(V[i], t), sc), proj3D(rot3D(V[j], t), sc)));
    for (int j = 2; j < 4; ++j)
        for (int k = 4; k < 6; ++k)
            d = min(d, sdSeg(p, proj3D(rot3D(V[j], t), sc), proj3D(rot3D(V[k], t), sc)));
    return d;
}

float drawIco(vec2 p, float t, float sc) {
    const float phi = 1.618034;
    const vec3 V[12] = vec3[12](
        vec3(-1, phi, 0), vec3(1, phi, 0), vec3(-1, -phi, 0), vec3(1, -phi, 0),
        vec3(0, -1, phi), vec3(0, 1, phi), vec3(0, -1, -phi), vec3(0, 1, -phi),
        vec3(phi, 0, -1), vec3(phi, 0, 1), vec3(-phi, 0, -1), vec3(-phi, 0, 1)
    );
    float d = 1e5;
    for (int i = 0; i < 12; ++i) {
        for (int j = i + 1; j < 12; ++j) {
            if (length(V[i] - V[j]) < 2.1) {
                d = min(d, sdSeg(p, proj3D(rot3D(V[i] * 0.65, t), sc), proj3D(rot3D(V[j] * 0.65, t), sc)));
            }
        }
    }
    return d;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = pal(length(p) * 0.3 + t * 0.05) * 0.14;
    float sc = (1.25 + 0.18 * (syn_BassLevel * response)) * sceneCageScale;
    float d = 1e5;
    if (VARIANT == 0) d = drawCube(p, t, sc);
    else if (VARIANT == 1) d = drawOcta(p, t, sc);
    else if (VARIANT == 2) d = drawIco(p, t, sc);
    else d = min(drawCube(p, t, sc), drawOcta(p, -t * 1.4, sc * 0.72));

    vec2 mUV = rot2(0.3 * sin(t * 0.5)) * p * 1.4 + 0.5;
    vec4 img = sampleMedia(mUV);
    float faceMask = sceneRamp(0.45, 0.15, length(p));
    col = mix(col, img.rgb * (0.75 + 0.35 * (syn_OnBeat * response)), img.a * faceMask);

    float wire = sceneRamp(0.016 * sceneLineWidth, 0.004 * sceneLineWidth, d);
    float glow = exp(-d * 38.0);
    vec3 wireCol = pal(d * 4.0 + t * 0.15) * (1.0 + 0.9 * (syn_OnBeat * response));
    return col + wireCol * (wire + 0.65 * glow);
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

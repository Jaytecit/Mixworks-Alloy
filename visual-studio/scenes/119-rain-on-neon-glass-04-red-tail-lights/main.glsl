// Family 119: Rain on Neon Glass — Red Tail-Lights
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
uniform float sceneDropDensity;
uniform float sceneGlassRefraction;

out vec4 fragColor;

const int VARIANT = 3;

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
    vec2 m = smoothstep(0.0, 0.008, uv) * (1.0 - smoothstep(0.992, 1.0, uv));
    vec4 c = texture(syn_Media, clamp(uv, 0.0, 1.0));
    c.a *= m.x * m.y;
    return c;
}
vec4 sampleMediaTile3D(vec2 uv, float dist) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    float lod = clamp(log2(max(dist * 0.35, 1.0)), 0.0, 4.0);
    return textureLod(syn_Media, fract(uv), lod);
}
vec4 sampleTriplanar(vec3 pos, vec3 n, float sc, float dist) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec3 w = abs(n);
    w = w * w * w;
    w /= max(w.x + w.y + w.z, 1e-4);
    vec4 cx = sampleMediaTile3D(pos.zy * sc, dist);
    vec4 cy = sampleMediaTile3D(pos.xz * sc, dist);
    vec4 cz = sampleMediaTile3D(pos.xy * sc, dist);
    return cx * w.x + cy * w.y + cz * w.z;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 sc = ((VARIANT == 1) ? vec2(10.0, 6.0) : vec2(18.0, 10.0)) * sceneDropDensity;
    vec2 q = uv * sc;
    q.y += t * (VARIANT == 2 ? 3.4 : 1.8);
    vec2 id = floor(q), gv = fract(q) - 0.5;
    float rnd = hash21(id);
    vec2 dropPos = vec2((rnd - 0.5) * 0.4, sin(t * 2.5 + rnd * 6.28) * 0.25);
    float d = length((gv - dropPos) * vec2(1.0, 0.55));
    float drop = smoothstep(0.24, 0.08, d) * step(VARIANT == 1 ? 0.65 : 0.35, rnd);
    vec2 n = (gv - dropPos) * drop;

    vec2 bUV = clamp(uv - n * (0.14 * sceneGlassRefraction), 0.0, 1.0);
    float bokeh = 0.0;
    vec3 neon = vec3(0.0);
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        vec2 lp = vec2(sin(fi * 1.9 + t * 0.3) * 0.65, cos(fi * 2.3) * 0.32);
        float bd = smoothstep(0.16, 0.05, length(p - n * (0.18 * sceneGlassRefraction) - lp));
        vec3 lc = (VARIANT == 3) ? vec3(1.0, 0.12, 0.08) : pal(fi * 0.18 + t * 0.05);
        neon += lc * bd * (0.7 + 0.5 * (syn_OnBeat * response));
    }
    vec4 img = sampleMedia(bUV);
    return mix(vec3(0.02, 0.025, 0.05) + neon, img.rgb * (0.7 + 0.5 * drop), img.a) + vec3(0.6, 0.8, 1.0) * drop * 0.15;
}



void main() {
    vec2 res = max(RENDERSIZE, vec2(1.0));
    vec2 uv = gl_FragCoord.xy / res;
    vec2 p = (gl_FragCoord.xy - 0.5 * res) / res.y;
    float t = motion * 1.35;
    vec3 col = renderScene(uv, p, t);
    col *= clamp(1.0 - 0.24 * dot(p, p), 0.28, 1.0);
    col = col / (vec3(1.0) + 0.18 * col);
    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}

// Family 126: Terracotta Relief — Wave Frieze
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
uniform float sceneReliefDepth;
uniform float sceneRakingLight;

out vec4 fragColor;

const int VARIANT = 1;

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

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0)), d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
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

float reliefH(vec2 p, float t) {
    float r = length(p), a = atan(p.y, p.x + 1e-6);
    if (VARIANT == 1) return 0.5 + 0.5 * sin(p.x * 14.0 + sin(p.y * 10.0 + t)) * cos(p.y * 8.0);
    if (VARIANT == 2) return step(0.45, fract(max(abs(p.x), abs(p.y)) * 8.0 - 0.1 * sin(a * 4.0 + t)));
    return 0.5 + 0.5 * cos(r * 24.0 - t * 1.5) * cos(a * 12.0);
}
vec3 renderScene(vec2 uv, vec2 p, float t) {
    float e = 0.008;
    float h = reliefH(p, t);
    vec4 img = sampleMedia(uv);
    h = mix(h, dot(img.rgb, vec3(0.3, 0.59, 0.11)), img.a * 0.6);
    float hx = reliefH(p + vec2(e, 0.0), t);
    float hy = reliefH(p + vec2(0.0, e), t);
    vec3 n = normalize(vec3((h - hx) * (2.5 * sceneReliefDepth), (h - hy) * (2.5 * sceneReliefDepth), 0.18));
    float diff = max(dot(n, normalize(vec3(0.6, 0.7 / sceneRakingLight, 0.5))), 0.15);
    vec3 clay = (VARIANT == 3) ? vec3(0.45, 0.22, 0.14) : vec3(0.82, 0.44, 0.26);
    clay = mix(clay, pal(h * 0.5 + 0.1), 0.2 + 0.25 * (syn_OnBeat * response));
    return mix(clay, img.rgb, img.a * 0.7) * diff;
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

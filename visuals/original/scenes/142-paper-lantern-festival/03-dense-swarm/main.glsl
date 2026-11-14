// Family 142: Paper Lantern Festival — Dense Swarm
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

out vec4 fragColor;

const int VARIANT = 2;

vec3 pal3(float t, vec3 a, vec3 b, vec3 d) {
    return clamp(a + b * cos(6.2831853 * (t + d)), 0.0, 1.0);
}
vec3 pal(float t) {
    return pal3(t + 0.14 * syn_MidLevel, vec3(0.48, 0.44, 0.54), vec3(0.50, 0.48, 0.45), vec3(0.55, 0.08, 0.85));
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

float sdBox2(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdBox3(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}
float sdCyl(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
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

float starLayer(vec2 uv, float thresh) {
    vec2 id = floor(uv), gv = fract(uv) - 0.5;
    float r = hash21(id);
    if (r < thresh) return 0.0;
    vec2 off = vec2(hash21(id + 11.3), hash21(id + 57.9)) - 0.5;
    float d = length(gv - off * 0.55);
    float rad = 0.04 + 0.06 * fract(r * 41.0);
    return smoothstep(rad, 0.0, d) * (0.65 + 0.35 * sin(TIME * 2.5 + r * 62.83));
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = vec3(0.01, 0.015, 0.045) + vec3(0.8, 0.9, 1.0) * starLayer(p * 18.0, 0.88);
    vec4 img = sampleMedia(uv);
    col = mix(col, img.rgb * 0.55, img.a);

    for (int i = 0; i < 16; ++i) {
        float fi = float(i);
        float z = fract(fi * 0.0625);
        vec2 lp = vec2((hash21(vec2(fi, 1.7)) - 0.5) * 1.5, fract(hash21(vec2(fi, 5.3)) + t * (0.08 + 0.08 * z)) - 0.5);
        if (VARIANT == 1) lp.y = -0.15 - 0.25 * z;
        float sz = mix(0.025, 0.075, z);
        float d = sdBox2(p - lp, vec2(sz * 0.75, sz));
        float glow = exp(-max(d, 0.0) * 42.0);
        vec3 lCol = (VARIANT == 3) ? vec3(1.0, 0.22, 0.12) : vec3(1.0, 0.65, 0.22);
        col += lCol * glow * (0.7 + 0.5 * syn_OnBeat);
    }
    return col;
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

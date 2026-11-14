// Family 101: Kintsugi Fracture — Celadon Crack
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

const int VARIANT = 1;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 q = p * (3.8 + 0.6 * syn_BassLevel) + (VARIANT == 3 ? vec2(sin(p.y * 5.0 + t) * 0.2, 0.0) : vec2(0.0));
    vec2 n = floor(q), f = fract(q);
    float d1 = 8.0, d2 = 8.0;
    vec2 bestID = vec2(0.0);
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = 0.5 + 0.42 * sin(t * 0.35 + 6.2831 * hash22(n + g));
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; bestID = n + g; }
            else if (d < d2) d2 = d;
        }
    }
    float seam = (sqrt(d2) - sqrt(d1)) + 0.06 * (vnoise(p * 18.0) - 0.5);
    float goldMask = smoothstep(0.075 + 0.03 * syn_OnBeat, 0.015, seam);
    vec3 ceramic = (VARIANT == 1) ? vec3(0.56, 0.76, 0.70) : (VARIANT == 2 ? vec3(0.06, 0.06, 0.08) : vec3(0.88, 0.85, 0.80));
    ceramic *= 0.78 + 0.22 * hash21(bestID);
    vec4 img = sampleMedia(clamp(uv + (hash22(bestID) - 0.5) * 0.06, 0.0, 1.0));
    ceramic = mix(ceramic, img.rgb, img.a);
    vec3 gold = mix(vec3(0.92, 0.65, 0.14), vec3(1.0, 0.94, 0.58), 0.5 + 0.5 * sin(p.x * 10.0 + t * 2.0)) * (1.0 + 0.7 * syn_OnBeat);
    return mix(ceramic, gold, goldMask);
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

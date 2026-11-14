// Family 114: Caustic Pool Floor — Sunset Shallows
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

mat2 rot2(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

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

float causticNet(vec2 u, float t) {
    vec2 p = mod(u * 6.2831853, 6.2831853) - 250.0;
    vec2 i = p;
    float c = 1.0;
    for (int n = 0; n < 4; ++n) {
        float nt = t * (1.0 - (3.5 / float(n + 1)));
        i = p + vec2(cos(nt - i.x) + sin(nt + i.y), sin(nt - i.y) + cos(nt + i.x));
        c += 1.0 / max(length(vec2(p.x / (sin(i.x + nt) / 0.005), p.y / (cos(i.y + nt) / 0.005))), 1e-4);
    }
    return pow(clamp(1.17 - pow(c / 4.0, 1.4), 0.0, 1.0), 6.0);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec2 warp = vec2(sin(p.y * 8.0 + t * 1.8), cos(p.x * 8.0 - t * 1.5)) * (0.018 + 0.025 * syn_BassLevel);
    float c = causticNet(p * 0.85 + 0.5, t * 0.8);
    if (VARIANT == 3) c += 0.5 * exp(-abs(fract(length(p * 4.0) - t * 1.2) - 0.5) * 18.0);

    vec2 tile = abs(fract((p + warp) * 7.0) - 0.5);
    float grout = smoothstep(0.46, 0.49, max(tile.x, tile.y));
    vec3 waterCol = (VARIANT == 2) ? vec3(0.95, 0.52, 0.28) : (VARIANT == 1 ? vec3(0.04, 0.18, 0.35) : vec3(0.08, 0.62, 0.78));
    vec3 col = mix(waterCol, waterCol * 0.55, grout);
    vec4 img = sampleMedia(clamp(uv + warp, 0.0, 1.0));
    col = mix(col, img.rgb, img.a);
    return col + mix(vec3(0.7, 0.95, 1.0), pal(0.2), 0.35) * c * (0.85 + 0.7 * syn_OnBeat);
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

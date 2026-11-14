// Family 93: Volumetric Cloud Tunnel — Sunset Clouds
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
uniform float sceneCloudDensity;
uniform float sceneDepthTravel;

out vec4 fragColor;

const int VARIANT = 1;

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

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    mat2 m = mat2(0.80, -0.60, 0.60, 0.80);
    for (int i = 0; i < 4; ++i) {
        v += a * vnoise(p);
        p = m * p * 2.02 + vec2(17.1, 31.7);
        a *= 0.5;
    }
    return v;
}

vec4 sampleMedia(vec2 uv) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = smoothstep(0.0, 0.008, uv) * smoothstep(1.0, 0.992, uv);
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
    vec2 rp = rot2(0.15 * sin(t * 0.5) + 0.15 * (syn_MidLevel * response)) * p;
    vec3 col = vec3(0.02, 0.02, 0.05);
    float r = length(rp);
    float a = atan(rp.y, rp.x + 1e-6);

    vec3 acc = vec3(0.0);
    for (int i = 0; i < 16; ++i) {
        float fi = float(i) / 16.0;
        float z = fract(fi - t * (0.35 * sceneDepthTravel));
        float sc = mix(0.6, 5.5, z);
        float twist = (VARIANT == 3) ? z * 3.0 : 0.0;
        vec2 cp = rot2(twist) * rp * sc;
        float n = fbm(cp + vec2(0.0, t * 0.4 + fi * 3.0));
        float ring = smoothstep(0.25, 0.85, length(cp) * 0.45) * smoothstep(0.25, 0.78, n + 0.2 * (syn_BassLevel * response));
        vec3 cCol = (VARIANT == 1)
            ? mix(vec3(0.95, 0.32, 0.18), vec3(1.0, 0.85, 0.45), n)
            : (VARIANT == 2 ? pal3(n + z, vec3(0.4), vec3(0.5), vec3(0.2, 0.65, 0.45)) : pal(n + z * 0.6 + t * 0.06));
        if (VARIANT == 0) cCol *= 0.7 + 0.9 * (syn_OnBeat * response) * step(0.62, n);
        acc += cCol * ring * sceneCloudDensity * smoothstep(0.0, 0.25, z) * smoothstep(1.0, 0.75, z) * 0.16;
    }

    vec4 img = sampleMedia(uv);
    col = mix(col + acc, acc + img.rgb * (0.4 + 0.8 * clamp(length(acc), 0.0, 1.0)), img.a);
    return col + pal(a / 6.2831853 + t * 0.08) * exp(-r * 6.0) * (0.4 + 0.6 * (syn_OnBeat * response));
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

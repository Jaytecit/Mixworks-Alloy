// Family 108: Paper Cut Diorama — Moonlit Arch
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
uniform float sceneParallaxDepth;
uniform float sceneLayerSeparation;

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
    vec3 col = mix(pal(0.25) * 0.85, vec3(0.98, 0.88, 0.72), smoothstep(0.6, -0.2, p.y));
    float moon = smoothstep(0.16, 0.14, length(p - vec2(0.0, 0.22)));
    col = mix(col, vec3(1.0, 0.96, 0.85), moon);
    vec4 img = sampleMedia(uv);
    col = mix(col, img.rgb, img.a * 0.8);

    for (int i = 0; i < 5; ++i) {
        float fi = float(i);
        float par = ((fi + 1.0) * 0.28) * sceneParallaxDepth;
        float x = p.x * (1.2 + fi * 0.3) + t * 0.25 * par;
        float h = -0.35 + fi * (0.06 * sceneLayerSeparation);
        if (VARIANT == 0) h += 0.22 * abs(sin(x * 4.5 + fi)) + 0.1 * vnoise(vec2(x * 8.0, fi));
        else if (VARIANT == 1) h += 0.35 * fbm(vec2(x * 1.4, fi * 2.3)) - 0.1;
        else if (VARIANT == 2) h += 0.26 * floor(hash21(vec2(floor(x * 4.0), fi)) * 4.0) * 0.25;
        else {
            float archX = p.x + (sceneParallaxDepth - 1.0) * (t * 0.25 * (fi + 1.0) * 0.28);
            h = 0.32 - sqrt(max(0.45 - archX * archX, 0.0)) * (0.6 - fi * (0.08 * sceneLayerSeparation));
        }
        h += 0.04 * (syn_BassLevel * response) * sin(fi + t * 2.0);
        float layerMask = smoothstep(h + 0.008, h - 0.008, p.y);
        vec3 paperCol = mix(vec3(0.12, 0.10, 0.20), pal(fi * 0.18 + 0.3) * 0.35, fi / 4.0);
        float rim = exp(-abs(p.y - h) * 55.0) * (0.4 + 0.6 * (syn_OnBeat * response));
        col = mix(col, paperCol, layerMask) + pal(0.15) * rim;
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

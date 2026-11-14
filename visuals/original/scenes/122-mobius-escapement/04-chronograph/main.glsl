// Family 122: Mobius Escapement — Chronograph
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

const int VARIANT = 3;

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

float gearWheel(vec2 p, float r, float teeth, float rot) {
    vec2 rp = rot2(rot) * p;
    float a = atan(rp.y, rp.x + 1e-6);
    float d = length(rp) - (r + 0.03 * smoothstep(0.2, 0.6, cos(a * teeth)));
    float rim = max(d, -(length(rp) - r * 0.68));
    float spoke = max(min(abs(rp.x), abs(rp.y)) - 0.015, length(rp) - r * 0.72);
    return smoothstep(0.012, 0.0, min(rim, spoke));
}
vec3 renderScene(vec2 uv, vec2 p, float t) {
    float tick = floor(t * 2.0 + syn_OnBeat * 0.5) * 0.26 + fract(t * 2.0) * 0.04;
    vec3 col = vec3(0.02, 0.02, 0.04);
    vec4 img = sampleMedia(uv);
    col = mix(col, img.rgb * 0.65, img.a);

    float g = gearWheel(p, 0.34, 18.0, tick);
    if (VARIANT == 1) g = max(gearWheel(p - vec2(-0.24, 0.0), 0.22, 12.0, tick), gearWheel(p - vec2(0.24, 0.0), 0.22, 12.0, -tick));
    else if (VARIANT == 2 || VARIANT == 3) g = max(g, gearWheel(p - vec2(0.18, 0.14), 0.14, 10.0, -tick * 1.8));
    vec3 brass = mix(vec3(0.88, 0.72, 0.32), pal(0.25 + t * 0.04), 0.35);
    return col + brass * g * (0.9 + 0.6 * syn_OnBeat);
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

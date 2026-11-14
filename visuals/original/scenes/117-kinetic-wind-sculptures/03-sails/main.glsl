// Family 117: Kinetic Wind Sculptures — Sails
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
    vec2 q = p * (VARIANT == 0 ? 4.5 : 3.2);
    vec2 id = floor(q), gv = fract(q) - 0.5;
    float phase = t * 1.5 + (id.x + id.y) * 0.45 + syn_MidLevel * 1.5;
    vec2 gp = rot2(phase) * gv;
    float a = atan(gp.y, gp.x + 1e-6);
    float r = length(gp);
    float blades = (VARIANT == 3) ? 6.0 : (VARIANT == 2 ? 3.0 : 5.0);
    float fin = exp(-abs(sin(a * blades + (VARIANT == 1 ? r * 8.0 : 0.0))) * 10.0) * smoothstep(0.46, 0.06, r);
    vec3 col = pal(id.x * 0.12 + a / 6.2831853 + t * 0.06) * (0.12 + 0.95 * fin * (0.85 + 0.6 * syn_OnBeat));
    vec4 img = sampleMedia(uv);
    return mix(col, img.rgb * (0.4 + 0.8 * fin), img.a);
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

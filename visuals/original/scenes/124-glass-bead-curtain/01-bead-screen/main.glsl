// Family 124: Glass Bead Curtain — Bead Screen
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

const int VARIANT = 0;

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
    vec2 qp = p * vec2(12.0, 9.0);
    if (VARIANT == 1) qp.x += 0.35 * sin(qp.y * 0.5 + t * 1.5);
    if (VARIANT == 3) qp.y += t * 2.0;
    vec2 id = floor(qp), gv = fract(qp) - 0.5;
    float r = length(gv);
    float rad = 0.42 + 0.06 * syn_BassLevel;
    float bead = 1.0 - smoothstep(rad - 0.06, rad, r);
    vec3 n = vec3(gv / rad, sqrt(max(1.0 - dot(gv, gv) / (rad * rad), 0.0)));
    vec2 refrUV = clamp(uv - n.xy * 0.09 * bead, 0.0, 1.0);
    vec3 bg = pal(id.x * 0.07 + id.y * 0.05 + t * 0.06) * (0.25 + 0.75 * bead);
    vec2 dispersion = n.xy * bead * (VARIANT == 2 ? 0.025 : 0.008) * (0.5 + syn_HighLevel);
    vec4 img = sampleMedia(refrUV);
    img.r = sampleMedia(refrUV + dispersion).r;
    img.b = sampleMedia(refrUV - dispersion).b;
    vec3 col = mix(bg, img.rgb, img.a);
    float spec = pow(max(dot(n, normalize(vec3(-0.4, 0.6, 0.7))), 0.0), 18.0) * bead;
    return col + vec3(1.0) * spec * (0.8 + 0.5 * syn_OnBeat);
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

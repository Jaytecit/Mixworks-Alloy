// Family 125: Acoustic Levitation — Standing Column
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

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1e-4), 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
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

vec3 camRay(vec2 p, vec3 ro, vec3 ta, float roll, float fov) {
    vec3 cw = normalize(ta - ro);
    vec3 up = vec3(sin(roll), cos(roll), 0.0);
    if (abs(dot(cw, up)) > 0.995) up = vec3(0.0, 0.0, 1.0);
    vec3 cu = normalize(cross(cw, up));
    vec3 cv = cross(cu, cw);
    return normalize(p.x * cu + p.y * cv + fov * cw);
}

void getCamera(float t, out vec3 ro, out vec3 ta, out float roll, out float fov) {
    float a = t * 0.42;
    ro = vec3(2.8 * cos(a), 0.45 * sin(t * 0.5), 2.8 * sin(a));
    ta = vec3(0.0);
    roll = 0.1 * sin(t * 0.3);
    fov = 1.25;
}
vec2 mapScene(vec3 q, float t) {
    float d = 1e5;
    for (int i = -2; i <= 2; ++i) {
        float fi = float(i);
        vec3 c = vec3(0.0, fi * 0.55 + 0.06 * sin(t * 4.0 + fi) * syn_BassLevel, 0.0);
        if (VARIANT == 2) c.xz = 0.45 * vec2(cos(t + fi), sin(t + fi));
        float r = 0.20 + (VARIANT == 3 ? 0.08 * syn_OnBeat : 0.04 * syn_BassLevel);
        d = smin(d, length(q - c) - r, 0.08);
        if (VARIANT == 1) d = smin(d, length(q - vec3(0.85, c.y, 0.0)) - r * 0.8, 0.08);
    }
    return vec2(d, length(q));
}
vec3 getSky(vec3 rd, float t) {
    float wave = exp(-abs(sin(rd.y * 18.0)) * 4.0);
    return vec3(0.015, 0.02, 0.05) + pal(0.2) * wave * (0.15 + 0.25 * syn_OnBeat);
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 2.5);
    vec3 col = pal(pos.y * 0.7 + t * 0.08) * (0.35 + 0.85 * fres) * (0.9 + 0.6 * syn_OnBeat);
    vec2 artworkUV = vec2(0.5 + dot(pos, normalize(cross(rd, vec3(0.0, 1.0, 0.0)))) * 0.52,
                          0.5 + pos.y * 0.36);
    vec4 img = sampleMedia(artworkUV);
    return mix(col, img.rgb * (0.65 + 0.45 * fres), img.a);
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0045;
    vec2 k = vec2(1.0, -1.0);
    return normalize(
        k.xyy * mapScene(p + k.xyy * e, t).x +
        k.yyx * mapScene(p + k.yyx * e, t).x +
        k.yxy * mapScene(p + k.yxy * e, t).x +
        k.xxx * mapScene(p + k.xxx * e, t).x
    );
}
vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.0), ta = vec3(0.0, 0.0, 1.0);
    float roll = 0.0, fov = 1.2;
    getCamera(t, ro, ta, roll, fov);
    vec3 rd = camRay(p, ro, ta, roll, fov);
    float d = 0.0;
    vec2 hit = vec2(0.0);
    bool ok = false;
    for (int i = 0; i < 48; ++i) {
        hit = mapScene(ro + rd * d, t);
        if (hit.x < 0.003) { ok = true; break; }
        if (d > 30.0) break;
        d += hit.x * 0.72;
    }
    vec3 sky = getSky(rd, t);
    if (!ok || d > 30.0) return sky;
    vec3 pos = ro + rd * d;
    vec3 n = calcNormal(pos, t);
    vec3 mat = shadeHit(pos, n, rd, hit.y, d, t);
    float fog = exp(-0.048 * d);
    return mix(sky, mat, fog);
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

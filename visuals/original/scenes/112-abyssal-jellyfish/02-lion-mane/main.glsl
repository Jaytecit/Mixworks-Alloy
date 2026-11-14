// Family 112: Abyssal Jellyfish — Lion Mane
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

vec3 camRay(vec2 p, vec3 ro, vec3 ta, float roll, float fov) {
    vec3 cw = normalize(ta - ro);
    vec3 up = vec3(sin(roll), cos(roll), 0.0);
    if (abs(dot(cw, up)) > 0.995) up = vec3(0.0, 0.0, 1.0);
    vec3 cu = normalize(cross(cw, up));
    vec3 cv = cross(cu, cw);
    return normalize(p.x * cu + p.y * cv + fov * cw);
}

void getCamera(float t, out vec3 ro, out vec3 ta, out float roll, out float fov) {
    float a = t * 0.35;
    ro = vec3(2.8 * cos(a), 0.35 * sin(t * 0.6), 2.8 * sin(a));
    ta = vec3(0.0, -0.2, 0.0);
    roll = 0.1 * sin(t * 0.3);
    fov = 1.25;
}
vec2 mapScene(vec3 q, float t) {
    vec3 p = q;
    if (VARIANT == 3) p = mod(q + vec3(1.5, t * 0.8, 1.5), 3.0) - 1.5;
    float pulse = sin(t * 2.8 + syn_BassLevel * 2.0);
    p.y -= 0.18 * pulse;
    vec3 bp = p;
    bp.xz *= 1.0 + 0.16 * pulse;
    float bell = max(length(bp * vec3(1.0, 1.45, 1.0)) - 0.72, -bp.y - 0.1);
    bell = max(bell, -(length(bp * vec3(1.0, 1.45, 1.0) - vec3(0.0, -0.08, 0.0)) - 0.58));

    float tent = 1e5;
    for (int i = 0; i < 6; ++i) {
        float a = float(i) * 1.0472;
        vec2 off = vec2(cos(a), sin(a)) * (VARIANT == 1 ? 0.38 : 0.25) + 0.12 * vec2(sin(p.y * 3.5 - t * 3.0 + a), cos(p.y * 3.0 - t * 2.5));
        float td = max(length(p.xz - off) - 0.04, max(p.y - 0.0, -1.55 - p.y));
        tent = min(tent, td);
    }
    float d = smin(bell, tent, 0.08);
    return vec2(d * 0.75, bell < tent ? 1.0 : 2.0);
}
vec3 getSky(vec3 rd, float t) {
    return vec3(0.005, 0.02, 0.06);
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 2.2);
    float band = (VARIANT == 2) ? (0.5 + 0.5 * sin(atan(pos.z, pos.x + 1e-5) * 8.0 + pos.y * 12.0 - t * 6.0)) : 0.6;
    vec3 bio = pal(matID * 0.3 + pos.y * 0.4 + t * 0.08) * (0.4 + 1.1 * fres) * band * (0.9 + 0.7 * syn_OnBeat);
    vec2 bellUV = vec2(atan(pos.z, pos.x + 1e-5) / 6.2831853 + 0.5 + t * 0.035,
                      (pos.y + 0.32) * 0.82 + 0.35);
    vec4 img = sampleMedia(vec2(fract(bellUV.x), bellUV.y));
    vec4 lens = sampleMedia(vec2(dot(pos, normalize(cross(rd, vec3(0.0, 1.0, 0.0)))) * 0.62 + 0.5,
                                 pos.y * 0.55 + 0.46));
    vec3 tint = mix(bio, img.rgb * (0.5 + 0.7 * fres), img.a);
    return mix(tint, lens.rgb * (0.65 + 0.35 * fres), lens.a * 0.68);
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

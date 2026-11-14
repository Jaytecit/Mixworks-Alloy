// Family 99: Horizon Torii Gates — Ascending Steps
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
float sdOcta(vec3 p, float s) {
    p = abs(p);
    return (p.x + p.y + p.z - s) * 0.57735027;
}
float sdHexPrism(vec3 p, vec2 h) {
    const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
    p = abs(p);
    p.xy -= 2.0 * min(dot(k.xy, p.xy), 0.0) * k.xy;
    vec2 d = vec2(length(p.xy - vec2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x), p.z - h.y);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h) - r;
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

vec3 camRay(vec2 p, vec3 ro, vec3 ta, float roll, float fov) {
    vec3 cw = normalize(ta - ro);
    vec3 up = vec3(sin(roll), cos(roll), 0.0);
    if (abs(dot(cw, up)) > 0.995) up = vec3(0.0, 0.0, 1.0);
    vec3 cu = normalize(cross(cw, up));
    vec3 cv = cross(cu, cw);
    return normalize(p.x * cu + p.y * cv + fov * cw);
}

float pathX(float z) { return (VARIANT == 1) ? 1.2 * sin(z * 0.25) : 0.0; }
float pathY(float z) { return (VARIANT == 2) ? z * 0.22 : 0.0; }
void getCamera(float t, out vec3 ro, out vec3 ta, out float roll, out float fov) {
    float z = t * 2.6;
    ro = vec3(pathX(z), 0.15 + pathY(z), z);
    ta = vec3(pathX(z + 3.0), 0.25 + pathY(z + 3.0), z + 3.0);
    roll = 0.08 * sin(t * 0.4);
    fov = 1.18;
}
vec2 mapScene(vec3 q, float t) {
    float fl = q.y + 1.0 - pathY(q.z);
    vec3 gp = vec3(q.x - pathX(q.z), q.y - pathY(q.z), mod(q.z, 2.4) - 1.2);
    float posts = sdBox3(vec3(abs(gp.x) - 0.95, gp.y, gp.z), vec3(0.11, 1.2, 0.11));
    float lintel = min(sdBox3(gp - vec3(0.0, 1.05, 0.0), vec3(1.35, 0.10, 0.14)), sdBox3(gp - vec3(0.0, 0.72, 0.0), vec3(1.10, 0.08, 0.10)));
    float gate = min(posts, lintel);
    float d = min(fl, gate);
    return vec2(d * 0.85, d == fl ? 0.0 : 1.0);
}
vec3 getSky(vec3 rd, float t) {
    return pal(rd.y * 0.6 + 0.15) * (0.28 + 0.3 * syn_OnBeat);
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    if (matID < 0.5) {
        float w = (VARIANT == 3) ? (0.4 + 0.6 * pow(1.0 - max(dot(n, -rd), 0.0), 2.5)) : 0.35;
        vec2 g = abs(fract(pos.xz * 0.8) - 0.5);
        return vec3(0.02, 0.02, 0.05) + pal(0.2) * smoothstep(0.06, 0.01, min(g.x, g.y)) * w * (0.7 + 0.6 * syn_OnBeat);
    }
    float diff = max(dot(n, normalize(vec3(0.4, 0.8, -0.5))), 0.2);
    vec3 col = mix(vec3(0.95, 0.16, 0.12), pal(pos.z * 0.08 + t * 0.05), 0.45) * diff * (0.9 + 0.5 * syn_OnBeat);
    vec4 img = sampleTriplanar(pos, n, 0.5, dist);
    return mix(col, img.rgb, img.a);
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

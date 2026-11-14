// Family 56: Dyson Bridge Span — Double Deck
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
uniform float cameraSweep;
uniform float sceneDeckWidth;

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

void getCamera(float t, out vec3 ro, out vec3 ta, out float roll, out float fov) {
    ro = vec3(0.9 * sin(t * 0.55), 0.75 + 0.3 * sin(t * 0.4), t * 3.2);
    ta = vec3(0.0, 0.45, t * 3.2 + 4.0);
    roll = -0.18 * cos(t * 0.55);
    fov = 1.18;
}
vec2 mapScene(vec3 q, float t) {
    vec3 p = q;
    float gap = (VARIANT == 3) ? step(0.72, hash21(vec2(floor(q.z * 0.25), 2.0))) : 0.0;
    float deck = sdBox3(vec3(p.x, p.y, mod(p.z, 2.0) - 1.0), vec3(1.35 * sceneDeckWidth, 0.14, 1.0 - 0.5 * gap));
    if (VARIANT == 2) deck = min(deck, sdBox3(vec3(p.x, p.y + 1.1, mod(p.z, 2.0) - 1.0), vec3(1.15, 0.12, 1.0)));
    vec3 tp = vec3(abs(p.x) - 1.45, p.y - 1.1, mod(p.z, 6.0) - 3.0);
    float tower = sdBox3(tp, vec3(0.22, 2.5, 0.28));
    if (VARIANT == 0) {
        float cable = sdCapsule(vec3(abs(p.x), p.y, mod(p.z, 1.5) - 0.75), vec3(1.4, 2.5, 0.0), vec3(1.2, 0.1, 0.7), 0.04);
        tower = min(tower, cable);
    } else if (VARIANT == 1) {
        float arch = sdTorus(vec3(p.y - 0.2, mod(p.z, 6.0) - 3.0, abs(p.x) - 1.35), vec2(2.6, 0.15));
        tower = min(tower, arch);
    }
    float abyss = q.y + 3.2 + 0.3 * sin(q.z * 0.4 + t * 2.0);
    float d = min(min(deck, tower), abyss);
    return vec2(d, d == deck ? 2.0 : (d == abyss ? 0.0 : 1.0));
}
vec3 getSky(vec3 rd, float t) {
    return pal(rd.y * 0.6 + 0.2) * (0.3 + 0.4 * (syn_OnBeat * response));
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float diff = max(dot(n, normalize(vec3(0.5, 0.9, -0.3))), 0.15);
    vec3 base = pal(matID * 0.35 + pos.z * 0.05 + t * 0.05) * diff;
    vec4 img = sampleTriplanar(pos, n, 0.4, dist);
    base = mix(base, img.rgb, img.a);
    return base + pal(0.2) * exp(-abs(pos.y) * 5.0) * 0.35 * (syn_OnBeat * response);
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
    if (cameraSweep < 1.0) {
        vec3 fixedEye, fixedTarget;
        float fixedRoll, fixedFov;
        getCamera(0.0, fixedEye, fixedTarget, fixedRoll, fixedFov);
        ro = mix(fixedEye, ro, cameraSweep);
        ta = mix(fixedTarget, ta, cameraSweep);
        roll = mix(fixedRoll, roll, cameraSweep);
        fov = mix(fixedFov, fov, cameraSweep);
    }
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

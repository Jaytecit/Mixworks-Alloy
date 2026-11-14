// Family 100: Singularity Core — Event Horizon
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
uniform float sceneAccretionGlow;

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

float starLayer(vec2 uv, float thresh) {
    vec2 id = floor(uv), gv = fract(uv) - 0.5;
    float r = hash21(id);
    if (r < thresh) return 0.0;
    vec2 off = vec2(hash21(id + 11.3), hash21(id + 57.9)) - 0.5;
    float d = length(gv - off * 0.55);
    float rad = 0.04 + 0.06 * fract(r * 41.0);
    return smoothstep(rad, 0.0, d) * (0.65 + 0.35 * sin(TIME * 2.5 + r * 62.83));
}

void getCamera(float t, out vec3 ro, out vec3 ta, out float roll, out float fov) {
    float a = t * 0.48;
    ro = vec3(3.1 * cos(a), 1.15 * sin(t * 0.5), 3.1 * sin(a));
    ta = vec3(0.0);
    roll = 0.28 * sin(t * 0.4) + 0.2 * (t == 0.0 && cameraSweep == 0.0 ? 0.0 : syn_MidLevel * response);
    fov = 1.25;
}
vec2 mapScene(vec3 q, float t) {
    float core = length(q) - (0.62 + 0.12 * (syn_BassLevel * response));
    vec3 p = q;
    p.xz = rot2(t * 0.9 - length(q.xz) * 0.8) * p.xz;
    float ring = sdTorus(p, vec2(1.45, 0.10 + 0.04 * (syn_OnBeat * response)));
    if (VARIANT == 0) ring = min(ring, abs(p.y + 0.45 / max(length(p.xz), 0.4)) - 0.05);
    else if (VARIANT == 1) ring = min(ring, min(sdTorus(p.xzy, vec2(1.45, 0.08)), sdTorus(p.zyx, vec2(1.45, 0.08))));
    else if (VARIANT == 2) ring = min(ring, sdCyl(p, 2.4, 0.16 + 0.08 * (syn_OnBeat * response)));
    float d = min(core, ring * 0.75);
    return vec2(d, d == core ? 0.0 : 1.0);
}
vec3 getSky(vec3 rd, float t) {
    return vec3(0.01, 0.01, 0.03) + vec3(0.85, 0.92, 1.0) * starLayer(rd.xy * 14.0 + rd.z * 5.0, 0.87);
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    if (matID < 0.5) return pal(0.25) * fres * (1.2 + 1.2 * (syn_OnBeat * response));
    vec3 col = pal(atan(pos.z, pos.x + 1e-5) / 6.2831853 + t * 0.15) * (0.9 + 0.8 * (syn_OnBeat * response)) * sceneAccretionGlow;
    vec4 img = sampleTriplanar(pos, n, 0.55, dist);
    return mix(col, img.rgb * 1.2, img.a);
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

// Family 61: Mirror Pyramid Plaza — Floating Apex
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
uniform float sceneReflectionStrength;

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
    float a = t * 0.42;
    ro = vec3(4.2 * cos(a), 0.7 + 0.55 * sin(t * 0.5), 4.2 * sin(a));
    ta = vec3(0.0, 0.35, 0.0);
    roll = 0.08 * sin(t * 0.4);
    fov = 1.25;
}
vec2 mapScene(vec3 q, float t) {
    float fl = q.y + 1.0;
    float mon = 1e5;
    if (VARIANT == 0 || VARIANT == 3) {
        mon = sdOcta(q + vec3(0.0, 0.2, 0.0), 2.35);
        if (VARIANT == 3) {
            float cut = q.y - 0.45;
            vec3 top = q - vec3(0.0, 0.35 + 0.3 * sin(t * 2.0) + 0.25 * (syn_BassLevel * response), 0.0);
            mon = min(max(mon, cut), max(sdOcta(top + vec3(0.0, 0.2, 0.0), 2.35), -cut));
        }
    } else if (VARIANT == 1) {
        vec3 p = vec3(abs(q.x) - 1.2, q.y, q.z);
        mon = sdBox3(p - vec3(0.0, 0.5, 0.0), vec3(0.38, 1.7 + 0.3 * (syn_BassLevel * response), 0.38));
    } else {
        mon = min(sdBox3(q + vec3(0.0, 0.6, 0.0), vec3(1.8, 0.4, 1.8)), min(sdBox3(q + vec3(0.0, 0.0, 0.0), vec3(1.2, 0.4, 1.2)), sdBox3(q - vec3(0.0, 0.6, 0.0), vec3(0.6, 0.4, 0.6))));
    }
    float d = min(fl, mon);
    return vec2(d, d == fl ? 0.0 : 1.0);
}
vec3 getSky(vec3 rd, float t) {
    return pal(rd.y * 0.7 + 0.2) * (0.35 + 0.3 * (syn_OnBeat * response));
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    if (matID < 0.5) {
        vec2 g = abs(fract(pos.xz * 0.8) - 0.5);
        float grid = smoothstep(0.06, 0.01, min(g.x, g.y));
        return vec3(0.02, 0.025, 0.06) + pal(0.15) * (grid + fres * 0.5) * (0.7 + 0.6 * (syn_OnBeat * response));
    }
    vec3 ref = reflect(rd, n);
    vec3 env = pal(ref.y * 0.6 + ref.x * 0.3 + t * 0.08);
    vec4 img = sampleTriplanar(pos, n, 0.45, dist);
    return mix(env * (0.4 + 0.7 * fres) * sceneReflectionStrength, img.rgb, img.a) + pal(0.3) * fres * (syn_OnBeat * response);
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

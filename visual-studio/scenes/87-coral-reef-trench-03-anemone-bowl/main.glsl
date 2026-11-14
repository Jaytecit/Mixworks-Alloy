// Family 87: Coral Reef Trench — Anemone Bowl
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
uniform float sceneCoralScale;

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

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1e-4), 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
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
    ro = vec3(0.75 * sin(t * 0.45), 0.45 + 0.2 * sin(t * 0.6), t * 1.8);
    ta = vec3(0.4 * sin(t * 0.45 + 0.6), 0.2, t * 1.8 + 3.0);
    roll = 0.12 * sin(t * 0.35);
    fov = 1.15;
}
vec2 mapScene(vec3 q, float t) {
    float n = fbm(q.xz * 0.65);
    float floorD = (q.y + 1.2 - n * 0.8 - (VARIANT == 3 ? smoothstep(0.6, 2.2, abs(q.x)) * 2.0 : 0.0)) * 0.65;
    vec3 p = vec3(mod(q.x + 1.4, 2.8) - 1.4, q.y + 0.85, mod(q.z, 2.6) - 1.3);
    vec3 coralPoint = p / sceneCoralScale;
    float coral = 1e5;
    if (VARIANT == 1) {
        coral = length(coralPoint.xz + 0.2 * sin(p.y * 4.0 + t)) - (0.22 - p.y * 0.1);
        coral = max(coral, abs(p.y) - 1.1);
    } else if (VARIANT == 2) {
        coral = length(coralPoint - vec3(0.0, 0.2, 0.0)) - (0.65 + 0.12 * sin(p.x * 12.0 + t * 3.0) * sin(p.z * 12.0));
    } else {
        coral = length(coralPoint) - (0.65 + 0.18 * sin(p.x * 8.0) * cos(p.y * 8.0) + 0.12 * (syn_BassLevel * response));
    }
    float d = smin(floorD, coral * 0.7, 0.22);
    return vec2(d, coral < floorD ? 2.0 + n : 1.0);
}
vec3 getSky(vec3 rd, float t) {
    return mix(vec3(0.02, 0.18, 0.32), vec3(0.01, 0.04, 0.12), clamp(1.0 - rd.y, 0.0, 1.0));
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float diff = max(dot(n, normalize(vec3(0.3, 0.9, -0.3))), 0.18);
    vec3 col = pal(matID * 0.45 + pos.y * 0.3 + t * 0.05) * diff * (0.9 + 0.6 * (syn_OnBeat * response));
    vec4 img = sampleTriplanar(pos, n, 0.55, dist);
    return mix(col, img.rgb * (0.4 + 0.7 * diff), img.a);
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

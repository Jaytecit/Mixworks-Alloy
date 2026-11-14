// Family 85: Volcanic Rift — Caldera Rim
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
    ro = vec3(0.45 * sin(t * 0.5), 0.75 + 0.25 * sin(t * 0.7), t * 2.5);
    ta = vec3(0.3 * sin(t * 0.5 + 0.6), 0.15, t * 2.5 + 3.8);
    roll = 0.12 * cos(t * 0.5);
    fov = 1.15;
}
vec2 mapScene(vec3 q, float t) {
    float river = q.y + 1.05 + 0.08 * sin(q.z * 2.5 - t * 3.0);
    float wallDist = smoothstep(0.4, 2.4, abs(q.x - 0.45 * sin(q.z * 0.25)));
    float n = fbm(q.xz * 0.55);
    if (VARIANT == 2) n = floor(n * 6.0) / 6.0;
    float cliffH = wallDist * (2.6 + n * 1.8) - 1.25;
    if (VARIANT == 1) cliffH = (abs(length(q.xz - vec2(0.0, t * 2.5 + 4.0)) - 2.4)) * 0.9 - 1.1;
    if (VARIANT == 3) cliffH += 0.5 * exp(-abs(q.x)) * sin(q.z * 1.2 - t * 4.0) * syn_BassLevel;
    float cliff = (q.y - cliffH) * 0.6;
    float d = min(river, cliff);
    return vec2(d, d == river ? 0.0 : 1.0 + n);
}
vec3 getSky(vec3 rd, float t) {
    return mix(vec3(0.55, 0.14, 0.04), vec3(0.04, 0.02, 0.05), clamp(rd.y * 2.2, 0.0, 1.0));
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    if (matID < 0.5) {
        float flow = fbm(pos.xz * 1.4 - vec2(0.0, t * 1.4));
        vec3 lava = mix(vec3(0.25, 0.02, 0.01), vec3(1.0, 0.45, 0.04), flow) + vec3(1.0, 0.9, 0.3) * pow(flow, 3.0) * (0.8 + 0.8 * syn_OnBeat);
        vec4 img = sampleMediaTile3D(pos.xz * 0.45, dist);
        return mix(lava, img.rgb, img.a);
    }
    float diff = max(dot(n, normalize(vec3(0.0, -0.5, 0.5))), 0.12);
    vec3 rock = vec3(0.09, 0.07, 0.08) * diff + vec3(0.85, 0.22, 0.02) * exp(-abs(pos.y + 1.0) * 2.5) * (0.6 + 0.6 * syn_BassLevel);
    vec4 img = sampleTriplanar(pos, n, 0.4, dist);
    return mix(rock, img.rgb, img.a);
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

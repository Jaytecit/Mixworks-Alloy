// Family 73: Ribbed Torus Interior — Segmented Hull
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

const int VARIANT = 3;

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
    float R = 3.2;
    float a = t * 0.45, a2 = a + 0.35;
    ro = vec3(R * cos(a), 0.15 * sin(t * 1.2), R * sin(a));
    ta = vec3(R * cos(a2), 0.0, R * sin(a2));
    roll = 0.45 + 0.25 * sin(t * 0.6) + 0.2 * syn_MidLevel;
    fov = 1.08;
}
vec2 mapScene(vec3 q, float t) {
    float R = 3.2;
    float a = atan(q.z, q.x + 1e-5);
    vec2 tp = vec2(length(q.xz) - R, q.y);
    float b = atan(tp.y, tp.x + 1e-5);
    float tubeR = 1.15 - 0.14 * syn_BassLevel;
    if (VARIANT == 1) tubeR -= 0.08 * smoothstep(0.5, 0.9, cos(a * 32.0));
    else if (VARIANT == 2) tubeR -= 0.08 * cos(a * 24.0 + b * 4.0);
    else if (VARIANT == 3) tubeR -= 0.07 * max(abs(cos(a * 18.0)), abs(cos(b * 6.0)));
    return vec2((tubeR - length(tp)) * 0.7, a / 6.2831853 + 0.5);
}
vec3 getSky(vec3 rd, float t) {
    return pal(t * 0.1) * 0.2;
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    float a = atan(pos.z, pos.x + 1e-5) / 6.2831853 + 0.5;
    vec2 tp = vec2(length(pos.xz) - 3.2, pos.y);
    float b = atan(tp.y, tp.x + 1e-5) / 6.2831853 + 0.5;
    vec3 col = pal(a * 3.0 + b + t * 0.06) * (0.35 + 0.75 * max(dot(n, -rd), 0.15));
    vec4 img = sampleMediaTile3D(vec2(a * 12.0, b * 3.0), dist);
    return mix(col, img.rgb, img.a) + pal(0.25) * pow(1.0 - max(dot(n, -rd), 0.0), 2.5) * 0.65 * syn_OnBeat;
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

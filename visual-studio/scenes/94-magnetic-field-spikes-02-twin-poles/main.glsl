// Family 94: Magnetic Field Spikes — Twin Poles
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
uniform float sceneSpikeHeight;

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
    return pal3(t + 0.14 * (syn_MidLevel * response), vec3(0.48, 0.44, 0.54), vec3(0.50, 0.48, 0.45), vec3(0.55, 0.08, 0.85));
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
    float a = t * 0.48;
    ro = (VARIANT == 2) ? vec3(0.5 * sin(t), 0.3, 2.8) : vec3(2.6 * cos(a), 0.85 * sin(t * 0.55), 2.6 * sin(a));
    ta = vec3(0.0);
    roll = 0.18 * sin(t * 0.4);
    fov = 1.28;
}
vec2 mapScene(vec3 q, float t) {
    float d = 1e5;
    float spikeAmp = (0.22 + 0.26 * (syn_BassLevel * response) + 0.18 * (syn_OnBeat * response)) * sceneSpikeHeight;
    if (VARIANT == 2) {
        float spikes = pow(max(sin(q.x * 7.0) * cos(q.y * 7.0), 0.0), 3.0) * spikeAmp * 2.5;
        d = abs(q.z + spikes) - 0.15;
    } else {
        vec3 p = q;
        if (VARIANT == 1) p.x = abs(p.x) - 0.85;
        float r = max(length(p), 1e-4);
        vec3 n = p / r;
        float spikes = pow(max(sin(n.x * 11.0 + t) * cos(n.y * 11.0) * sin(n.z * 11.0 - t), 0.0), 2.0) * spikeAmp;
        if (VARIANT == 3) spikes += 0.25 * exp(-abs(n.y) * 8.0) * (0.5 + 0.5 * sin(atan(n.z, n.x + 1e-5) * 16.0)) * spikeAmp;
        d = (r - (0.78 + spikes)) * 0.58;
    }
    return vec2(d, length(q));
}
vec3 getSky(vec3 rd, float t) {
    return pal(rd.y * 0.5 + 0.2) * 0.22;
}
vec3 shadeHit(vec3 pos, vec3 n, vec3 rd, float matID, float dist, float t) {
    vec3 r = reflect(rd, n);
    float spec = pow(max(dot(r, normalize(vec3(0.4, 0.9, -0.5))), 0.0), 24.0);
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    vec3 ferro = vec3(0.04, 0.05, 0.08) + pal(matID * 0.7 + t * 0.08) * fres * (0.8 + 0.8 * (syn_OnBeat * response)) + vec3(1.0) * spec;
    vec4 img = sampleTriplanar(pos, n, 0.6, dist);
    return mix(ferro, img.rgb, img.a);
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

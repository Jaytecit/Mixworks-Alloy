// Family 47: Ocean Surface — Neon Night
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
uniform float sceneSwellHeight;
uniform float sceneReflectionStrength;

out vec4 fragColor;

const int VARIANT = 2;

// Keep both ascending and descending fades defined in desktop GLSL.
float sceneRamp(float start, float end, float value) {
    if (start < end) return smoothstep(start, end, value);
    if (start > end) return 1.0 - smoothstep(end, start, value);
    return step(start, value);
}
vec2 sceneRamp(float start, float end, vec2 value) {
    return vec2(sceneRamp(start, end, value.x), sceneRamp(start, end, value.y));
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

vec4 sampleMedia(vec2 uv) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = sceneRamp(0.0, 0.008, uv) * sceneRamp(1.0, 0.992, uv);
    vec4 c = texture(syn_Media, clamp(uv, 0.0, 1.0));
    c.a *= m.x * m.y;
    return c;
}
vec4 sampleMediaTile(vec2 uv) {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    if (syn_MediaType <= 0.0) return vec4(0.0);
    return textureGrad(syn_Media, fract(uv), dx, dy);
}
vec4 sampleMediaChroma(vec2 uv, float amt) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = sceneRamp(0.0, 0.008, uv) * sceneRamp(1.0, 0.992, uv);
    vec4 c = texture(syn_Media, clamp(uv, 0.0, 1.0));
    float r = texture(syn_Media, clamp(uv + vec2(amt, 0.0), 0.0, 1.0)).r;
    float b = texture(syn_Media, clamp(uv - vec2(amt, 0.0), 0.0, 1.0)).b;
    return vec4(r, c.g, b, c.a * m.x * m.y);
}

float waveH(vec2 xz, float t) {
    float amp = ((VARIANT == 1) ? 0.45 : (VARIANT == 0 ? 0.18 : 0.26)) * sceneSwellHeight;
    float w = amp * sin(xz.x * 1.4 + t * 1.8) * cos(xz.y * 1.1 - t * 1.4);
    w += (VARIANT == 1 ? 0.30 : 0.12) * fbm(xz * 1.5 + vec2(0.0, t * 0.9));
    if (VARIANT == 3) {
        float zFront = 4.5 - mod(t * 1.4, 7.0);
        w += 0.95 * exp(-pow((xz.y - zFront) * 0.9, 2.0)) * (0.8 + 0.5 * (syn_BassLevel * response));
    }
    return w * (0.7 + 0.6 * (syn_BassLevel * response));
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.0, 1.4, 0.0);
    vec3 rd = normalize(vec3(p.x, p.y - 0.15, 1.2));
    vec3 sky = (VARIANT == 2)
        ? pal(rd.y * 0.8 + 0.15) * 0.25
        : mix(vec3(0.18, 0.26, 0.42), vec3(0.02, 0.04, 0.12), clamp(rd.y * 2.0, 0.0, 1.0));
    vec4 skyImg = sampleMedia(vec2(uv.x, clamp(uv.y * 1.2 - 0.2, 0.0, 1.0)));
    sky = mix(sky, skyImg.rgb, skyImg.a * sceneRamp(-0.05, 0.25, rd.y));
    if (rd.y > 0.02) return sky;

    float d = 1.0, hit = 0.0;
    for (int i = 0; i < 36; ++i) {
        vec3 pos = ro + rd * d;
        float h = waveH(pos.xz, t);
        if (pos.y < h) { hit = 1.0; break; }
        d += max((pos.y - h) * 0.6, 0.08);
        if (d > 24.0) break;
    }
    if (hit < 0.5) return sky;

    vec3 pos = ro + rd * d;
    float e = 0.06;
    vec3 n = normalize(vec3(waveH(pos.xz - vec2(e, 0.0), t) - waveH(pos.xz + vec2(e, 0.0), t), 2.0 * e, waveH(pos.xz - vec2(0.0, e), t) - waveH(pos.xz + vec2(0.0, e), t)));
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    vec2 reflUV = clamp(vec2(uv.x + n.x * 0.2, 1.0 - uv.y + n.z * 0.15), 0.0, 1.0);
    vec4 rImg = sampleMedia(reflUV);
    vec3 water = (VARIANT == 2)
        ? pal(pos.z * 0.15 + t * 0.1) * (0.2 + 0.9 * fres)
        : mix(vec3(0.01, 0.08, 0.16), vec3(0.25, 0.65, 0.85), fres);
    water = mix(water, rImg.rgb * (0.4 + 0.8 * fres * sceneReflectionStrength), clamp(rImg.a * sceneReflectionStrength, 0.0, 1.0));
    if (VARIANT == 1) water += vec3(0.85, 0.95, 1.0) * sceneRamp(0.25, 0.55, pos.y) * (0.5 + 0.6 * (syn_OnBeat * response));
    return mix(sky, water, exp(-0.065 * d));
}

void main() {
    vec2 res = max(RENDERSIZE, vec2(1.0));
    vec2 uv = gl_FragCoord.xy / res;
    vec2 p = (gl_FragCoord.xy - 0.5 * res) / res.y;
    float t = motion * 1.45;
    vec3 col = renderScene(uv, p, t);
    col *= clamp(1.0 - 0.25 * dot(p, p), 0.28, 1.0);
    col = col / (vec3(1.0) + 0.18 * col);
    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}

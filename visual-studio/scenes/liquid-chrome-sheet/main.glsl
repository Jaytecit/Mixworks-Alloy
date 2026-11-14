// Family 21: Liquid Chrome — Sheet
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
uniform float sceneSurfaceDeformation;
uniform float sceneReflectionStrength;

out vec4 fragColor;

const int VARIANT = 1;

// Keep both ascending and descending fades defined in desktop GLSL.
float sceneRamp(float start, float end, float value) {
    if (start < end) return smoothstep(start, end, value);
    if (start > end) return 1.0 - smoothstep(end, start, value);
    return step(start, value);
}
vec2 sceneRamp(float start, float end, vec2 value) {
    return vec2(sceneRamp(start, end, value.x), sceneRamp(start, end, value.y));
}

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

float mapChrome(vec3 q, float t) {
    float ripple = (0.05 * sin(q.x * 7.0 + t * 2.6) * cos(q.z * 7.0 - t * 2.1) * (0.6 + (syn_BassLevel * response))) * sceneSurfaceDeformation;
    if (VARIANT == 0) {
        return length(q) - (0.85 + 0.12 * (syn_BassLevel * response)) + ripple;
    } else if (VARIANT == 1) {
        vec3 p = q;
        p.yz = rot2(0.55) * p.yz;
        return abs(p.y) - 0.08 + (0.14 * sceneSurfaceDeformation) * sin(p.x * 3.5 + t * 2.2) * cos(p.z * 3.5 - t * 1.8);
    } else if (VARIANT == 2) {
        vec3 p = q;
        p.xy = rot2(t * 0.45) * p.xy;
        p.yz = rot2(0.65 + 0.3 * sin(t * 0.5)) * p.yz;
        vec2 t2 = vec2(length(p.xz) - (0.75 + 0.1 * (syn_BassLevel * response)), p.y);
        return length(t2) - 0.28 + ripple * 0.7;
    } else {
        float base = q.y + 0.55 + 0.14 * sin(length(q.xz) * 8.0 - t * 4.0) * exp(-length(q.xz) * 0.7);
        float drop = length(q - vec3(0.0, 0.25 * abs(sin(t * 2.2)) + 0.35 * (syn_BassHits * response), 0.0)) - 0.32;
        return smin(base, drop, 0.28);
    }
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.0, 0.2, -2.8);
    vec3 rd = normalize(vec3(p, 1.35));
    float d = 0.0;
    for (int i = 0; i < 42; ++i) {
        float h = mapChrome(ro + rd * d, t);
        if (h < 0.004 || d > 8.0) break;
        d += h * 0.82;
    }
    vec3 bg = pal(rd.y * 0.6 + 0.2) * 0.22;
    if (d > 8.0) return bg;

    vec3 pos = ro + rd * d;
    float e = 0.008;
    vec3 n = normalize(vec3(
        mapChrome(pos + vec3(e, 0.0, 0.0), t) - mapChrome(pos - vec3(e, 0.0, 0.0), t),
        mapChrome(pos + vec3(0.0, e, 0.0), t) - mapChrome(pos - vec3(0.0, e, 0.0), t),
        mapChrome(pos + vec3(0.0, 0.0, e), t) - mapChrome(pos - vec3(0.0, 0.0, e), t)
    ));
    vec3 r = reflect(rd, n);
    vec2 envUV = vec2(atan(r.z, r.x + 1e-5) / 6.2831853 + 0.5, r.y * 0.5 + 0.5);
    vec3 envCol = pal(envUV.x * 2.0 + envUV.y) * (0.45 + 0.55 * sceneRamp(-0.2, 0.4, r.y));
    vec4 img = sampleMediaTile(envUV);
    envCol = mix(envCol, img.rgb * 1.15, img.a);
    float spec = pow(max(dot(r, normalize(vec3(0.4, 0.9, -0.5))), 0.0), 32.0);
    return envCol * sceneReflectionStrength * (0.85 + 0.45 * (syn_OnBeat * response)) + vec3(1.0) * spec;
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

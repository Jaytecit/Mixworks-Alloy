// Family 09: Metaball Mass — Glass Lens
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
uniform float sceneBlobSpread;
uniform float sceneSurfaceHighlights;

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

float mapBlobs(vec3 q, float t) {
    float d = 1e5;
    float spread = ((VARIANT == 3) ? (0.85 + 0.95 * max((syn_OnBeat * response), (syn_BassHits * response))) : (0.65 + 0.25 * (syn_BassLevel * response))) * sceneBlobSpread;
    float k = (VARIANT == 3) ? 0.26 : 0.48;
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float a = t * (0.7 + 0.15 * fi) + fi * 1.047;
        vec3 c = spread * vec3(sin(a), cos(a * 0.8 + fi), sin(a * 0.6 - fi));
        float r = 0.40 + 0.12 * sin(fi * 2.1 + t * 2.0) + 0.10 * (syn_BassLevel * response);
        if (VARIANT == 2) r += 0.04 * sin(q.y * 7.0 + t * 5.0);
        d = smin(d, length(q - c) - r, k);
    }
    return d;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 ro = vec3(0.0, 0.0, -3.2 + 0.45 * (syn_BassLevel * response));
    vec3 rd = normalize(vec3(p, 1.25));
    float d = 0.0;
    for (int i = 0; i < 40; ++i) {
        float h = mapBlobs(ro + rd * d, t);
        if (h < 0.004 || d > 7.5) break;
        d += h * 0.85;
    }

    vec3 bg = pal(p.y * 0.4 + p.x * 0.2 + t * 0.08) * (0.28 + 0.25 * (0.5 + 0.5 * sin(p.x * 16.0 + p.y * 10.0 - t)));
    vec4 bgImg = sampleMedia(uv);
    bg = mix(bg, bgImg.rgb * 0.75, bgImg.a * 0.7);
    if (d > 7.5) return bg;

    vec3 pos = ro + rd * d;
    float e = 0.008;
    vec3 n = normalize(vec3(
        mapBlobs(pos + vec3(e, 0.0, 0.0), t) - mapBlobs(pos - vec3(e, 0.0, 0.0), t),
        mapBlobs(pos + vec3(0.0, e, 0.0), t) - mapBlobs(pos - vec3(0.0, e, 0.0), t),
        mapBlobs(pos + vec3(0.0, 0.0, e), t) - mapBlobs(pos - vec3(0.0, 0.0, e), t)
    ));
    float fres = pow(1.0 - clamp(dot(n, -rd), 0.0, 1.0), 3.0);
    float spec = (pow(max(dot(reflect(rd, n), normalize(vec3(0.5, 0.8, -0.6))), 0.0), 24.0)) * sceneSurfaceHighlights;
    vec2 refUV = clamp(uv + n.xy * 0.24, 0.0, 1.0);
    vec4 refImg = sampleMediaChroma(refUV, 0.015 * (syn_HighLevel * response));

    if (VARIANT == 0) {
        vec3 env = pal(n.y * 0.8 + n.x * 0.4 + t * 0.1) * (0.55 + 0.5 * abs(n.x));
        env = mix(env, refImg.rgb, refImg.a);
        return env * (0.7 + 0.6 * fres) + vec3(1.0) * spec;
    } else if (VARIANT == 1) {
        vec3 refr = pal(refUV.x * 0.6 + refUV.y * 0.4 + t * 0.1) * 0.65;
        refr = mix(refr, refImg.rgb * 1.15, refImg.a);
        return refr + pal(0.2) * fres * (0.9 + 0.5 * (syn_OnBeat * response)) + vec3(1.0) * spec;
    } else if (VARIANT == 2) {
        vec3 jelly = pal(0.65 + n.z * 0.3) * (0.45 + 0.65 * (1.0 - fres));
        jelly = mix(jelly, refImg.rgb * 1.1, refImg.a * 0.85);
        return jelly + pal(0.1) * fres + vec3(0.95) * spec * 0.8;
    } else {
        vec3 burst = pal(length(pos) * 0.5 + t * 0.2) * (0.6 + 0.8 * (syn_OnBeat * response));
        burst = mix(burst, refImg.rgb, refImg.a);
        return burst + vec3(1.0) * (spec + fres * 0.7);
    }
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

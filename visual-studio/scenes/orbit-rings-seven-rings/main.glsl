// Family 42: Orbit Rings — Seven Rings
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
uniform float sceneRingSpacing;
uniform float sceneRingThickness;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = pal(length(p) * 0.3 + t * 0.05) * 0.14;
    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 rd = normalize(vec3(p, 1.35));
    float bestT = 1e5;
    int count = (VARIANT == 1) ? 7 : 3;

    for (int i = 0; i < 7; ++i) {
        if (i >= count) break;
        float fi = float(i);
        vec3 n = vec3(0.0, 1.0, 0.0);
        float a1 = t * (0.55 + 0.18 * fi) + fi * 1.05 + (syn_BassLevel * response);
        float a2 = (VARIANT == 2) ? 0.85 : (t * 0.35 + fi * 0.72 + (syn_MidLevel * response) * 0.8);
        n.yz = rot2(a1) * n.yz;
        n.xz = rot2(a2) * n.xz;

        float denom = dot(rd, n);
        if (abs(denom) > 0.01) {
            float th = dot(-ro, n) / denom;
            if (th > 0.2 && th < bestT) {
                vec3 hit = ro + rd * th;
                float r = length(hit);
                float baseR = (VARIANT == 1) ? (0.34 + fi * (0.13 * sceneRingSpacing)) : (0.52 + fi * (0.20 * sceneRingSpacing));
                if (VARIANT == 3) baseR += 0.28 * max((syn_OnBeat * response), (syn_BassHits * response));
                float w = ((VARIANT == 1) ? 0.032 : 0.055) * sceneRingThickness;
                if (abs(r - baseR) < w) {
                    bestT = th;
                    float ang = atan(hit.y, hit.x + 1e-6) / 6.2831853 + 0.5;
                    vec3 rCol = pal(fi * 0.16 + ang * 0.4 + t * 0.1) * (0.95 + 0.65 * (syn_OnBeat * response));
                    vec4 img = sampleMediaTile(vec2(ang * 3.0 + t * 0.2, (r - baseR) / (2.0 * w) + 0.5));
                    col = mix(rCol, img.rgb, img.a);
                }
            }
        }
    }
    return col + pal(0.25) * exp(-length(p) * 8.0) * (0.45 + 0.65 * (syn_OnBeat * response));
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

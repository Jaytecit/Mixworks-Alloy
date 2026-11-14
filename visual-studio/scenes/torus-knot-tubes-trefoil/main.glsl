// Family 44: Torus Knot Tubes — Trefoil
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
uniform float sceneTubeThickness;
uniform float sceneKnotRadius;

out vec4 fragColor;

const int VARIANT = 0;

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

float sdBox2(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdBox3(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
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

vec3 knotPos(float s, float t, float offset) {
    float pK = 2.0, qK = (VARIANT == 1) ? 5.0 : 3.0;
    float a = s * 6.2831853 + offset;
    float rMaj = 0.42 + (VARIANT == 3 ? 0.18 * (syn_OnBeat * response) * sin(a * 2.0) : 0.0);
    float rMin = 0.18 + 0.05 * (syn_BassLevel * response);
    vec3 v = vec3(
        (rMaj + rMin * cos(qK * a)) * cos(pK * a),
        (rMaj + rMin * cos(qK * a)) * sin(pK * a),
        rMin * sin(qK * a)
    );
    v.xz = rot2(t * 0.65 + (syn_MidLevel * response) * 0.8) * v.xz;
    v.yz = rot2(t * 0.45) * v.yz;
    return v;
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = pal(length(p) * 0.35 + t * 0.05) * 0.15;
    float bestZ = 1e5;
    float tubeR = (0.055 + 0.015 * (syn_BassLevel * response)) * sceneTubeThickness;
    int tubes = (VARIANT == 2) ? 2 : 1;

    for (int k = 0; k < 2; ++k) {
        if (k >= tubes) break;
        float off = float(k) * 3.1415926;
        vec3 prev = knotPos(0.0, t, off) * sceneKnotRadius;
        for (int i = 1; i <= 52; ++i) {
            float s = float(i) / 52.0;
            vec3 cur = knotPos(s, t, off) * sceneKnotRadius;
            float d = sdSeg(p, prev.xy, cur.xy);
            float zMid = 0.5 * (prev.z + cur.z);
            if (d < tubeR && zMid < bestZ) {
                bestZ = zMid;
                float shade = sqrt(max(1.0 - (d / tubeR) * (d / tubeR), 0.0));
                vec3 tCol = pal(s * 2.0 + float(k) * 0.4 + t * 0.1) * (0.4 + 0.8 * shade);
                vec4 img = sampleMediaTile(vec2(s * 6.0 + t * 0.3, d / tubeR));
                col = mix(tCol, img.rgb * (0.5 + 0.7 * shade), img.a) * (0.9 + 0.4 * (syn_OnBeat * response));
            }
            prev = cur;
        }
    }
    return col;
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

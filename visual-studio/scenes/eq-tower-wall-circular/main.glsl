// Family 41: EQ Tower Wall — Circular
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
uniform float sceneTowerDensity;
uniform float sceneHeightGain;

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
    float h = (clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0)) * sceneHeightGain;
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

float synthSpec(float x) {
    float b = clamp(x, 0.0, 1.0);
    float dm = (b - 0.45) * 3.4;
    float dh = (b - 0.85) * 4.2;
    float low = (syn_BassLevel * response) * exp(-b * 4.5);
    float mid = (syn_MidLevel * response) * exp(-(dm * dm));
    float hi = (syn_HighLevel * response) * exp(-(dh * dh));
    float rip = 0.5 + 0.5 * sin(b * 26.0 - TIME * 4.2 + (syn_MidLevel * response) * 3.0);
    return clamp((low + mid + hi) * (0.65 + 0.35 * rip) + 0.14 * (syn_Level * response) + 0.18 * (syn_OnBeat * response) * (1.0 - b * 0.5), 0.04, 1.25);
}

vec3 renderScene(vec2 uv, vec2 p, float t) {
    if (VARIANT == 0 || VARIANT == 3) {
        vec2 qp = p;
        float mirrorMask = 1.0;
        if (VARIANT == 3 && p.y < 0.0) {
            qp.y = -p.y;
            mirrorMask = 0.45;
        }
        float cols = 16.0 * sceneTowerDensity;
        float colIdx = floor((qp.x + 0.85) * cols * 0.55);
        float fCol = fract((qp.x + 0.85) * cols * 0.55);
        float band = clamp(colIdx / cols, 0.0, 1.0);
        float h = (synthSpec(band) * 0.42 + (VARIANT == 3 ? 0.0 : -0.35)) * sceneHeightGain;
        float barMask = sceneRamp(0.12, 0.20, fCol) * sceneRamp(0.88, 0.80, fCol) *
                        step(VARIANT == 3 ? 0.0 : -0.40, qp.y) * sceneRamp(h + 0.01, h - 0.01, qp.y);
        vec3 col = pal(band + t * 0.05) * (0.2 + 0.8 * barMask) * mirrorMask;
        vec4 img = sampleMedia(vec2(band, clamp((qp.y + 0.4) / 0.85, 0.0, 1.0)));
        col = mix(col, img.rgb * mirrorMask, img.a * barMask);
        return col + pal(0.2) * exp(-abs(qp.y) * 35.0) * (VARIANT == 3 ? 0.6 : 0.0);
    }

    vec3 ro = vec3(0.0, 1.9 + 0.25 * (syn_BassLevel * response), t * 1.8);
    vec3 rd = normalize(vec3(p.x, p.y - 0.35, 1.2));
    float d = 0.0, hit = 0.0;
    vec2 cellID = vec2(0.0), cellUV = vec2(0.0);
    for (int i = 0; i < 42; ++i) {
        vec3 pos = ro + rd * d;
        vec2 xz = pos.xz;
        if (VARIANT == 2) xz = vec2(atan(pos.z, pos.x + 1e-5) * 3.0, length(pos.xz));
        xz *= sceneTowerDensity;
        cellID = floor(xz);
        cellUV = fract(xz);
        float h = (synthSpec(fract(cellID.x * 0.13 + cellID.y * 0.07)) * 1.35) * sceneHeightGain;
        vec3 bp = vec3(cellUV.x - 0.5, pos.y - h * 0.5, cellUV.y - 0.5);
        float sd = sdBox3(bp, vec3(0.36, h * 0.5, 0.36));
        if (sd < 0.008) { hit = 1.0; break; }
        d += max(sd * 0.7, 0.05);
        if (d > 16.0) break;
    }
    vec3 sky = pal(rd.y + 0.2) * 0.16;
    if (hit < 0.5) return sky;
    vec3 topCol = pal(fract(cellID.x * 0.15) + t * 0.05) * (0.9 + 0.6 * (syn_OnBeat * response));
    vec4 img = sampleMediaTile(cellUV);
    topCol = mix(topCol, img.rgb, img.a);
    return mix(sky, topCol, exp(-0.09 * d));
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

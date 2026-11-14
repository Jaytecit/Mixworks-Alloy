// Family 50: Volumetric Beams — Fan Sweep
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
uniform float sceneBeamWidth;
uniform float sceneHazeBrightness;

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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    float haze = 0.5 + 0.5 * fbm(p * 3.2 + vec2(t * 0.25, -t * 0.15));
    vec3 col = vec3(0.015, 0.018, 0.045);
    vec4 img = sampleMedia(uv);

    vec3 beams = vec3(0.0);
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float beamVal = 0.0;
        if (VARIANT == 0) {
            vec2 origin = vec2(0.0, 0.65);
            float ang = -1.5707963 + (fi - 2.5) * 0.22 + 0.28 * sin(t * 1.4 + fi * 0.3 + (syn_MidLevel * response));
            vec2 dir = vec2(cos(ang), sin(ang));
            vec2 pa = p - origin;
            float along = max(dot(pa, dir), 0.0);
            float perp = length(pa - dir * along);
            beamVal = exp(-perp / (sceneBeamWidth * (0.04 + 0.14 * along))) * exp(-along * 0.45);
        } else if (VARIANT == 1) {
            float side = mod(fi, 2.0) * 2.0 - 1.0;
            vec2 origin = vec2(side * 0.85, 0.58);
            float ang = 1.5707963 + side * (0.72 + 0.22 * sin(t * 1.6 + fi));
            vec2 dir = vec2(cos(ang), -sin(ang));
            vec2 pa = p - origin;
            float along = max(dot(pa, dir), 0.0);
            float perp = length(pa - dir * along);
            beamVal = exp(-perp / (sceneBeamWidth * (0.03 + 0.12 * along))) * exp(-along * 0.4);
        } else if (VARIANT == 2) {
            float xPos = (fi - 2.5) * 0.28;
            float strobe = step(0.4, hash21(vec2(fi, floor(t * 4.0))));
            beamVal = exp(-(abs(p.x - xPos) / sceneBeamWidth) * 24.0) * (0.25 + 1.1 * (syn_OnBeat * response) * strobe);
        } else {
            float xPos = (fi - 2.5) * 0.28 + 0.08 * sin(t * 1.2 + fi);
            float yCut = sceneRamp(0.55, -0.45, p.y);
            beamVal = exp(-(abs(p.x - xPos) / sceneBeamWidth) / (0.03 + 0.10 * (0.6 - p.y))) * yCut;
        }
        beams += pal(fi * 0.16 + t * 0.08) * beamVal * haze * sceneHazeBrightness * (0.55 + 0.85 * (syn_OnBeat * response));
    }

    col = mix(col, img.rgb * (0.25 + 0.9 * clamp(length(beams), 0.0, 1.2)), img.a);
    return col + beams;
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

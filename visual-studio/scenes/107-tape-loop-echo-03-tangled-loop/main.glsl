// Family 107: Tape Loop Echo — Tangled Loop
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
uniform float sceneTapeDeflection;
uniform float sceneTapeBrightness;

out vec4 fragColor;

const int VARIANT = 2;

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
float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}
float sdCyl(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
}

vec4 sampleMedia(vec2 uv) {
    if (syn_MediaType <= 0.0) return vec4(0.0);
    vec2 m = smoothstep(0.0, 0.008, uv) * (1.0 - smoothstep(0.992, 1.0, uv));
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

vec3 renderScene(vec2 uv, vec2 p, float t) {
    vec3 col = vec3(0.03, 0.025, 0.045);
    vec2 rp = vec2(abs(p.x) - 0.36, p.y - 0.05);
    float rReel = length(rp);
    vec2 spokeP = rot2(t * 2.2 * sign(p.x + 1e-5)) * rp;
    float spoke = smoothstep(0.03, 0.01, min(abs(spokeP.x), abs(spokeP.y))) * step(rReel, 0.22);
    float reel = exp(-abs(rReel - 0.23) * 48.0) + exp(-abs(rReel - 0.08) * 55.0) + spoke;

    float tapeD = 1e5;
    if (VARIANT == 2) {
        tapeD = abs(p.y + 0.25 - 0.14 * sceneTapeDeflection * sin(p.x * 7.0 - t * 3.0) * cos(p.x * 3.0 + t));
    } else {
        tapeD = min(sdSeg(p, vec2(-0.36, -0.18), vec2(0.0, -0.18 - 0.16 * sceneTapeDeflection)), sdSeg(p, vec2(0.0, -0.18 - 0.16 * sceneTapeDeflection), vec2(0.36, -0.18)));
        if (VARIANT == 1) tapeD = min(tapeD, abs(sdBox2(p, vec2(0.55, 0.32))));
    }
    float flutter = (VARIANT == 3) ? (0.5 + 0.5 * sin(p.x * 40.0 + t * 14.0)) * 0.02 * (syn_BassLevel * response) : 0.0;
    vec4 img = sampleMedia(clamp(uv + vec2(flutter, 0.0), 0.0, 1.0));
    col = mix(col, img.rgb * 0.78, img.a);
    col += pal(0.15 + t * 0.04) * reel * (0.7 + 0.6 * (syn_OnBeat * response));
    col += vec3(0.95, 0.45, 0.18) * exp(-tapeD * 42.0) * sceneTapeBrightness * (0.9 + 0.6 * (syn_OnBeat * response));
    return col;
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

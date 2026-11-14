// Family 144: Velvet Curtain Stage — Crimson Velvet
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
uniform float sceneCurtainOpening;
uniform float sceneFoldDepth;

out vec4 fragColor;

const int VARIANT = 0;

vec3 pal3(float t, vec3 a, vec3 b, vec3 d) {
    return clamp(a + b * cos(6.2831853 * (t + d)), 0.0, 1.0);
}
vec3 pal(float t) {
    return pal3(t + 0.14 * (syn_MidLevel * response), vec3(0.48, 0.44, 0.54), vec3(0.50, 0.48, 0.45), vec3(0.55, 0.08, 0.85));
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
    float open = (VARIANT == 3) ? (0.25 + 0.35 * (0.5 + 0.5 * sin(t * 0.8)) + 0.2 * (syn_BassLevel * response)) : 0.0;
    open = clamp(open + 0.55 * (sceneCurtainOpening - 1.0), 0.0, 0.8);
    float x = p.x - sign(p.x + 1e-5) * open;
    float fold = sin(x * 26.0 + sin(p.y * 4.0 + t * 1.5) * 1.5);
    float shade = mix(0.72, 0.45 + 0.55 * pow(0.5 + 0.5 * fold, 1.5), sceneFoldDepth);
    vec3 fab = (VARIANT == 1) ? vec3(0.08, 0.14, 0.45) : (VARIANT == 2 ? vec3(0.65, 0.12, 0.28) : vec3(0.75, 0.06, 0.14));
    vec3 curtain = fab * shade + (VARIANT == 2 ? vec3(0.95, 0.8, 0.25) * smoothstep(-0.35, -0.48, p.y) : vec3(0.0));
    vec4 img = sampleMedia(uv);
    float stageMask = smoothstep(open + 0.02, open - 0.02, abs(p.x));
    return mix(mix(curtain, img.rgb * (0.5 + 0.7 * shade), img.a), mix(pal(0.2) * 0.3, img.rgb, img.a), stageMask) * (0.9 + 0.35 * (syn_OnBeat * response));
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

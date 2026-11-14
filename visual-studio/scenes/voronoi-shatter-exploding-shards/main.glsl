// Family 12: Voronoi Shatter — Exploding Shards
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
uniform float sceneShardDensity;
uniform float sceneEdgeWidth;

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
    float sc = (4.2 + 0.8 * (syn_BassLevel * response)) * sceneShardDensity;
    vec2 q = p * sc;
    if (VARIANT == 3) q.x *= 1.1547;
    vec2 n = floor(q), f = fract(q);
    float md1 = 8.0, md2 = 8.0;
    vec2 bestCell = vec2(0.0), bestOff = vec2(0.0);

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 cell = n + g;
            vec2 o = hash22(cell);
            float jit = (VARIANT == 3) ? 0.24 : 0.88;
            o = 0.5 + jit * (0.5 * sin(t * 0.45 + 6.2831 * o));
            if (VARIANT == 3 && mod(cell.y, 2.0) > 0.5) o.x += 0.5;
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < md1) {
                md2 = md1;
                md1 = d;
                bestCell = cell;
                bestOff = r;
            } else if (d < md2) {
                md2 = d;
            }
        }
    }

    float edge = sqrt(md2) - sqrt(md1);
    float cid = hash21(bestCell);
    vec2 cropShift = (hash22(bestCell + 7.3) - 0.5) * (0.28 + 0.25 * (syn_BassLevel * response));
    float explode = (VARIANT == 1) ? (0.06 + 0.16 * max((syn_OnBeat * response), (syn_BassHits * response))) : 0.0;
    vec2 shardUV = uv + cropShift - normalize(bestOff + 1e-4) * explode;
    if (VARIANT == 2) shardUV = uv + bestOff * 0.08 * (1.0 + (syn_BassLevel * response));

    vec3 shardCol = pal(cid + t * 0.08) * (0.45 + 0.55 * sqrt(md1));
    vec4 img = sampleMedia(fract(shardUV));
    shardCol = mix(shardCol, img.rgb, img.a);

    float lineW = ((VARIANT == 2) ? 0.04 : 0.07) * sceneEdgeWidth;
    float crack = sceneRamp(lineW, 0.005, edge);
    if (VARIANT == 1) {
        float gap = sceneRamp(0.0, (0.09 + 0.12 * (syn_OnBeat * response)) * sceneEdgeWidth, edge);
        shardCol = mix(pal(0.15) * (0.4 + 0.9 * (syn_OnBeat * response)), shardCol, gap);
    } else {
        shardCol += pal(cid + 0.4) * crack * (0.65 + 0.85 * (syn_OnBeat * response) + 0.4 * (syn_HighLevel * response));
    }
    return shardCol;
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

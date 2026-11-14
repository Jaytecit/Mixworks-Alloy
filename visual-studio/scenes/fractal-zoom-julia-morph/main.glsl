// Family 07: Fractal Zoom — Julia Morph
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
uniform float sceneZoomDepth;
uniform float sceneOrbitTrapGlow;

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
    vec2 pr = rot2(0.08 * sin(t * 0.35) + 0.15 * (syn_MidLevel * response)) * p;
    float zoom = (exp(-mod(t * 0.45, 5.2)) * (1.0 - 0.12 * (syn_BassLevel * response))) * sceneZoomDepth;
    vec2 z = vec2(0.0), c = vec2(0.0);
    if (VARIANT == 0) {
        c = vec2(-0.743643887, 0.131825904) + pr * (1.35 * zoom);
    } else if (VARIANT == 1) {
        z = pr * (1.45 - 0.25 * (syn_BassLevel * response)) * sceneZoomDepth;
        float a = t * 0.55 + (syn_MidLevel * response) * 1.2;
        c = 0.7885 * vec2(cos(a), sin(a));
    } else if (VARIANT == 2) {
        c = vec2(-1.762, -0.028) + vec2(pr.x, -pr.y) * (0.28 * zoom + 0.04);
    } else {
        c = vec2(-0.42, 0.22) + pr * (1.55 * (0.25 + 0.75 * zoom));
    }

    float minTrap = 1e5;
    vec2 trapZ = vec2(0.0);
    float iter = 0.0;
    for (int i = 0; i < 48; ++i) {
        if (VARIANT == 2) z = abs(z);
        if (VARIANT == 3) {
            float r = max(length(z), 1e-5);
            float ang = atan(z.y, z.x + 1e-6);
            float pw = 3.0 + step(0.5, fract(t * 0.12));
            float rp = pow(r, pw);
            z = rp * vec2(cos(pw * ang), sin(pw * ang)) + c;
        } else {
            z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        }
        float dTrap = min(length(z), min(abs(z.x), abs(z.y)));
        if (dTrap < minTrap) {
            minTrap = dTrap;
            trapZ = z;
        }
        if (dot(z, z) > 16.0) break;
        iter += 1.0;
    }

    float smoothIt = iter < 48.0 ? iter - log2(max(log2(max(dot(z, z), 1.01)), 1e-4)) : 48.0;
    vec3 col = iter >= 48.0
        ? pal(minTrap * 2.5 + t * 0.15) * 0.22
        : pal(smoothIt * 0.055 + t * 0.12) * (0.85 + 0.45 * (syn_HighLevel * response));

    float trapGlow = (exp(-minTrap * (5.5 - 2.0 * (syn_OnBeat * response)))) * sceneOrbitTrapGlow;
    col += pal(minTrap + 0.35) * trapGlow * (0.65 + 0.75 * (syn_OnBeat * response));

    vec2 trapUV = trapZ * 0.45 + 0.5;
    vec4 img = sampleMedia(trapUV);
    col = mix(col, img.rgb * (0.9 + 0.4 * (syn_OnBeat * response)), img.a * clamp(trapGlow * 1.4, 0.0, 1.0));
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

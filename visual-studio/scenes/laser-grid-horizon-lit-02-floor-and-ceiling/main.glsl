#version 330 core
uniform vec2  RENDERSIZE;
uniform float TIME;
uniform float motion;
uniform float gridScale;
uniform float laserGain;
uniform float response;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_OnBeat;
uniform sampler2D syn_Media;
uniform float syn_MediaType;
out vec4 fragColor;


// ---- shared helpers -------------------------------------------------------
vec3 palette(float t) {
    // saturated cosine palette
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.00, 0.33, 0.67);
    return a + b * cos(6.28318 * (c * t + d));
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// soft laser line (distance to line segment in screen space)
float laserLine(vec2 uv, vec2 a, vec2 b, float width) {
    vec2 pa = uv - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);
    return (1.0 - smoothstep(width * 0.15, width, d));
}

// simple vignette
float vignette(vec2 uv) {
    vec2 q = uv * (1.0 - uv);
    return pow(16.0 * q.x * q.y, 0.35);
}


void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 p  = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;

    float horizon = 0.0;
    float floorMask   = step(p.y, horizon);
    float ceilingMask = 1.0 - floorMask;

    // perspective for both planes
    float fy = abs(p.y - horizon) + 0.001;
    vec2  planeUV = vec2(p.x / fy, 1.0 / fy);
    planeUV.y += (0.55 * TIME + 1.1 * motion) * (floorMask * 2.0 - 1.0);
    planeUV.x *= 1.0 + 0.2 * (syn_MidLevel * response);

    vec2 cell = floor(planeUV);
    vec2 fuv  = fract(planeUV * gridScale);
    float grid = max(
        1.0 - smoothstep(0.0, 0.035, abs(fuv.x - 0.5) - 0.43),
        1.0 - smoothstep(0.0, 0.035, abs(fuv.y - 0.5) - 0.43)
    );

    vec3 col = vec3(0.015, 0.02, 0.04);

    // media on both planes
    if (syn_MediaType > 0.0) {
        vec2 tuv = fract(planeUV * 0.22);
        tuv += 0.015 * sin(planeUV.yx * 2.5 + TIME * 0.7);
        vec4 img = texture(syn_Media, tuv);
        float m = img.a * (floorMask + ceilingMask * 0.85);
        col = mix(col, img.rgb, m);
    } else {
        float n = hash21(cell + vec2(floorMask * 17.0, 0.0));
        col = mix(col, palette(n * 0.7 + TIME * 0.04) * 0.5, 0.9);
    }

    // coloured grids
    vec3 gColF = mix(vec3(0.0, 1.0, 0.9), vec3(0.3, 0.6, 1.0), (syn_MidLevel * response));
    vec3 gColC = mix(vec3(1.0, 0.2, 0.7), vec3(1.0, 0.6, 0.1), (syn_MidLevel * response));
    col = mix(col, gColF, grid * floorMask   * (0.4 + 0.3 * (syn_HighLevel * response)));
    col = mix(col, gColC, grid * ceilingMask * (0.35 + 0.25 * (syn_HighLevel * response)));

    // crossing lasers
    float laser = 0.0;
    for (int i = 0; i < 6; ++i) {
        float t = float(i) / 5.0;
        vec2 a = vec2(-1.2 + t * 2.4,  0.9);
        vec2 b = vec2(-0.9 + t * 1.8, -0.9);
        laser = max(laser, laserLine(p, a, b, 0.01 + 0.01 * (syn_OnBeat * response)));
        // mirrored
        laser = max(laser, laserLine(p, vec2(a.x, -a.y), vec2(b.x, -b.y), 0.01 + 0.01 * (syn_OnBeat * response)));
    }
    col += vec3(1.0, 0.25, 0.55) * laser * laserGain * (0.55 + 1.5 * (syn_OnBeat * response));

    // centre glow
    col += vec3(0.1, 0.3, 0.8) * exp(-abs(p.y) * 4.0) * (0.15 + 0.2 * (syn_BassLevel * response));

    col *= vignette(uv);
    col = pow(col, vec3(0.9));
    fragColor = vec4(col, 1.0);
}

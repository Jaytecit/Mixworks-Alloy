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

    // split into left / right, each mirrored vertically for a dual-horizon look
    float side = sign(p.x);
    vec2  lp   = vec2(abs(p.x), p.y);          // fold to positive x
    // further mirror top/bottom inside each half for symmetric tunnels
    float ySign = sign(lp.y);
    lp.y = abs(lp.y);

    float horizon = 0.12 + 0.05 * (syn_BassLevel * response);
    float below   = step(lp.y, horizon);

    float fy = horizon - lp.y + 0.001;
    vec2  floorUV = vec2(lp.x / fy, 1.0 / fy);
    floorUV.y += 0.7 * TIME + 1.3 * motion;
    floorUV.x *= 1.0 + 0.18 * (syn_MidLevel * response);

    vec2 cell = floor(floorUV);
    vec2 fuv  = fract(floorUV * gridScale);
    float grid = max(
        1.0 - smoothstep(0.0, 0.038, abs(fuv.x - 0.5) - 0.42),
        1.0 - smoothstep(0.0, 0.038, abs(fuv.y - 0.5) - 0.42)
    );

    vec3 col = vec3(0.02, 0.025, 0.05);

    if (syn_MediaType > 0.0) {
        vec2 tuv = fract(floorUV * 0.28);
        tuv += 0.018 * sin(floorUV.yx * 2.8 + TIME);
        // flip u for the mirrored side so the image reads correctly
        if (side < 0.0) tuv.x = 1.0 - tuv.x;
        vec4 img = texture(syn_Media, tuv);
        col = mix(col, img.rgb, img.a * below);
    } else {
        float n = hash21(cell + vec2(side * 11.0, ySign * 7.0));
        col = mix(col, palette(n + TIME * 0.045) * 0.55, below * 0.9);
    }

    vec3 gCol = mix(vec3(0.1, 1.0, 0.85), vec3(1.0, 0.3, 0.9), 0.5 + 0.5 * side);
    col = mix(col, gCol, grid * below * (0.42 + 0.32 * (syn_HighLevel * response)));

    // lasers that cross the centre split
    float laser = 0.0;
    vec2 vanishL = vec2(-0.35, 0.0);
    vec2 vanishR = vec2( 0.35, 0.0);
    for (int i = 0; i < 3; ++i) {
        float a = float(i) * 0.45 - 0.45;
        laser = max(laser, laserLine(p, vanishL, vanishL + vec2(sin(a), -0.7) * 1.6,
                                     0.01 + 0.01 * (syn_OnBeat * response)));
        laser = max(laser, laserLine(p, vanishR, vanishR + vec2(sin(a), -0.7) * 1.6,
                                     0.01 + 0.01 * (syn_OnBeat * response)));
    }
    // vertical centre beam
    laser = max(laser, (1.0 - smoothstep(0.0, 0.03, abs(p.x))) * (0.4 + 0.6 * (syn_OnBeat * response)));

    col += vec3(1.0, 0.2, 0.55) * laser * laserGain * (0.6 + 1.3 * (syn_OnBeat * response));

    // sky on both halves
    vec3 sky = mix(vec3(0.01, 0.02, 0.07), vec3(0.07, 0.03, 0.12), uv.y);
    col = mix(sky, col, below);

    // thin centre divider glow
    col += vec3(0.4, 0.8, 1.0) * exp(-abs(p.x) * 40.0) * 0.25 * (0.5 + 0.5 * (syn_OnBeat * response));

    col *= vignette(uv);
    col = pow(col, vec3(0.92));
    fragColor = vec4(col, 1.0);
}

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
    vec2 p  = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y; // aspect-correct

    // horizon
    float horizon = 0.18 + 0.04 * (syn_BassLevel * response);
    float below   = step(p.y, horizon);

    // perspective floor
    float fy = horizon - p.y;
    vec2  floorUV = vec2(p.x / max(fy, 0.001), 1.0 / max(fy, 0.001));
    floorUV.y += 0.6 * TIME + 1.2 * motion + (syn_BassLevel * response) * 0.8;
    floorUV.x *= 1.0 + 0.15 * (syn_MidLevel * response);

    // tile grid
    vec2  cell  = floor(floorUV);
    vec2  fuv   = fract(floorUV * gridScale);
    float grid  = max(
        1.0 - smoothstep(0.0, 0.04, abs(fuv.x - 0.5) - 0.42),
        1.0 - smoothstep(0.0, 0.04, abs(fuv.y - 0.5) - 0.42)
    );

    // rushing image tiles
    vec3 col = vec3(0.02, 0.03, 0.06);
    if (syn_MediaType > 0.0) {
        vec2 tuv = fract(floorUV * 0.25);
        // slight perspective warp on the sample
        tuv += 0.02 * sin(floorUV.yx * 3.0 + TIME);
        vec4 img = texture(syn_Media, tuv);
        col = mix(col, img.rgb, img.a * below);
    } else {
        // procedural tile colour
        float n = hash21(cell);
        col = mix(col, palette(n + TIME * 0.05) * 0.55, below * 0.85);
    }

    // grid lines
    vec3 gridCol = mix(vec3(0.0, 0.9, 1.0), vec3(1.0, 0.2, 0.9), (syn_MidLevel * response));
    col = mix(col, gridCol, grid * below * (0.45 + 0.35 * (syn_HighLevel * response)));

    // lasers from vanishing point on beat
    float laser = 0.0;
    vec2  vanish = vec2(0.0, horizon);
    for (int i = 0; i < 5; ++i) {
        float ang = float(i) * 0.35 - 0.7 + 0.15 * sin(TIME + float(i));
        vec2  dir = vec2(sin(ang), -cos(ang) * 0.6);
        laser = max(laser, laserLine(p, vanish, vanish + dir * 1.8, 0.012 + 0.008 * (syn_OnBeat * response)));
    }
    vec3 laserCol = vec3(1.0, 0.15, 0.45) * (0.6 + 1.4 * (syn_OnBeat * response));
    col += laserCol * laser * laserGain * (0.7 + 0.3 * below);

    // sky gradient
    vec3 sky = mix(vec3(0.01, 0.02, 0.06), vec3(0.08, 0.04, 0.14), uv.y);
    col = mix(sky, col, below);

    // edge energy
    col += vec3(0.15, 0.4, 1.0) * (syn_HighLevel * response) * 0.08 * (1.0 - below);

    col *= vignette(uv);
    col = pow(col, vec3(0.92));
    fragColor = vec4(col, 1.0);
}

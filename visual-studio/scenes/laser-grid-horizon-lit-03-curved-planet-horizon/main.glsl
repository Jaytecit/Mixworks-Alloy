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

    // curved horizon (sphere-like)
    float R = 1.35 + 0.15 * (syn_BassLevel * response);
    float cy = -0.55;                       // centre of planet below
    float dist = length(vec2(p.x, p.y - cy));
    float surface = dist - R;
    float onPlanet = 1.0 - smoothstep(-0.02, 0.08, surface);

    // spherical-ish UV
    float theta = atan(p.x, -(p.y - cy));
    float phi   = acos(clamp((p.y - cy) / max(dist, 0.001), -1.0, 1.0));
    vec2  sphUV = vec2(theta * 1.2, phi * 2.5);
    sphUV.x += 0.25 * TIME + 0.55 * motion;
    sphUV.y += 0.08 * motion;

    vec2 cell = floor(sphUV * 3.0);
    vec2 fuv  = fract(sphUV * 3.0 * gridScale);
    float grid = max(
        1.0 - smoothstep(0.0, 0.04, abs(fuv.x - 0.5) - 0.41),
        1.0 - smoothstep(0.0, 0.04, abs(fuv.y - 0.5) - 0.41)
    );

    vec3 col = vec3(0.01, 0.015, 0.04);

    // atmosphere / sky
    float atmos = exp(-max(surface, 0.0) * 3.5);
    col = mix(col, vec3(0.05, 0.12, 0.35), atmos * 0.6);

    if (syn_MediaType > 0.0) {
        vec2 tuv = fract(sphUV * 0.35);
        tuv += 0.02 * sin(sphUV.yx * 2.0 + TIME);
        vec4 img = texture(syn_Media, tuv);
        col = mix(col, img.rgb, img.a * onPlanet);
    } else {
        float n = hash21(cell);
        col = mix(col, palette(n + TIME * 0.03) * 0.6, onPlanet * 0.9);
    }

    // grid on surface
    vec3 gCol = mix(vec3(0.0, 0.95, 1.0), vec3(1.0, 0.4, 0.1), 0.5 + 0.5 * sin(TIME * 0.4));
    col = mix(col, gCol, grid * onPlanet * (0.4 + 0.35 * (syn_HighLevel * response)));

    // lasers arcing over the horizon
    float laser = 0.0;
    for (int i = 0; i < 4; ++i) {
        float ang = -0.6 + float(i) * 0.4 + 0.1 * sin(TIME * 1.3 + float(i));
        vec2 a = vec2(sin(ang) * 0.9, 0.55);
        vec2 b = vec2(sin(ang) * 0.3, -0.1);
        laser = max(laser, laserLine(p, a, b, 0.011 + 0.012 * (syn_OnBeat * response)));
    }
    col += vec3(1.0, 0.2, 0.5) * laser * laserGain * (0.5 + 1.6 * (syn_OnBeat * response));

    // limb glow
    float limb = exp(-abs(surface) * 12.0) * onPlanet;
    col += vec3(0.2, 0.5, 1.0) * limb * (0.25 + 0.3 * (syn_BassLevel * response));

    col *= vignette(uv);
    col = pow(col, vec3(0.91));
    fragColor = vec4(col, 1.0);
}

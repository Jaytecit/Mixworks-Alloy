// F23-01 · Gravity Foundry / Binary Capture. Original Mixworks Studio scene.

float grain(vec2 id) {
    return fract(sin(dot(id, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f *= f * (3.0 - 2.0 * f);
    return mix(mix(grain(i), grain(i + vec2(1.0, 0.0)), f.x),
               mix(grain(i + vec2(0.0, 1.0)), grain(i + vec2(1.0)), f.x), f.y);
}

mat2 turn(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, s, -s, c);
}

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) {
    if (syn_MediaType < 0.5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    vec4 sprite = texture(syn_Media, uv);
    float luma = dot(sprite.rgb, vec3(.2126, .7152, .0722));
    return clamp(sprite.a * (.50 + .85 * luma) * imprint, 0.0, 1.0);
}

vec4 renderMain() {
    vec2 p = _uvc;
    float low = bass();
    vec2 grid = p * vec2(105.0, 94.0);
    float star = step(.968, grain(floor(grid))) * (1.0 - smoothstep(.10, .38, length(fract(grid) - .5)));
    float haze = noise(p * 6.0) * (.62 + .38 * noise(p * 17.0));
    vec3 col = vec3(.064, .044, .127) + vec3(.17, .12, .25) * haze + vec3(.45, .41, .64) * star;
    for (int pair = 0; pair < 4; ++pair) {
        float k = float(pair);
        vec2 centre = vec2((mod(k, 2.0) - .5) * .89, (floor(k * .5) - .5) * .56);
        vec2 uv = .5 + centre * RENDERSIZE.y / RENDERSIZE;
        float nearArt = artwork(uv + vec2(.075, 0.0));
        float farArt = artwork(uv - vec2(.075, 0.0));
        float well = nearArt + farArt + .60 * artwork(_uv);
        centre += vec2(.14, .07) * (nearArt - farArt);
        vec2 q = turn(.27 + k * 1.35 + well * 2.1) * (p - centre);
        q.y *= 1.48;
        float r = length(q);
        float angle = atan(q.y, q.x + .00001);
        float orbit = .176 + .048 * sin(angle * 2.0 + k + 1.2 * low) + .092 * well;
        float path = lineBand(r - orbit, .002, .012);
        float phase = angle - motion * (2.4 + .12 * k) + k * 2.7 + well * 2.1;
        float tail = exp(-2.1 * mod(phase, 6.2831853));
        float kernel = 1.0 - smoothstep(.014, .024, length(q - vec2(.057, 0.0)));
        float partner = 1.0 - smoothstep(.010, .018, length(q + vec2(.057, 0.0)));
        vec3 light = mix(vec3(.96, .52, .32), vec3(.44, .72, .97), mod(k, 2.0));
        col += light * (path * (.30 + .84 * tail) + .94 * kernel + .71 * partner);
        col += light * exp(-r * 14.0) * (.37 + .28 * low);
    }
    return vec4(col, 1.0);
}

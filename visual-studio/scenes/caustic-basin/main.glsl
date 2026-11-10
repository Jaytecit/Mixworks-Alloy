// F11-01 · Caustic Basin / Shoal Focus. Original Mixworks Studio scene.

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

vec2 artworkSlope(vec2 uv, float spacing) {
    vec2 dx = vec2(spacing, 0.0);
    vec2 dy = vec2(0.0, spacing);
    return vec2(artwork(uv + dx) - artwork(uv - dx),
                artwork(uv + dy) - artwork(uv - dy));
}

vec4 renderMain() {
    vec2 p = _uvc;
    float low = bass();
    vec2 bend = artworkSlope(_uv, .020);
    vec2 ray = p + .13 * bend;
    ray += vec2(.026 * sin(p.y * 14.0 + motion * 1.1),
                .025 * sin(p.x * 11.0 - motion * 1.4)) * (1.0 + 1.9 * low);
    vec2 flow = vec2(12.3 * ray.x + 2.1 * ray.y + motion * 1.2,
                     13.0 * ray.y - 4.0 * ray.x - motion * .9);
    float ribbon = sin(flow.x + 1.35 * sin(flow.y)) + sin(flow.y + 1.0 * cos(flow.x));
    float shine = exp(-11.0 * abs(ribbon));
    float twinkle = shine * (.25 + .75 * pow(.5 + .5 * cos(flow.x - flow.y), 3.0));
    float shoal = noise(p * 6.0 + vec2(2.0, 0.0));
    float bed = noise(p * 90.0) * .28 + shoal * .72;
    vec3 sand = mix(vec3(.045, .23, .27), vec3(.25, .57, .52), bed);
    float ripple = lineBand(sin(p.y * 103.0 + 5.0 * shoal), .075, .18);
    sand += vec3(.08, .16, .13) * ripple;
    sand += vec3(.49, .77, .57) * (shine * .32 + twinkle * (.32 + .31 * low));
    return vec4(sand, 1.0);
}

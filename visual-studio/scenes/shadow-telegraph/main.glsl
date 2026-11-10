// F12-01 · Shadow Telegraph / Raking Wall. Original Mixworks Studio scene.

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

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
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
    vec2 p = _uv;
    float mid = middle();
    vec2 light = vec2(.26 + .16 * sin(motion * 1.25), .11 + .05 * mid);
    vec2 brick = p * vec2(8.0, 10.0);
    brick.x += .5 * mod(floor(brick.y), 2.0);
    vec2 seam = fract(brick);
    float mortar = 1.0 - smoothstep(.014, .047, min(seam.y, seam.x));
    vec3 wall = mix(vec3(.41, .32, .24), vec3(.94, .75, .56),
                    .58 + .18 * grain(floor(brick)) + .11 * noise(p * 135.0));
    wall *= 1.0 - .54 * mortar;
    float shade = 0.0;
    float artShade = 0.0;
    for (int ray = 0; ray < 6; ++ray) {
        float rayDepth = float(ray) / 5.0;
        vec2 q = p + light * rayDepth;
        float rail = lineBand(fract(5.8 * q.x - .85 * q.y) - .56, .050, .020);
        float lintel = lineBand(fract(q.y * 2.0 + .14) - .12, .015, .014);
        shade = max(shade, max(rail, lintel) * (1.0 - .32 * rayDepth));
        artShade = max(artShade, artwork(q) * (1.0 - .38 * rayDepth));
    }
    float occlusion = max(.73 * shade, clamp(1.68 * artShade, 0.0, .88));
    vec3 col = mix(wall, wall * vec3(.34, .43, .63), occlusion);
    col *= .86 + .16 * sin(4.3 * p.y + p.x * 2.2 - motion);
    return vec4(col, 1.0);
}

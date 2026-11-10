// F01-04 - Fault Ledger / Terrace Ratchet. Original Mixworks Studio scene.

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 id = floor(p);
    vec2 f = fract(p);
    f *= f * (3.0 - 2.0 * f);
    return mix(mix(hash21(id), hash21(id + vec2(1.0, 0.0)), f.x),
               mix(hash21(id + vec2(0.0, 1.0)), hash21(id + vec2(1.0)), f.x), f.y);
}

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float luma(vec3 rgb) {
    return dot(rgb, vec3(.2126, .7152, .0722));
}

float bass() {
    return clamp(response * (.86 * syn_BassLevel + .60 * syn_BassHits + .30 * syn_OnBeat), 0.0, 1.0);
}

// Studio applies media position, scale, grade and injection before this sample.
vec4 spriteAt(vec2 uv) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, uv);
}

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float rise = p.y + .041 * sin(p.x * 4.8 + motion * .42);
    float floorIndex = floor(rise * 6.0);
    float tier = fract(rise * 6.0);
    float stepShift = .14 * mod(floorIndex, 2.0) + .087 * low * sin(floorIndex * 2.3);
    float blocks = (p.x + stepShift) * 2.48;
    vec2 tile = vec2(fract(blocks) - .5, tier - .54);
    float blockIndex = floor(blocks);
    float ratchet = .5 + .5 * cos(motion * 1.5 + floorIndex * .84);
    vec2 printUV = .5 + tile * vec2(1.38, 1.69);
    printUV.x += .22 * tile.y * sin(floorIndex + .91 * blockIndex) + .042 * ratchet;
    if (mod(blockIndex + floorIndex, 2.0) > .5) printUV.x = 1.0 - printUV.x;
    float face = smoothstep(.15, .28, tier) * (1.0 - smoothstep(.82, .94, tier));
    vec4 printSample = spriteAt(printUV);
    float printMask = printSample.a * imprint * face;
    float tone = .58 + .42 * luma(printSample.rgb);
    float lower = spriteAt(printUV + vec2(-.032, .061)).a * imprint * face;
    float stepEdge = lineBand(tier - .18, .034 + .025 * low, .041);
    float seams = lineBand(tile.x - .492, .023, .042);
    float grit = noise(vec2(211.0 * p.x, 97.0 * p.y));
    vec3 clay = mix(vec3(.36, .19, .13), vec3(.86, .62, .40),
                    .25 + .43 * tier + .13 * sin(floorIndex * 2.2) + .12 * grit);
    clay *= 1.0 - .44 * stepEdge - .27 * seams;
    clay = mix(clay, clay * vec3(.42, .34, .32), clamp(printMask * tone, 0.0, .88));
    clay += vec3(.50, .35, .17) * max(0.0, lower - printMask) * (.64 + .33 * ratchet);
    return vec4(clay, 1.0);
}

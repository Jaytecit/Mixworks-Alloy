// F01-06 - Fault Ledger / Island Breakaway. Original Mixworks Studio scene.

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

mat2 rotateBy(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, s, -s, c);
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
    float floorGrit = noise(p * vec2(105.0, 77.0));
    float layer = fract(p.y * 15.0 + 1.2 * sin(p.x * 6.8 - motion * .58) + .30 * floorGrit);
    vec3 col = mix(vec3(.22, .18, .11), vec3(.45, .36, .19), .24 + .34 * layer + .38 * floorGrit);
    vec2 density = vec2(12.0, 8.0);
    vec2 grid = p * density;
    vec2 tile = floor(grid);
    float cover = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 id = tile + vec2(float(x), float(y));
            vec2 home = id + .5;
            float seed = hash21(id);
            vec2 samplePoint = home / density;
            vec4 centreArt = spriteAt(samplePoint);
            vec4 cornerArt = spriteAt(samplePoint + vec2(.023, .029));
            float breakFree = clamp(max(centreArt.a, .86 * cornerArt.a) * imprint, 0.0, 1.0);
            vec2 drift = normalize(vec2(seed - .46, .28 + .54 * hash21(id + 4.0)));
            float impulse = .26 + .34 * sin(motion * 2.4 + id.x * .55 + id.y * .67) + .68 * low;
            vec2 shift = drift * breakFree * clamp(impulse, -.16, 1.0) * .96;
            shift.x += .13 * sin(id.y * 1.4 - motion);
            vec2 local = grid - home - shift;
            float turnAngle = breakFree * impulse * (seed - .5) * 1.6;
            vec2 cut = rotateBy(turnAngle) * local;
            vec2 shape = abs(cut) + vec2(.09 * cut.y, -.13 * cut.x);
            float chip = max(shape.x, shape.y) + .35 * min(shape.x, shape.y);
            float fragment = 1.0 - smoothstep(.43, .47, chip);
            if (fragment <= cover) continue;

            // Sample artwork from the original bed, then move the whole rock chip.
            vec4 textureArt = spriteAt((home + cut) / density);
            float ink = clamp(textureArt.a * imprint, 0.0, .82);
            float bevel = lineBand(chip - .405, .022, .035);
            float shelf = .5 + .5 * sin(cut.y * 19.0 + cut.x * 5.0 + seed * 14.0);
            vec3 stone = mix(vec3(.45, .32, .16), vec3(.85, .73, .42), .32 + .27 * seed + .32 * shelf);
            stone = mix(stone, .14 + .73 * textureArt.rgb, ink);
            stone += vec3(.42, .30, .14) * bevel * clamp(.45 - cut.y, 0.0, 1.0);
            col = mix(col, stone * (.91 + .15 * floorGrit), fragment);
            cover = fragment;
        }
    }
    return vec4(col, 1.0);
}

// F15-01 · Ceramic Resonator / Honeycomb Buckle. Original Mixworks Studio scene.

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

vec2 tileCentre(vec2 p) {
    vec2 spacing = vec2(1.0, 1.73205);
    vec2 a = spacing * (floor(p / spacing) + .5);
    vec2 b = spacing * (floor(p / spacing + .5));
    return length(p - a) < length(p - b) ? a : b;
}

vec4 renderMain() {
    vec2 p = _uvc * 11.6;
    float low = bass();
    vec2 tile = tileCentre(p);
    float art = artwork(.5 + tile * RENDERSIZE.y / (11.6 * RENDERSIZE));
    float travel = tile.x * .89 - tile.y * .63 - 3.2 * motion;
    float flex = sin(travel + 2.6 * art) * (.24 + .47 * low) / (1.0 + 2.0 * art);
    vec2 q = p - tile;
    vec2 folded = abs(turn(flex) * (q - vec2(.045, -.074) * flex));
    float hex = max(folded.x, .5 * folded.x + .866 * folded.y);
    float grout = smoothstep(.443, .483, hex);
    float bevel = lineBand(hex - .423, .013, .022);
    float glaze = grain(tile * 1.4);
    float fleck = noise(p * 35.0);
    vec3 ceramic = mix(vec3(.10, .30, .46), vec3(.34, .66, .72), glaze);
    ceramic *= .83 + .24 * flex + .19 * fleck;
    ceramic += vec3(.39, .53, .48) * bevel * (.57 + .4 * sin(4.0 * q.y + flex));
    return vec4(mix(ceramic, vec3(.15, .17, .20) * (.84 + .25 * fleck), grout), 1.0);
}

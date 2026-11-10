// F16-01 · Kinetic Typesetter / Column Set. Original Mixworks Studio scene.

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

float segment(vec2 p, vec2 a, vec2 b) {
    vec2 axis = b - a;
    vec2 rel = p - a;
    return length(rel - axis * clamp(dot(rel, axis) / dot(axis, axis), 0.0, 1.0));
}

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) {
    if (syn_MediaType < 0.5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    vec4 sprite = texture(syn_Media, uv);
    float luma = dot(sprite.rgb, vec3(.2126, .7152, .0722));
    return clamp(sprite.a * (.50 + .85 * luma) * imprint, 0.0, 1.0);
}

float typeMark(vec2 p, int letter) {
    const int forms[12] = int[12](63, 6, 91, 79, 102, 109, 125, 7, 127, 111, 119, 124);
    int mask = forms[letter];
    float d = 1.0;
    if ((mask & 1) != 0) d = min(d, segment(p, vec2(-.22, .31), vec2(.22, .31)));
    if ((mask & 2) != 0) d = min(d, segment(p, vec2(.22, .31), vec2(.22, .02)));
    if ((mask & 4) != 0) d = min(d, segment(p, vec2(.22, -.02), vec2(.22, -.31)));
    if ((mask & 8) != 0) d = min(d, segment(p, vec2(-.22, -.31), vec2(.22, -.31)));
    if ((mask & 16) != 0) d = min(d, segment(p, vec2(-.22, -.31), vec2(-.22, -.02)));
    if ((mask & 32) != 0) d = min(d, segment(p, vec2(-.22, .02), vec2(-.22, .31)));
    if ((mask & 64) != 0) d = min(d, segment(p, vec2(-.20, 0.0), vec2(.20, 0.0)));
    return 1.0 - smoothstep(.035, .063, d);
}

vec4 renderMain() {
    vec2 p = _uv;
    float mid = middle();
    float hit = percussion();
    float column = floor(p.x * 4.0);
    float localX = fract(p.x * 4.0);
    float spacing = 5.6 - .84 * mid;
    float typeset = localX * spacing + .14 * sin(motion * 2.0 + column) * (1.0 - hit);
    float baseline = p.y * 13.0 + .43 * motion + .07 * sin(column + motion);
    vec2 grid = vec2(typeset, baseline);
    vec2 id = floor(grid) + vec2(column * 43.0, 0.0);
    vec2 q = fract(grid) - .5;
    vec2 centre = p - q / vec2(4.0 * spacing, 13.0);
    float art = artwork(centre);
    float seed = grain(id);
    int letter = int(mod(floor(seed * 12.0) + floor(art * 16.0), 12.0));
    q *= turn(.37 * (1.0 - hit) * sin(motion * 1.8 + seed * 9.0) + art * 1.1);
    float mark = typeMark(q, letter) * (1.0 - .82 * smoothstep(.20, .64, art));
    float rule = lineBand(localX - .05, .005, .010);
    vec3 paper = vec3(.91, .84, .69) * (.94 + .09 * noise(p * 154.0));
    vec3 ink = mix(vec3(.10, .14, .16), vec3(.73, .21, .16), step(.76, seed));
    return vec4(mix(paper, ink, max(mark, .72 * rule)), 1.0);
}

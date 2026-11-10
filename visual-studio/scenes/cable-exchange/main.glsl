// F17-01 · Cable Exchange / Bus Arbitration. Original Mixworks Studio scene.

float grain(vec2 id) {
    return fract(sin(dot(id, vec2(127.1, 311.7))) * 43758.5453);
}

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
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

vec4 renderMain() {
    vec2 p = _uv;
    float hit = percussion();
    vec2 board = p * vec2(13.0, 9.0);
    vec2 id = floor(board);
    vec2 cell = fract(board);
    float art = artwork((id + .5) / vec2(13.0, 9.0));
    float route = grain(id + 13.0);
    float gate = step(.52, route + .62 * art + .12 * hit);
    float busY = mix(.26, .71, mod(id.y, 2.0));
    float track = lineBand(cell.y - busY, .017, .028);
    float riser = lineBand(cell.x - .5, .022, .035) * gate;
    float node = 1.0 - smoothstep(.046, .10, length(cell - vec2(.5, busY)));
    float run = fract(board.x * .42 - motion * 1.5 + id.y * .19 - .20 * hit);
    float busPulse = lineBand(run - .28, .045, .12);
    float branchPulse = lineBand(fract(board.y * .6 + motion * 1.22 + id.x * .37) - .47, .062, .15);
    float marks = (1.0 - smoothstep(.036, .075, length(fract(board * 3.0) - .5))) * .13;
    vec3 plate = vec3(.053, .08, .11) + vec3(.09, .15, .17) * marks;
    plate += vec3(.43, .29, .10) * track * (.40 + .51 * busPulse);
    plate += vec3(.09, .47, .64) * riser * (.39 + 1.09 * branchPulse);
    plate += vec3(.34, .61, .54) * node * (.25 + .60 * gate + .45 * hit * busPulse);
    return vec4(plate, 1.0);
}

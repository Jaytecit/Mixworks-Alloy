// F22-01 · Clockwork Conveyor / Corner Transfer. Original Mixworks Studio scene.

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
    float bolt = 1.0 - smoothstep(.04, .10, length(fract(p * vec2(27.0, 16.0)) - .5));
    vec3 col = vec3(.12, .16, .16) + vec3(.19, .20, .13) * (.31 * bolt + .40 * noise(p * 81.0));
    for (int lane = 0; lane < 4; ++lane) {
        float k = float(lane);
        float y = .12 + .229 * k;
        float nominalTurn = .91 - .195 * k;
        float gateArt = artwork(vec2(nominalTurn, y + .05)) + .66 * artwork(vec2(.48, y));
        float gate = smoothstep(.13, .38, gateArt);
        float turnX = nominalTurn - (.11 + .035 * hit) * gate;
        vec2 corner = vec2(turnX, y);
        vec2 delta = p - corner;
        float across = length(vec2(max(delta.x, 0.0), delta.y));
        float upright = length(vec2(delta.x, min(delta.y, 0.0)));
        bool vertical = upright < across;
        float off = min(across, upright);
        float along = vertical ? turnX + max(delta.y, 0.0) : min(p.x, turnX);
        float belt = 1.0 - smoothstep(.039, .045, off);
        float lip = lineBand(off - .043, .003, .004);
        float slats = lineBand(fract(along * 48.0 - motion * 4.0) - .25, .034, .041);
        float art = gateArt + .82 * artwork(p);
        float shift = step(.18, art + .08 * hit);
        float phase = fract(along * 6.0 - motion * 1.31 - .17 * k + .30 * shift);
        float payload = lineBand(phase - .48, .19 + .11 * art, .035)
                      * (1.0 - smoothstep(.017 + .014 * art, .021 + .017 * art, off));
        vec3 tread = vec3(.23, .25, .21) * (1.0 - .48 * slats);
        col = mix(col, tread, belt);
        col = mix(col, mix(vec3(.84, .61, .20), vec3(.32, .78, .63), shift), payload);
        col += vec3(.36, .40, .30) * lip * (.61 + .32 * hit);
    }
    return vec4(col, 1.0);
}

// F20-01 · Sonar Curtain / Lateral Ping Sweep. Original Mixworks Studio scene.

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
    float source = .13 + .72 * fract(.34 * motion);
    vec3 col = vec3(.041, .086, .10);
    float graph = lineBand(fract(p.x * 16.0) - .03, .009, .022)
                + lineBand(fract(p.y * 11.0) - .03, .013, .023);
    col += vec3(.11, .27, .24) * graph;
    for (int i = 0; i < 5; ++i) {
        float k = float(i);
        float depth = .10 + .20 * k + .045 * sin(p.x * 9.0 + k * 2.7);
        float art = artwork(vec2(p.x, depth)) + .73 * artwork(p);
        float range = depth + .25 * art + .034 * sin(p.x * 19.0 + k * 3.0 - motion * .6);
        float arrival = source - (.075 + .095 * k) - .28 * art;
        float streak = exp(-12.0 * abs(p.x - arrival)) * smoothstep(-.03, .01, source - p.x);
        float returnLine = lineBand(p.y - range, .004 + .004 * hit, .012);
        float bottom = lineBand(p.y - depth, .001, .007);
        col += vec3(.10, .23, .19) * bottom;
        col += vec3(.37, .84, .51) * returnLine * streak * (.63 + .85 * hit);
    }
    col += vec3(.28, .73, .58) * lineBand(p.x - source, .003, .018) * (.56 + .44 * hit);
    return vec4(col, 1.0);
}

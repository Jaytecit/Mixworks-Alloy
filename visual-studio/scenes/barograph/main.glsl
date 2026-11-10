// F18-01 · Barograph / Stacked Seismograms. Original Mixworks Studio scene.

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

float treble() {
    return clamp(response * (3.3 * syn_HighLevel + 1.15 * syn_HighHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) {
    if (syn_MediaType < 0.5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    vec4 sprite = texture(syn_Media, uv);
    float luma = dot(sprite.rgb, vec3(.2126, .7152, .0722));
    return clamp(sprite.a * (.50 + .85 * luma) * imprint, 0.0, 1.0);
}

vec4 writeTrace() {
    vec2 p = _uv;
    if (reset_trace > .5 || FRAMECOUNT == 0.0) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(chart, p);
    float shift = min(iTimeDelta, .066) * (.16 + .16 * paper_rate);
    vec4 trace = p.x > shift ? texture(chart, p - vec2(shift, 0.0)) : vec4(0.0);
    if (p.x > shift) return trace * exp(-.29 * iTimeDelta);

    float lane = floor(p.y * 7.0);
    float age = 1.0 - p.x / max(shift, .00001);
    float wobble = sin(motion * (2.6 + .31 * lane) + lane * 1.6);
    float level = mix(bass(), treble(), lane / 6.0);
    float sampleY = (lane + .5) / 7.0;
    float art = .72 * artwork(vec2(.5 + .28 * wobble, sampleY)) +
                .45 * artwork(vec2(.38, sampleY));
    float nib = .50 + .077 * wobble + (.12 + .28 * level) * sin(lane + motion * 9.0)
                + .46 * art + .065 * age * cos(lane + motion * 9.0);
    float mark = lineBand(fract(p.y * 7.0) - nib, .020, .021) * (1.0 - .52 * art);
    return vec4(mark, mark * (.24 + .66 * level), mark * art, 1.0);
}

vec4 renderMain() {
    if (PASSINDEX == 0) return writeTrace();
    vec2 p = _uv;
    vec2 grid = p * vec2(48.0, 56.0);
    grid.x -= motion * 11.0;
    float graph = 1.0 - smoothstep(.012, .092, min(fract(grid.x), fract(grid.y)));
    float lane = lineBand(fract(p.y * 7.0) - .5, .009, .022);
    vec4 trace = texture(chart, p);
    float guide = lineBand(fract(p.y * 7.0) - .50 - .06 * sin(motion * 3.0 - p.x * 28.0), .006, .016);
    vec3 paper = vec3(.88, .79, .65) * (.94 + .11 * noise(p * 142.0));
    paper = mix(paper, vec3(.65, .37, .29), .32 * graph + .33 * lane);
    vec3 pen = mix(vec3(.15, .19, .22), vec3(.75, .21, .14), clamp(trace.g + trace.b, 0.0, 1.0));
    paper = mix(paper, vec3(.42, .35, .30), .34 * guide * (1.0 - trace.r));
    return vec4(mix(paper, pen, trace.r), 1.0);
}

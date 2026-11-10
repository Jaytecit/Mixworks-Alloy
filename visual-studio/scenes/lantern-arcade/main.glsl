// F02-01 · Lantern Arcade / Long Arcade Sweep. Original Mixworks Studio scene.

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

float boxDistance(vec2 p, vec2 halfSize) {
    vec2 q = abs(p) - halfSize;
    return length(max(q, 0.0)) + min(0.0, max(q.x, q.y));
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
    vec2 p = _uvc;
    vec2 drift = p - vec2(.10 * sin(motion * .42), -.065);
    float mid = middle();
    float wallDepth = min(.52 / max(abs(drift.x), .001),
                          .34 / max(abs(drift.y), .001));
    bool wall = .52 * abs(drift.y) < .34 * abs(drift.x);
    float height = wall ? drift.y * wallDepth : drift.x * wallDepth;
    float travel = wallDepth + motion * .77;
    float bay = floor(travel);
    float rib = lineBand(fract(travel) - .05, .028, .045);
    vec2 section = vec2(fract(travel) - .48, height);
    float art = artwork(_uv);
    float window = lineBand(boxDistance(section, vec2(.24, .11 + .19 * art)), 0.0, .025);
    window = max(window, 1.0 - smoothstep(0.0, .022,
                 boxDistance(section, vec2(.23 + .25 * art, .065 + .19 * art))));
    float live = .34 + .66 * pow(.5 + .5 * cos(bay * 1.8 - motion * 3.3 + 3.0 * mid), 3.0);
    vec3 brick = mix(vec3(.15, .13, .21), vec3(.44, .25, .25), .48 + .35 * sin(bay * 2.1));
    brick *= .70 + .32 * noise(vec2(travel * 23.0, height * 72.0));
    vec3 col = brick * (wall ? 1.0 : .67) + rib * vec3(.30, .18, .13);
    if (wall) {
        col += window * live * vec3(1.0, .62, .26);
    } else if (drift.y < 0.0) {
        float shaft = artwork(_uv + vec2(sign(drift.x) * .19, .18));
        col += vec3(.66, .37, .14) * live * (.19 + .86 * shaft)
             * lineBand(section.x + .20 * sin(6.0 * height), .11, .09);
    }
    float fog = 1.0 - exp(-.13 * wallDepth);
    return vec4(mix(col, vec3(.095, .14, .21) + mid * vec3(.08, .04, .02), fog), 1.0);
}

// F04-01 · Sediment Clock / Delta Prograde. Original Mixworks Studio scene.

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

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) {
    if (syn_MediaType < 0.5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    vec4 sprite = texture(syn_Media, uv);
    float luma = dot(sprite.rgb, vec3(.2126, .7152, .0722));
    return clamp(sprite.a * (.50 + .85 * luma) * imprint, 0.0, 1.0);
}

vec4 storeSediment() {
    if (FRAMECOUNT == 0.0 || clear_bed > .5) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(bedHistory, _uv);
    float dt = min(iTimeDelta, .066);
    vec2 upstream = _uv - dt * vec2(.075 + .053 * bass(), -.16);
    float inside = step(0.0, upstream.x) * step(upstream.y, 1.0);
    float previous = texture(bedHistory, upstream).r * inside * exp(-.72 * dt);
    float grains = .45 + .55 * grain(floor(_uv * vec2(128.0, 79.0)));
    return vec4(clamp(previous + 3.1 * dt * artwork(_uv) * grains, 0.0, 1.0), 0.0, 0.0, 1.0);
}

vec4 renderMain() {
    if (PASSINDEX == 0) return storeSediment();
    vec2 p = _uv;
    float low = bass();
    float deposit = artwork(p) + .36 * artwork(p + vec2(0.0, .12))
                  + .80 * texture(bedHistory, p).r;
    float front = .53 + .21 * sin(motion * .41) + .11 * low;
    float shelf = .76 - .36 * smoothstep(front - .23, front + .30, p.x)
                + .053 * sin(p.x * 11.0 + motion) + .20 * deposit;
    float slope = smoothstep(front - .3, front + .32, p.x);
    float below = shelf - p.y;
    float depth = below * (19.0 + 12.0 * low) + 1.6 * slope + .42 * noise(p * 22.0);
    float bed = floor(depth);
    float seam = lineBand(fract(depth) - .025, .018, .080);
    float cobbles = grain(floor(p * vec2(155.0, 120.0)));
    vec3 clay = mix(vec3(.23, .10, .075), vec3(.87, .65, .35), .5 + .45 * sin(bed * 1.67));
    clay *= .63 + .31 * cobbles + .19 * (1.0 - seam);
    clay += vec3(.70, .38, .14) * deposit * smoothstep(.5, .92, cobbles);
    float stream = sin(p.y * 62.0 + 4.0 * sin(p.x * 10.0 - motion * 1.5)
                  + 2.2 * noise(p * 12.0));
    vec3 water = vec3(.06, .18, .22) + vec3(.06, .20, .18) * (.50 + .50 * stream);
    vec3 col = mix(water, clay, smoothstep(-.005, .005, below));
    col += vec3(.48, .40, .21) * lineBand(below, .002, .012) * (.36 + .45 * low);
    return vec4(col, 1.0);
}

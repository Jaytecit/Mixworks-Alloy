// F09-01 · Fumarole Field / Ridge Jets. Original Mixworks Studio scene.

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

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float ridge = .25 + .048 * sin(p.x * 20.0) + .04 * sin(p.x * 8.0 - 1.2);
    float ash = noise(p * vec2(63.0, 25.0));
    float ground = 1.0 - smoothstep(-.006, .014, p.y - ridge);
    vec3 col = mix(vec3(.15, .18, .25) + vec3(.12, .14, .16) * ash,
                   vec3(.22, .14, .10) + vec3(.28, .16, .08) * ash, ground);
    float obstruction = artwork(p);
    for (int vent = 0; vent < 8; ++vent) {
        float k = float(vent);
        float x = (k + .5) / 8.0;
        float base = .25 + .048 * sin(x * 20.0) + .04 * sin(x * 8.0 - 1.2);
        float h = p.y - base;
        float valve = artwork(vec2(x, .35)) + .78 * artwork(vec2(x, .55));
        float surge = .44 + .32 * sin(k * 1.46 + motion * 2.8) + .62 * low;
        float neck = x + (.13 + .25 * valve) * h * h * sin(4.0 * h - motion + k)
                     + .13 * h * obstruction;
        float width = .012 + h * (.11 + .12 * valve);
        float density = exp(-pow(abs(p.x - neck) / max(width, .006), 2.0))
                      * smoothstep(-.018, .026, h) * exp(-max(h, 0.0) * 1.4);
        vec2 vapor = vec2(p.x * 41.0, p.y * 12.0 - motion * (1.6 + .4 * low));
        float billow = smoothstep(.20, .87, noise(vapor + k * 14.0) + .24 * ash);
        col = mix(col, mix(vec3(.52, .31, .20), vec3(.86, .79, .68), smoothstep(.01, .40, h)),
                  clamp(density * surge * billow * (1.0 - .56 * valve), 0.0, .91));
        col += vec3(.78, .37, .13) * lineBand(p.x - x, .004, .014) * lineBand(h, .005, .02) * surge;
    }
    return vec4(col, 1.0);
}

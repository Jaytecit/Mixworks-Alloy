// F24-01 · Granular Hopper / Funnel Arch Break. Original Mixworks Studio scene.

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
    float block = artwork(p);
    float catchment = artwork(p + vec2(0.0, .22)) + .72 * artwork(p + vec2(0.0, .37));
    float bay = floor(p.x * 3.0);
    float side = fract(p.x * 3.0) - .5;
    float neck = .10 * sin(bay * 3.3 + motion) + .45 * block;
    float floorY = .10 + .26 * max(0.0, .48 - abs(side - neck))
                 + .034 * sin(p.x * 17.0) + .21 * catchment;
    float heap = 1.0 - smoothstep(-.009, .014, p.y - floorY);
    float wall = abs(side - neck) - max(.046, .72 * (p.y - .31));
    float metal = lineBand(wall, .011, .015) * smoothstep(.29, .37, p.y);
    vec2 fall = vec2(p.x * 183.0 + .68 * sin(p.y * 28.0 + motion * 5.0),
                     (p.y + motion * .24 + .09 * hit) * 107.0);
    vec2 grid = floor(fall);
    float seed = grain(grid);
    float chute = 1.0 - smoothstep(.038, .085 + .07 * hit, abs(side - neck));
    float above = (1.0 - smoothstep(.0, .055, wall)) * smoothstep(.40, .70, p.y);
    float particles = step(.76 - .08 * hit, seed) * max(chute, .76 * above);
    float gravel = grain(floor(p * vec2(221.0, 117.0)));
    float grate = lineBand(fract(p.x * 92.0 + p.y * 36.0) - .5, .03, .036) * smoothstep(.12, .51, block);
    vec3 col = vec3(.11, .083, .10) + vec3(.14, .10, .11) * noise(p * 23.0);
    col = mix(col, mix(vec3(.35, .20, .10), vec3(.83, .59, .30), gravel), heap);
    col += vec3(.79, .51, .23) * particles * (.58 + .43 * seed) * (1.0 - .68 * block);
    col += vec3(.49, .39, .35) * (metal + .44 * grate);
    return vec4(col, 1.0);
}

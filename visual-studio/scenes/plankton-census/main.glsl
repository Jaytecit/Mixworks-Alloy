// F08-01 · Plankton Census / Estuary Sorting. Original Mixworks Studio scene.

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
    vec2 p = _uv;
    float mid = middle();
    float art = artwork(p);
    float channel = .48 + .15 * sin(6.1 * p.x - .5 * motion);
    float water = noise(p * vec2(9.0, 27.0) - vec2(motion * .6, 0.0));
    vec3 col = mix(vec3(.058, .11, .22), vec3(.13, .40, .44),
                   .20 + .32 * water + .35 * exp(-7.0 * abs(p.y - channel)));
    for (int lane = 0; lane < 5; ++lane) {
        float k = float(lane);
        float way = mod(k, 2.0) * 2.0 - 1.0;
        float route = channel + (k - 2.0) * (.16 - .033 * mid);
        float cover = artwork(p - vec2(.075 * way, 0.0));
        float jet = art * (.085 + .027 * mid) + .14 * cover;
        float y = p.y - route - way * jet;
        vec2 grid = vec2((p.x - way * motion * (.095 + .027 * k)) * 31.0,
                         y * (43.0 + 3.0 * mid));
        grid.x += .51 * mod(floor(grid.y), 2.0);
        vec2 id = floor(grid);
        vec2 q = fract(grid) - .5;
        float seed = grain(id + 57.0 * k);
        q.y += .10 * sin(seed * 15.0 + motion * 5.0 + q.x * 4.0);
        float body = 1.0 - smoothstep(.72, 1.1, length(q / vec2(.35, .12)));
        float tail = lineBand(q.y - .27 * abs(q.x), .024, .049)
                   * smoothstep(-.48, -.10, way * q.x) * (1.0 - smoothstep(-.06, .02, way * q.x));
        float school = exp(-pow(abs(y) / (.079 - .024 * mid), 2.0)) * smoothstep(.11, .49, seed);
        vec3 tint = mix(vec3(.44, .96, .75), vec3(1.0, .58, .23), .5 + .5 * way);
        col += tint * min(1.0, body + tail) * school * (.43 + .41 * seed);
    }
    return vec4(col, 1.0);
}

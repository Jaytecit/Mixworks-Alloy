// F21-01 · Pendulum Fleet / Crossed Arms. Original Mixworks Studio scene.

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

float segment(vec2 p, vec2 a, vec2 b) {
    vec2 axis = b - a;
    vec2 rel = p - a;
    return length(rel - axis * clamp(dot(rel, axis) / dot(axis, axis), 0.0, 1.0));
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
    vec2 p = _uvc;
    float low = bass();
    float grid = lineBand(fract(p.x * 18.0) - .025, .009, .022)
               + lineBand(fract(p.y * 18.0) - .025, .009, .022);
    vec3 col = vec3(.072, .10, .14) + vec3(.17, .24, .29) * (.29 * grid + .43 * noise(p * 6.0));
    for (int row = 0; row < 3; ++row) {
        float k = float(row);
        float ceiling = .51 - .326 * k;
        float rail = lineBand(p.y - ceiling, .004, .007);
        col += vec3(.27, .29, .33) * rail * (.6 + .11 * k);
        for (int arm = 0; arm < 6; ++arm) {
            float j = float(arm);
            vec2 pivot = vec2((j - 2.5) * .30 + .05 * mod(k, 2.0), ceiling);
            vec2 centre = .5 + (pivot - vec2(0.0, .18)) * RENDERSIZE.y / RENDERSIZE;
            float art = artwork(centre) + .62 * artwork(centre + vec2(.04, -.10));
            pivot += vec2(.11, -.045) * art;
            float sweep = sin(motion * (2.1 - .22 * k) + j * 1.7 + k * 1.4 + 2.2 * art);
            float angle = sweep * (.51 + .45 * low + .40 * art);
            float armLength = .29 + .025 * sin(1.8 * j + k);
            vec2 weight = pivot + armLength * vec2(sin(angle), -cos(angle));
            float cable = 1.0 - smoothstep(.0023, .0060, segment(p, pivot, weight));
            vec2 knob = p - weight;
            float disk = 1.0 - smoothstep(.039, .044, length(knob));
            float rim = lineBand(length(knob) - .037, .003, .004);
            float stud = 1.0 - smoothstep(.006, .009, length(p - pivot));
            vec3 brass = mix(vec3(.66, .42, .23), vec3(.27, .68, .77), .5 + .46 * sin(k * 2.7));
            col = mix(col, vec3(.64, .65, .59), cable * (.69 + .10 * k));
            col = mix(col, brass * (.70 + 7.4 * knob.y + .31 * rim), disk);
            col += vec3(.62, .53, .37) * stud;
        }
    }
    return vec4(col, 1.0);
}

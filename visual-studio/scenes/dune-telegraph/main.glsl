// F05-01 · Dune Telegraph / Crescent March. Original Mixworks Studio scene.

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

mat2 turn(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, s, -s, c);
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
    vec2 p = turn(-.34) * _uvc;
    float mid = middle();
    float barrier = artwork(_uv);
    float lee = artwork(_uv - vec2(.085, .021)) + .57 * artwork(_uv - vec2(.17, .045));
    p.x += .18 * lee - .12 * barrier;
    vec2 grid = vec2(p.x * 4.3 - motion * (.62 + .35 * mid),
                     p.y * 5.3 + .15 * sin(5.0 * p.x));
    grid.x += .48 * mod(floor(grid.y), 2.0);
    vec2 cell = fract(grid) - .5;
    float crescent = cell.x + .25 - 1.50 * cell.y * cell.y;
    float bank = exp(-11.0 * abs(crescent)) * (1.0 - smoothstep(.30, .49, abs(cell.y)));
    float slip = exp(-5.0 * max(crescent, 0.0)) * smoothstep(-.08, .045, crescent) * bank;
    float wind = .5 + .5 * sin(p.y * 139.0 + 6.0 * noise(p * 13.0) - p.x * 32.0 + motion * 3.0);
    float dust = noise(p * vec2(154.0, 320.0) - vec2(motion * 2.3, 0.0));
    vec3 col = mix(vec3(.40, .29, .23), vec3(.93, .73, .47), .21 + .47 * bank + .13 * wind);
    col -= vec3(.26, .24, .23) * slip;
    col += vec3(.20, .095, .033) * dust * (.48 + .42 * mid + .90 * lee);
    return vec4(col, 1.0);
}

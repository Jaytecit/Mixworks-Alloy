// F25-01 · Pneumatic Quilt / Diamond Inflation. Original Mixworks Studio scene.

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
    vec2 p = _uvc;
    vec2 grid = turn(.785398) * p * 9.8;
    vec2 id = floor(grid);
    vec2 q = fract(grid) - .5;
    vec2 centre = turn(-.785398) * (id + .5) / 9.8;
    vec2 uv = .5 + centre * RENDERSIZE.y / RENDERSIZE;
    float plug = artwork(uv);
    float leak = artwork(uv + vec2(.060, -.055));
    float low = bass();
    float pulse = .5 + .5 * sin(1.3 * id.x + .82 * id.y - 3.3 * motion + 1.4 * leak);
    float pressure = clamp(.27 + .36 * pulse + .41 * low + .72 * plug - .36 * leak, .10, 1.0);
    vec2 stretch = abs(q) / vec2(.39 + .088 * pressure, .40 + .08 * pressure);
    float squircle = dot(stretch * stretch, stretch * stretch);
    float pocket = 1.0 - smoothstep(.93, 1.02, squircle);
    float dome = sqrt(max(0.0, 1.0 - squircle)) * pressure;
    float seam = lineBand(squircle - .94, .02, .065);
    float mesh = noise(grid * 18.0) * .52 + .48 * noise(grid * 42.0);
    float flank = smoothstep(-.33, .42, q.y - q.x);
    vec3 textile = mix(vec3(.19, .16, .38), vec3(.64, .33, .62), .38 + .62 * grain(id));
    vec3 rubber = textile * (.46 + .30 * mesh + .55 * dome + .25 * flank);
    rubber += vec3(.44, .29, .47) * dome * dome * (1.10 - flank);
    vec3 col = mix(vec3(.086, .072, .13) + .07 * mesh, rubber, pocket);
    col += vec3(.24, .17, .29) * seam * (.45 + .34 * pressure);
    return vec4(col, 1.0);
}

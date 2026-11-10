// F10-01 · Storm Isobars / Squall Bow. Original Mixworks Studio scene.

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

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float highs = treble();
    float art = artwork(p);
    float push = art + .70 * artwork(p - vec2(.11, .03));
    vec2 q = vec2(p.x - motion * .08, p.y + .045 * sin(p.x * 5.0));
    float cloud = noise(q * 8.0) + .35 * noise(q * 25.0 + motion * .6);
    float bow = p.x - (.49 + .18 * sin(.82 * motion) + .085 * low)
              + 1.15 * pow(abs(p.y - .48), 2.0) - .39 * push;
    float pressure = 8.5 * bow + 1.1 * cloud + .30 * sin(10.0 * p.y + motion);
    float trailing = 1.0 - smoothstep(-.1, .06, bow);
    float isobar = lineBand(fract(pressure) - .14, .019, .049);
    float ragged = bow + .018 * (noise(p * 113.0) - .5) * (1.0 + 6.0 * highs);
    vec3 col = mix(vec3(.075, .18, .34), vec3(.26, .48, .64), .48 * cloud + .35 * trailing);
    col += vec3(.41, .60, .78) * isobar * (.39 + .42 * trailing);
    col += vec3(.93, .48, .19) * lineBand(ragged, .006 + .010 * low, .045);
    float dots = step(.89, grain(floor(p * vec2(129.0, 75.0)))) * exp(-11.0 * abs(bow + .08));
    return vec4(col + vec3(.27, .36, .49) * dots, 1.0);
}

// F01-02 - Fault Ledger / Basin Collapse. Original Mixworks Studio scene.

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 id = floor(p);
    vec2 f = fract(p);
    f *= f * (3.0 - 2.0 * f);
    return mix(mix(hash21(id), hash21(id + vec2(1.0, 0.0)), f.x),
               mix(hash21(id + vec2(0.0, 1.0)), hash21(id + vec2(1.0)), f.x), f.y);
}

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float luma(vec3 rgb) {
    return dot(rgb, vec3(.2126, .7152, .0722));
}

float bass() {
    return clamp(response * (.86 * syn_BassLevel + .60 * syn_BassHits + .30 * syn_OnBeat), 0.0, 1.0);
}

// Studio applies media position, scale, grade and injection before this sample.
vec4 spriteAt(vec2 uv) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, uv);
}

float elevation(vec2 uv, float low) {
    vec2 p = uv - vec2(.44, .54);
    float basin = length(p * vec2(1.37, 1.0));
    float rim = .31 + .053 * sin(motion * 1.14) + .074 * low;
    float sag = 1.0 - smoothstep(.07, rim, basin + .021 * sin(p.x * 33.0));
    float rock = .19 * sin(p.x * 12.0 + p.y * 4.7) + .13 * sin(p.y * 14.0 - p.x * 8.0)
               + .13 * noise(p * 19.0) - (.43 + .18 * low) * sag;
    vec4 art = spriteAt(uv);
    return rock + (.77 * luma(art.rgb) - .31) * art.a * imprint;
}

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float centre = elevation(p, low);
    vec2 dx = vec2(.0038, 0.0);
    vec2 dy = vec2(0.0, .0068);
    vec2 slope = vec2(elevation(p + dx, low) - elevation(p - dx, low),
                      elevation(p + dy, low) - elevation(p - dy, low));
    vec3 normal = normalize(vec3(-slope * 15.5, .83));
    vec3 lamp = normalize(vec3(-.61 + .35 * sin(motion * .65), .61, .60));
    float light = .31 + .69 * max(dot(normal, lamp), 0.0);
    float band = fract(centre * 14.0 + .55 * p.x);
    float terrace = lineBand(band - .085, .023, .095);
    vec3 stone = mix(vec3(.125, .21, .27), vec3(.55, .78, .80),
                     smoothstep(-.59, .34, centre));
    stone = stone * light + vec3(.18, .31, .38) * terrace * (.41 + .37 * light);
    stone += vec3(.14, .17, .14) * pow(light, 7.0);
    return vec4(stone * (.87 + .19 * noise(p * 122.0)), 1.0);
}

// F01-08 - Fault Ledger / Shelf Rejoin. Original Mixworks Studio scene.

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

float jointAt(float x) {
    return .51 + .086 * sin(x * 7.2 + .57 * motion) + .019 * sin(31.0 * x - motion * .72);
}

vec4 moveFossils() {
    if (FRAMECOUNT == 0.0 || clear_fossils > .5) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(fossils, _uv);
    float dt = min(iTimeDelta, .062);
    vec2 p = _uv;
    float split = p.y - jointAt(p.x);
    float motionSide = split < 0.0 ? -1.0 : 1.0;
    float low = bass();
    vec2 drift = vec2(motionSide * (.092 + .104 * low), -.29 * split)
                 + vec2(.0, .022 * sin(p.x * 15.0 - motion));
    vec2 upstream = p - drift * dt;
    float inside = step(0.0, upstream.x) * step(upstream.x, 1.0);
    vec2 fossil = texture(fossils, upstream).rg * inside * exp(-dt * vec2(.83, 1.38));
    vec4 sprite = spriteAt(p);
    float intake = sprite.a * imprint * dt * (.87 + 5.2 * low);
    float edge = abs(sprite.a - spriteAt(p + vec2(.008, .014)).a);
    fossil += intake * vec2(.54 + .46 * luma(sprite.rgb), .86 + 2.3 * edge);
    return vec4(clamp(fossil, 0.0, 1.0), 0.0, 1.0);
}

vec4 renderMain() {
    if (PASSINDEX == 0) return moveFossils();
    vec2 p = _uv;
    float low = bass();
    float split = p.y - jointAt(p.x);
    float opening = .008 + .049 * pow(.5 + .5 * sin(motion * 1.87), 2.0) + .031 * low;
    float side = split < 0.0 ? -1.0 : 1.0;
    vec2 carved = vec2(p.x + side * opening, abs(split) - opening);
    float grain = noise(p * vec2(102.0, 81.0));
    float stratum = carved.y * 21.0 + .48 * sin(carved.x * 20.0 + side * motion) + .45 * grain;
    float line = lineBand(fract(stratum) - .20, .038, .12);
    vec2 fossil = texture(fossils, p).rg;
    vec3 stone = mix(vec3(.14, .22, .21), vec3(.46, .61, .50),
                     .16 + .29 * fract(stratum) + .37 * grain);
    stone += vec3(.24, .31, .20) * line;
    stone = mix(stone, vec3(.033, .065, .071), 1.0 - smoothstep(-.006, .004, carved.y));
    float exposed = lineBand(carved.y, .003, .024);
    stone += vec3(.59, .68, .39) * exposed * (.41 + .47 * low);
    vec3 mineral = mix(vec3(.93, .65, .29), vec3(.89, .94, .69), fossil.g);
    stone = mix(stone, mineral, clamp(fossil.r * (.36 + .54 * line) + fossil.g * .31, 0.0, .87));
    return vec4(stone, 1.0);
}

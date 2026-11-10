// F01-05 - Fault Ledger / Diagonal Shear. Original Mixworks Studio scene.

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

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    vec2 along = normalize(vec2(.77, .64));
    vec2 across = vec2(-along.y, along.x);
    vec2 q = vec2(dot(p - .5, along), dot(p - .5, across));
    vec4 art = spriteAt(p);
    vec4 upstream = spriteAt(p - .11 * along);
    float strength = clamp((art.a + .62 * upstream.a) * imprint, 0.0, 1.0);
    vec3 signal = (art.rgb * art.a + .62 * upstream.rgb * upstream.a)
                  / max(art.a + .62 * upstream.a, .001);
    float steering = 6.2831853 * (.59 * signal.r + .31 * signal.g - .26 * signal.b);
    vec2 direction = vec2(cos(steering), sin(steering));
    float slide = sin(motion * 2.8 + q.x * 5.0 + 3.9 * luma(signal))
                * (.065 + .125 * low);
    q += strength * (direction * slide + vec2(-direction.y, direction.x) * low * .041);
    float diagonal = q.y - .045 * sin(q.x * 11.0 + motion * 1.8);
    q.x += sign(diagonal) * (.053 * sin(motion * .91) + .055 * low);
    float stratum = q.y * 18.0 + 1.1 * sin(q.x * 10.1 - motion * .69) + .47 * noise(q * 30.0);
    float ridge = fract(stratum);
    float grain = noise(q * vec2(100.0, 155.0));
    float fins = lineBand(ridge - .16, .042, .12);
    vec3 col = mix(vec3(.15, .14, .27), vec3(.66, .48, .68),
                    .21 + .34 * ridge + .28 * grain);
    col += vec3(.20, .23, .35) * fins * (.53 + .37 * sin(16.0 * q.x));
    float split = lineBand(diagonal, .013 + .020 * low, .008);
    col = mix(col, vec3(.044, .038, .10), split);
    col += vec3(.62, .45, .86) * lineBand(abs(diagonal) - (.014 + .023 * low), .004, .015);
    return vec4(col, 1.0);
}

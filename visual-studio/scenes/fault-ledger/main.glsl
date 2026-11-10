// F01-01 - Fault Ledger / Ridge Unzip. Original Mixworks Studio scene.

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

float bass() {
    return clamp(response * (.86 * syn_BassLevel + .60 * syn_BassHits + .30 * syn_OnBeat), 0.0, 1.0);
}

// Studio applies media position, scale, grade and injection before this sample.
vec4 spriteAt(vec2 uv) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, uv);
}

float edgeOfArtwork(vec2 uv) {
    vec2 dx = vec2(.011, 0.0);
    vec2 dy = vec2(0.0, .0196);
    vec2 edge = vec2(spriteAt(uv + dx).a - spriteAt(uv - dx).a,
                     spriteAt(uv + dy).a - spriteAt(uv - dy).a);
    return clamp(2.25 * length(edge) * imprint, 0.0, 1.0);
}

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float silhouette = spriteAt(p).a * imprint;
    float edge = edgeOfArtwork(p);
    float capture = edgeOfArtwork(p - vec2(.018, .027));
    float ridge = .53 + .13 * sin(6.1 * p.x - .5 * motion)
                  + .042 * sin(21.0 * p.x + motion) + .046 * silhouette;
    float fork = ridge + .085 * sin(9.0 * p.x + 1.0);
    float opening = .006 + .031 * low + .028 * capture;
    float front = -.08 + 1.22 * fract(motion * .24);
    float rupture = 1.0 - smoothstep(-.045, .06, p.x - front - .14 * edge);
    float split = (p.y - ridge) * (1.0 + 1.75 * rupture);
    float grit = noise(p * vec2(83.0, 119.0));
    float fold = split * 16.0 + 1.2 * sin(7.0 * p.x + split * 5.0)
                 + .42 * grit + low * .8 * sin(12.0 * p.x - motion);
    float layer = fract(fold);
    vec3 stone = mix(vec3(.16, .086, .072), vec3(.82, .50, .30),
                     clamp(.33 + .27 * sin(floor(fold) * 1.7) + .35 * layer, 0.0, 1.0));
    stone += vec3(.42, .29, .20) * lineBand(layer, .035, .10);
    float gap = rupture * lineBand(p.y - ridge, opening, .005);
    float branch = rupture * lineBand(p.y - fork, .003 + .012 * low, .007);
    float outlineFracture = edge * (.30 + .70 * rupture);
    stone = mix(stone, vec3(.043, .023, .027), max(outlineFracture, max(gap, .70 * branch)));
    float faultLip = rupture * lineBand(abs(p.y - ridge) - opening, .002, .008);
    stone += vec3(.91, .38, .12) * (faultLip + .85 * capture * (.34 + .66 * rupture))
           * (.53 + .43 * low);
    if (alloy_MilkDropActive > .5) {
        vec3 unlit = stone;
        // Sample across the broken strata. A kick widens the flow on both banks.
        float tributary = .22 * sin(p.x * 6.2 + 1.6) + .035 * sin(p.x * 32.0);
        float sideRift = abs(p.y - ridge + tributary);
        float distanceToRift = min(min(abs(p.y - ridge), abs(p.y - fork) * 1.28), sideRift * 1.45);
        float bank = exp(-distanceToRift * (11.0 - 5.0 * low));
        float travellingFront = .52 + .48 * rupture;
        vec2 stream = vec2(fract(p.x * 1.05 - motion * .17 + .04 * grit),
                           fract(.5 + split * 2.2 + .16 * sin(p.x * 16.0 + motion)));
        vec3 plasma = texture(alloy_MilkDrop, stream).rgb;
        float mineral = max(plasma.r, max(plasma.g, plasma.b));
        float seams = lineBand(layer, .052 + .06 * low, .14);
        float arteries = max(lineBand(distanceToRift, .008 + .027 * low, .023),
                             .42 * lineBand(sideRift, .016, .022));
        float spill = travellingFront * bank * (.18 + .82 * seams);
        stone *= 1.0 - .58 * spill - .71 * arteries;
        stone += (plasma * 1.68 + vec3(.34, .07, .015) * mineral)
               * (1.6 * arteries + 1.38 * spill + .47 * faultLip);
        stone += plasma.bgr * mineral * (.25 + .22 * low) * seams;
        stone = mix(unlit, stone, alloy_MilkDropAmount);
    }
    return vec4(stone * (.77 + .30 * grit), 1.0);
}

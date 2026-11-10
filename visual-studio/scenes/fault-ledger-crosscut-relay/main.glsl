// F01-07 - Fault Ledger / Crosscut Relay. Original Mixworks Studio scene.

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

// Studio applies media position, scale, grade and injection before this sample.
vec4 spriteAt(vec2 uv) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, uv);
}

const vec2 relayGrid = vec2(20.0, 12.0);

vec4 tickRelay() {
    ivec2 cell = ivec2(_xy);
    if (FRAMECOUNT == 0.0 || clear_relay > .5) return vec4(2.0, 0.0, 0.0, 0.0);
    vec4 previous = texelFetch(relayClock, cell, 0);
    if (iTimeDelta <= 0.0) return previous;
    float trigger = step(.49, response * max(syn_OnBeat, .71 * syn_BassHits));
    float age = min(2.0, previous.r + iTimeDelta);
    if (trigger > previous.b) {
        vec4 sprite = spriteAt(_uv);
        float gate = clamp(sprite.a * (.14 + .96 * luma(sprite.rgb)) * imprint, 0.0, 1.0);
        float normalDelay = .019 + .10 * _uv.x + .067 * hash21(floor(_xy));
        return vec4(0.0, normalDelay + .33 * gate, trigger, gate);
    }
    return vec4(age, previous.g, trigger, previous.a);
}

vec4 renderMain() {
    if (PASSINDEX == 0) return tickRelay();
    vec2 p = _uv;
    vec2 grid = p * relayGrid;
    vec2 cell = fract(grid) - .5;
    vec4 state = texelFetch(relayClock, clamp(ivec2(grid), ivec2(0), ivec2(relayGrid) - 1), 0);
    float onset = state.r - state.g;
    float wave = .012 + .71 * max(onset, 0.0) * 4.0;
    float crosscut = min(abs(cell.x + .29 * cell.y), abs(cell.y - .35 * cell.x));
    float paths = lineBand(crosscut, .030, .060);
    float pulse = lineBand(length(cell * vec2(1.0, 1.1)) - wave, .042, .083)
                * step(0.0, onset) * exp(-2.5 * max(onset, 0.0));
    float alive = step(0.0, onset) * exp(-9.6 * max(onset, 0.0));
    float waiting = step(.02, state.r) * (1.0 - step(0.0, onset)) * state.a;
    float strata = fract(p.y * 29.0 + p.x * 8.8 + .9 * sin(p.x * 9.0 - motion));
    float score = lineBand(strata - .54, .021, .095);
    vec3 col = vec3(.065, .107, .126) + vec3(.073, .14, .16) * noise(p * 35.0);
    col += vec3(.14, .25, .25) * score + vec3(.12, .24, .29) * paths;
    col += vec3(.92, .54, .22) * waiting * exp(-18.0 * length(cell));
    col += vec3(.41, .91, .96) * (paths * pulse + alive * exp(-12.0 * length(cell)));
    return vec4(col, 1.0);
}

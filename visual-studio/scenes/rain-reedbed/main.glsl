// F06-01 · Rain Reedbed / Bank Gust. Original Mixworks Studio scene.

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

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
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
    float hit = percussion();
    float art = artwork(p);
    vec3 col = vec3(.043, .16, .17) + vec3(.10, .24, .22) * noise(p * vec2(7.0, 23.0));
    float rain = pow(.5 + .5 * sin(p.x * 268.0 + p.y * 21.0 + motion * 21.0), 15.0);
    col += vec3(.14, .27, .26) * rain * (.31 + hit * .68);
    // Draw rear rows first so each lower bank can occlude the one behind it.
    for (int layer = 0; layer < 5; ++layer) {
        float k = float(layer);
        float root = .71 - .22 * k;
        float height = .31 + .036 * k;
        float h = (p.y - root) / height;
        if (h < 0.0 || h > 1.0) continue;
        float front = fract(motion * .54 + k * .17);
        float gust = exp(-16.0 * abs(p.x - front)) * (.11 + .52 * hit);
        float bend = h * h * (.026 * sin(p.x * 11.0 - motion * 3.5 + k)
                              + .21 * gust + .14 * art);
        float stalk = (p.x - bend) * (39.0 - 4.0 * k) + k * 5.3;
        float seed = grain(vec2(floor(stalk), k));
        float stem = lineBand(fract(stalk) - .5, .035 + .04 * k, .036);
        float tip = (1.0 - smoothstep(.54 + .35 * seed, .60 + .35 * seed, h))
                    * (1.0 - smoothstep(.19, .66, art) * .83);
        vec3 reed = mix(vec3(.20, .36, .27), vec3(.66, .74, .34), .55 * seed + .08 * k);
        col = mix(col, reed * (.65 + .49 * h), stem * tip);
    }
    return vec4(col, 1.0);
}

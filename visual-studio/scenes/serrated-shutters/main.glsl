// F14-01 · Serrated Shutters / Concertina Chase. Original Mixworks Studio scene.

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
    float field = p.x * 13.0 + .68 * p.y;
    float blade = floor(field);
    float lip = fract(field);
    float art = artwork(vec2((blade + .5 - .68 * p.y) / 13.0, p.y));
    float chase = pow(.5 + .5 * sin(blade * .83 - motion * 2.9), 2.0);
    float pivot = clamp(.22 + .40 * chase + .20 * hit + .64 * art, .1, .94);
    float cover = 1.0 - smoothstep(pivot - .008, pivot + .008, lip);
    float fold = lip / max(pivot, .05);
    float bevel = lineBand(lip - pivot, .007, .019);
    float set = sin(p.x * 40.0 + p.y * 49.0 + 2.0 * sin(p.x * 11.0 + motion));
    vec3 behind = mix(vec3(.082, .23, .31), vec3(.49, .79, .71), .32 + .29 * set + .18 * hit);
    float rib = lineBand(fract(p.y * 12.0 + blade * .11) - .04, .013, .031);
    vec3 metal = mix(vec3(.21, .09, .10), vec3(.87, .40, .24), .27 + .57 * clamp(fold, 0.0, 1.0));
    metal += vec3(.13, .18, .18) * (rib + .52 * noise(p * 134.0));
    vec3 col = mix(behind, metal, cover);
    col += vec3(.52, .24, .09) * bevel * (.60 + hit);
    return vec4(col, 1.0);
}

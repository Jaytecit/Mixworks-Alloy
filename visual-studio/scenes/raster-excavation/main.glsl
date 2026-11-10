// F19-01 · Raster Excavation / Rolling Sweep. Original Mixworks Studio scene.

float grain(vec2 id) {
    return fract(sin(dot(id, vec2(127.1, 311.7))) * 43758.5453);
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
    float sweep = 1.28 * fract(motion * .55) - .14;
    float scan = p.y + .075 * sin(p.x * 7.0 + motion) - sweep;
    float cut = 1.0 - smoothstep(-.015, .017, scan);
    float lattice = floor(p.x * 48.0) + .48 * floor(p.y * 29.0);
    float bed = cut * (2.1 + 1.15 * hit) + 3.60 * art
              + .70 * grain(vec2(lattice, floor(p.y * 25.0)));
    float tier = floor(bed);
    vec2 voxel = fract(p * vec2(48.0, 29.0));
    float rim = 1.0 - smoothstep(.018, .12, min(voxel.x, voxel.y));
    float stripe = .5 + .5 * sin(p.x * (39.0 + 20.0 * tier) + p.y * 57.0 + tier * 3.1);
    vec3 rock = .51 + .39 * cos(vec3(.0, 2.1, 4.1) + tier * 1.66 + .65);
    vec3 col = rock * (.45 + .42 * stripe + .14 * rim);
    col += vec3(.31, .65, .44) * lineBand(scan, .005 + .009 * hit, .024);
    return vec4(col, 1.0);
}

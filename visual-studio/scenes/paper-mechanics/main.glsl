// F03-01 · Paper Mechanics / Fan Unfurl. Original Mixworks Studio scene.

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
    vec2 p = _uvc - vec2(-.67, -.44);
    float angle = atan(p.y, p.x);
    float hit = percussion();
    float art = artwork(_uv);
    float fold = angle * (8.0 + 2.1 * sin(motion * .9) + hit * 2.8)
               + .82 * art + .28 * sin(length(p) * 4.0 - motion);
    float face = fract(fold);
    float front = smoothstep(.48, .55, face);
    float facet = mix(face * 1.9, (1.0 - face) * 2.08, front);
    float hinge = lineBand(face - .52, .012, .025);
    float radius = length(p);
    float rows = fract(radius * 8.0 + .21 * floor(fold));
    float shortEdge = lineBand(rows - .05, .01, .019);
    float scores = lineBand(fract(radius * 28.0 + .2 * fold) - .50, .027, .020);
    vec3 faceA = vec3(.90, .74, .51);
    vec3 faceB = vec3(.21, .39, .43);
    vec3 col = mix(faceA, faceB, front) * (.48 + .51 * facet);
    col = mix(col, vec3(.85, .31, .18), shortEdge * .79);
    col += vec3(.33, .28, .17) * hinge + .05 * scores;
    col *= .91 + .14 * noise(p * 122.0);
    return vec4(col, 1.0);
}

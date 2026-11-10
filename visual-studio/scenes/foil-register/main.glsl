// F13-01 · Foil Register / Ribbon Corrugation. Original Mixworks Studio scene.

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

mat2 turn(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, s, -s, c);
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

vec2 artworkSlope(vec2 uv, float spacing) {
    vec2 dx = vec2(spacing, 0.0);
    vec2 dy = vec2(0.0, spacing);
    return vec2(artwork(uv + dx) - artwork(uv - dx),
                artwork(uv + dy) - artwork(uv - dy));
}

vec4 renderMain() {
    vec2 p = turn(.33) * _uvc;
    float highs = treble();
    vec2 artNormal = artworkSlope(_uv, .013);
    float lift = artwork(_uv);
    float ribbon = p.y * 46.0 + 2.5 * sin(p.x * 6.0 + motion * .77) + 4.7 * lift;
    float crest = sin(ribbon);
    vec3 normal = normalize(vec3(.36 * cos(p.x * 9.0 + p.y * 2.3) + 3.1 * artNormal.x,
                                 crest + 4.3 * artNormal.y, .55));
    vec3 lamp = normalize(vec3(.58 * sin(motion * .98), .58 * cos(motion * .64), .75));
    float reflection = clamp(dot(normal, lamp), 0.0, 1.0);
    float glint = pow(reflection, 8.0 + 24.0 * (1.0 - highs));
    float brush = .65 + .35 * sin(p.y * 293.0 + .9 * noise(p * 112.0));
    vec3 steel = mix(vec3(.12, .10, .18), vec3(.53, .32, .46), .42 + .34 * crest);
    vec3 col = steel * (.61 + .71 * reflection);
    col += vec3(.97, .77, .49) * glint * (.79 + .28 * highs) * brush;
    col += vec3(.07, .13, .23) * brush * (.41 + .35 * sin(p.x * 8.0));
    return vec4(col, 1.0);
}

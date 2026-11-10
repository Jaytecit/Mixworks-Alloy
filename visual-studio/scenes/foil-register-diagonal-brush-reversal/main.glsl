// F13-03 - Foil Register / Diagonal Brush Reversal.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 55.00000;
const float LIB_B = 9.60000;
const vec2 LIB_AXIS = vec2(0.99318, 0.11655);
const vec2 LIB_GRID = vec2(4.0, 5.0);
const float LIB_POWER = 1.220;
const vec3 LIB_INK = vec3(1.0000, 0.7400, 0.5900);

vec2 libraryPosition;
vec2 libraryCentered() {
    return (libraryPosition - .5) * vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
}

// Shared inputs for authored sibling scenes. Studio has already placed and
// graded syn_Media in output space, including the live injection amount.
vec4 libraryMedia(vec2 p) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(p, vec2(0.0))) || any(greaterThan(p, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, p);
}

float libraryLuma(vec3 colour) {
    return dot(colour, vec3(.2126, .7152, .0722));
}

float libraryWeight(float alpha) {
    return clamp(alpha * imprint, 0.0, 1.0);
}

float libraryBeat() {
    return clamp(response * max(syn_OnBeat, max(.85 * syn_BassHits, .85 * syn_MidHighHits)), 0.0, 1.0);
}

mat2 libraryTurn(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, s, -s, c);
}

float libraryHash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 libraryMirror(vec2 p) {
    return 1.0 - abs(mod(p, 2.0) - 1.0);
}

float libraryLight(vec4 colour) {
    return colour.a * (.17 + .83 * libraryLuma(colour.rgb));
}

float libraryEdge(vec2 p, float width) {
    vec2 dx = vec2(width, 0.0);
    vec2 dy = vec2(0.0, width * 1.62);
    vec2 edge = vec2(libraryMedia(p + dx).a - libraryMedia(p - dx).a,
                     libraryMedia(p + dy).a - libraryMedia(p - dy).a);
    return libraryWeight(length(edge) * 1.7);
}

struct LibraryMark {
    vec2 shift;
    vec3 colour;
    float tint;
    float shade;
    vec3 glow;
};

LibraryMark emptyMark() {
    return LibraryMark(vec2(0.0), vec3(1.0), 0.0, 1.0, vec3(0.0));
}

float librarySignal(vec2 p) {
    vec4 pixel = libraryMedia(p);
    return .35 * libraryWeight(libraryLight(pixel));
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 a = libraryMedia(p);
    vec4 b = libraryMedia(p - LIB_AXIS * .105);
    float ink = a.a + .59 * b.a;
    vec3 rgb = (a.rgb * a.a + .59 * b.rgb * b.a) / max(ink, .001);
    float angle = 6.2831853 * (.61 * rgb.r + .32 * rgb.g - .38 * rgb.b);
    vec2 direction = libraryTurn(angle) * LIB_AXIS;
    float travel = sin(motion * (2.1 + 2.0 * libraryLuma(rgb)) - dot(p, direction) * 9.0);
    mark.shift = direction * travel * (.09 + .13 * libraryBeat()) * libraryWeight(ink) * LIB_POWER;
    mark.shade = 1.0 + .16 * libraryWeight(ink) * travel;
    return mark;
}

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
float artwork(vec2 uv) { return librarySignal(uv); }

vec2 artworkSlope(vec2 uv, float spacing) {
    vec2 dx = vec2(spacing, 0.0);
    vec2 dy = vec2(0.0, spacing);
    return vec2(artwork(uv + dx) - artwork(uv - dx),
                artwork(uv + dy) - artwork(uv - dy));
}

vec4 renderFamily() {
    vec2 p = turn(.33) * libraryCentered();
    float highs = treble();
    vec2 artNormal = artworkSlope(libraryPosition, .013);
    float lift = artwork(libraryPosition);
    float ribbon = p.y * LIB_A + 2.5 * sin(p.x * LIB_B + motion * .77) + 4.7 * lift;
    float crest = sin(ribbon);
    vec3 normal = normalize(vec3(.36 * cos(p.x * 9.0 + p.y * 2.3) + 3.1 * artNormal.x,
                                 crest + 4.3 * artNormal.y, .55));
    vec3 lamp = normalize(vec3(.58 * sin(motion * .98), .58 * cos(motion * .64), .75));
    float reflection = clamp(dot(normal, lamp), 0.0, 1.0);
    float glint = pow(reflection, 8.0 + 24.0 * (1.0 - highs));
    float brush = .65 + .35 * sin(p.y * (LIB_A * 6.3) + .9 * noise(p * 112.0));
    vec3 steel = mix(vec3(.12, .10, .18), vec3(.53, .32, .46), .42 + .34 * crest);
    vec3 col = steel * (.61 + .71 * reflection);
    col += vec3(.97, .77, .49) * glint * (.79 + .28 * highs) * brush;
    col += vec3(.07, .13, .23) * brush * (.41 + .35 * sin(p.x * 8.0));
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .34 * q.y, .5 + q.y * 1.13);
}

vec4 renderMain() {
    libraryPosition = _uv;
    LibraryMark art = emptyMark();
    if (syn_MediaType >= .5 && imprint > 0.0) art = libraryMark(_uv);
    libraryPosition = libraryMirror(libraryLayout(_uv) + art.shift);
    vec4 surface = renderFamily();
    float material = .50 + .50 * smoothstep(.09, .68, libraryLuma(surface.rgb));
    vec3 pigment = (.18 + surface.rgb * .97) * art.colour;
    vec3 colour = mix(surface.rgb, pigment, clamp(material * art.tint, 0.0, 1.0));
    return vec4(colour * art.shade + material * art.glow, surface.a);
}

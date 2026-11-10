// F16-04 - Kinetic Typesetter / Corner Index.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 5.00000;
const float LIB_B = 14.00000;
const vec2 LIB_AXIS = vec2(-0.26289, 0.96483);
const vec2 LIB_GRID = vec2(5.0, 3.0);
const float LIB_POWER = 1.220;
const vec3 LIB_INK = vec3(0.9000, 0.7000, 0.4400);

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

vec4 libraryStamp(vec2 p) {
    vec2 tile = p * LIB_GRID;
    tile.x += .45 * mod(floor(tile.y), 2.0);
    vec2 id = floor(tile);
    vec2 uv = fract(tile) - .5;
    uv = libraryTurn(.15 * (mod(id.x + id.y, 3.0) - 1.0)) * uv;
    uv.x *= mod(id.x + id.y, 2.0) < .5 ? 1.0 : -1.0;
    uv *= 1.36 - .24 * libraryBeat();
    return libraryMedia(.5 + uv);
}

float librarySignal(vec2 p) {
    return .57 * libraryWeight(libraryStamp(p).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 image = libraryStamp(p);
    float face = libraryWeight(image.a);
    float edge = libraryWeight(libraryStamp(p - .011 * LIB_AXIS).a) - face;
    mark.shift = LIB_AXIS * .029 * LIB_POWER * face;
    mark.shade = 1.0 - .73 * face * (.48 + .52 * libraryLuma(image.rgb));
    mark.glow = LIB_INK * max(edge, 0.0) * .76;
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

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float segment(vec2 p, vec2 a, vec2 b) {
    vec2 axis = b - a;
    vec2 rel = p - a;
    return length(rel - axis * clamp(dot(rel, axis) / dot(axis, axis), 0.0, 1.0));
}

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

float typeMark(vec2 p, int letter) {
    const int forms[12] = int[12](63, 6, 91, 79, 102, 109, 125, 7, 127, 111, 119, 124);
    int mask = forms[letter];
    float d = 1.0;
    if ((mask & 1) != 0) d = min(d, segment(p, vec2(-.22, .31), vec2(.22, .31)));
    if ((mask & 2) != 0) d = min(d, segment(p, vec2(.22, .31), vec2(.22, .02)));
    if ((mask & 4) != 0) d = min(d, segment(p, vec2(.22, -.02), vec2(.22, -.31)));
    if ((mask & 8) != 0) d = min(d, segment(p, vec2(-.22, -.31), vec2(.22, -.31)));
    if ((mask & 16) != 0) d = min(d, segment(p, vec2(-.22, -.31), vec2(-.22, -.02)));
    if ((mask & 32) != 0) d = min(d, segment(p, vec2(-.22, .02), vec2(-.22, .31)));
    if ((mask & 64) != 0) d = min(d, segment(p, vec2(-.20, 0.0), vec2(.20, 0.0)));
    return 1.0 - smoothstep(.035, .063, d);
}

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float mid = middle();
    float hit = percussion();
    float column = floor(p.x * LIB_A);
    float localX = fract(p.x * LIB_A);
    float spacing = 5.6 - .84 * mid;
    float typeset = localX * spacing + .14 * sin(motion * 2.0 + column) * (1.0 - hit);
    float baseline = p.y * LIB_B + .43 * motion + .07 * sin(column + motion);
    vec2 grid = vec2(typeset, baseline);
    vec2 id = floor(grid) + vec2(column * 43.0, 0.0);
    vec2 q = fract(grid) - .5;
    vec2 centre = p - q / vec2(LIB_A * spacing, LIB_B);
    float art = artwork(centre);
    float seed = grain(id);
    int letter = int(mod(floor(seed * 12.0) + floor(art * 16.0), 12.0));
    q *= turn(.37 * (1.0 - hit) * sin(motion * 1.8 + seed * 9.0) + art * 1.1);
    float mark = typeMark(q, letter) * (1.0 - .82 * smoothstep(.20, .64, art));
    float rule = lineBand(localX - .05, .005, .010);
    vec3 paper = vec3(.91, .84, .69) * (.94 + .09 * noise(p * 154.0));
    vec3 ink = mix(vec3(.10, .14, .16), vec3(.73, .21, .16), step(.76, seed));
    return vec4(mix(paper, ink, max(mark, .72 * rule)), 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .11 * abs(q.y), u.y + .10 * abs(q.x));
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

// F17-02 - Cable Exchange / Switchyard Detour.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 11.00000;
const float LIB_B = 10.00000;
const vec2 LIB_AXIS = vec2(0.84362, 0.53695);
const vec2 LIB_GRID = vec2(4.0, 3.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(0.4500, 0.8600, 0.8200);

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

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float hit = percussion();
    vec2 board = p * vec2(LIB_A, LIB_B);
    vec2 id = floor(board);
    vec2 cell = fract(board);
    float art = artwork((id + .5) / vec2(LIB_A, LIB_B));
    float route = grain(id + 13.0);
    float gate = step(.52, route + .62 * art + .12 * hit);
    float busY = mix(.22, .76, mod(id.y + floor(id.x * .32), 2.0));
    float track = lineBand(cell.y - busY, .017, .028);
    float riser = lineBand(cell.x - .5, .022, .035) * gate;
    float node = 1.0 - smoothstep(.046, .10, length(cell - vec2(.5, busY)));
    float run = fract(board.x * .42 - motion * 1.5 + id.y * .19 - .20 * hit);
    float busPulse = lineBand(run - .28, .045, .12);
    float branchPulse = lineBand(fract(board.y * .6 + motion * 1.22 + id.x * .37) - .47, .062, .15);
    float marks = (1.0 - smoothstep(.036, .075, length(fract(board * 3.0) - .5))) * .13;
    vec3 plate = vec3(.053, .08, .11) + vec3(.09, .15, .17) * marks;
    plate += vec3(.43, .29, .10) * track * (.40 + .51 * busPulse);
    plate += vec3(.09, .47, .64) * riser * (.39 + 1.09 * branchPulse);
    plate += vec3(.34, .61, .54) * node * (.25 + .60 * gate + .45 * hit * busPulse);
    return vec4(plate, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .13 * sin(7.0 * u.y - t), u.y + .12 * q.x);
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

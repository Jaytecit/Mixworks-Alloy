// F17-03 - Cable Exchange / Ring Bypass.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 13.00000;
const float LIB_B = 8.00000;
const vec2 LIB_AXIS = vec2(0.21235, 0.97719);
const vec2 LIB_GRID = vec2(5.0, 4.0);
const float LIB_POWER = 1.135;
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

vec4 libraryTick() {
    if (FRAMECOUNT == 0.0 || clear_nested > .5) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(nestedHistory, _uv);
    float dt = min(iTimeDelta, .064);
    vec2 current = libraryTurn(.70 * sin(dot(_uv, LIB_AXIS.yx) * 10.0 - motion)) * LIB_AXIS;
    vec2 upstream = _uv - current * dt * (.12 + .14 * libraryBeat()) * LIB_POWER;
    float inside = step(0.0, upstream.x) * step(upstream.x, 1.0)
                 * step(0.0, upstream.y) * step(upstream.y, 1.0);
    vec4 trace = texture(nestedHistory, upstream) * exp(-dt * .87) * inside;
    vec4 art = libraryMedia(_uv);
    float deposit = dt * (1.05 + 4.2 * libraryBeat()) * libraryWeight(art.a);
    return clamp(trace + vec4(.27 + .73 * art.rgb, 1.0) * deposit, 0.0, 1.0);
}

float librarySignal(vec2 p) {
    return .58 * texture(nestedHistory, p).a;
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 trace = texture(nestedHistory, p);
    mark.shift = LIB_AXIS * .065 * trace.a * LIB_POWER;
    mark.colour = .24 + trace.rgb / max(trace.a, .08);
    mark.tint = .77 * trace.a;
    mark.glow = trace.rgb * LIB_INK * .27;
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
    return vec2(a, r * 1.55);
}

vec4 renderMain() {
    libraryPosition = _uv;
    if (PASSINDEX == 0) return libraryTick();
    LibraryMark art = libraryMark(_uv);
    libraryPosition = libraryMirror(libraryLayout(_uv) + art.shift);
    vec4 surface = renderFamily();
    float material = .50 + .50 * smoothstep(.09, .68, libraryLuma(surface.rgb));
    vec3 pigment = (.18 + surface.rgb * .97) * art.colour;
    vec3 colour = mix(surface.rgb, pigment, clamp(material * art.tint, 0.0, 1.0));
    return vec4(colour * art.shade + material * art.glow, surface.a);
}

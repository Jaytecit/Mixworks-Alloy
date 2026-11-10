// F19-07 - Raster Excavation / Corner Traverse.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 40.00000;
const float LIB_B = 30.00000;
const vec2 LIB_AXIS = vec2(0.41180, -0.91128);
const vec2 LIB_GRID = vec2(5.0, 3.0);
const float LIB_POWER = 1.135;
const vec3 LIB_INK = vec3(0.6500, 0.9500, 0.5700);

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
    float art = artwork(p);
    float sweep = 1.28 * fract(motion * .55) - .14;
    float scan = p.y + .075 * sin(p.x * 7.0 + motion) - sweep;
    float cut = 1.0 - smoothstep(-.015, .017, scan);
    float lattice = floor(p.x * LIB_A) + .48 * floor(p.y * LIB_B);
    float bed = cut * (2.1 + 1.15 * hit) + 3.60 * art
              + .70 * grain(vec2(lattice, floor(p.y * 25.0)));
    float tier = floor(bed);
    vec2 voxel = fract(p * vec2(LIB_A, LIB_B));
    float rim = 1.0 - smoothstep(.018, .12, min(voxel.x, voxel.y));
    float stripe = .5 + .5 * sin(p.x * (39.0 + 20.0 * tier) + p.y * 57.0 + tier * 3.1);
    vec3 rock = .51 + .39 * cos(vec3(.0, 2.1, 4.1) + tier * 1.66 + .65);
    vec3 col = rock * (.45 + .42 * stripe + .14 * rim);
    col += vec3(.31, .65, .44) * lineBand(scan, .005 + .009 * hit, .024);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + q.x * (.65 + .76 * abs(q.y)), u.y + .16 * q.x);
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

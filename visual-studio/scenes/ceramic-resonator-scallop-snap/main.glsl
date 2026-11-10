// F15-06 - Ceramic Resonator / Scallop Snap.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 8.70000;
const float LIB_B = -0.72000;
const vec2 LIB_AXIS = vec2(-0.99940, 0.03477);
const vec2 LIB_GRID = vec2(3.0, 3.0);
const float LIB_POWER = 1.050;
const vec3 LIB_INK = vec3(0.6100, 0.8900, 0.9500);

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
    return .73 * libraryWeight(libraryMedia(p).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    float solid = libraryWeight(libraryMedia(p).a);
    float rim = libraryEdge(p, .011);
    mark.shift = LIB_AXIS * (.082 * solid + .024 * rim) * LIB_POWER;
    mark.shade = 1.0 - .81 * solid;
    mark.glow = LIB_INK * rim * (.30 + .39 * libraryBeat());
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

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec2 tileCentre(vec2 p) {
    vec2 spacing = vec2(1.0, 1.73205);
    vec2 a = spacing * (floor(p / spacing) + .5);
    vec2 b = spacing * (floor(p / spacing + .5));
    return length(p - a) < length(p - b) ? a : b;
}

vec4 renderFamily() {
    vec2 p = libraryCentered() * LIB_A;
    float low = bass();
    vec2 tile = tileCentre(p);
    float art = artwork(.5 + tile * RENDERSIZE.y / (LIB_A * RENDERSIZE));
    float travel = tile.x * LIB_B - tile.y * .63 - 3.2 * motion;
    float flex = sin(travel + 2.6 * art) * (.24 + .47 * low) / (1.0 + 2.0 * art);
    vec2 q = p - tile;
    vec2 folded = abs(turn(flex) * (q - vec2(.045, -.074) * flex));
    float hex = max(folded.x, .5 * folded.x + .866 * folded.y);
    float grout = smoothstep(.443, .483, hex);
    float bevel = lineBand(hex - .423, .013, .022);
    float glaze = grain(tile * 1.4);
    float fleck = noise(p * 35.0);
    vec3 ceramic = mix(vec3(.10, .30, .46), vec3(.34, .66, .72), glaze);
    ceramic *= .83 + .24 * flex + .19 * fleck;
    ceramic += vec3(.39, .53, .48) * bevel * (.57 + .4 * sin(4.0 * q.y + flex));
    return vec4(mix(ceramic, vec3(.15, .17, .20) * (.84 + .25 * fleck), grout), 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .15 * sin(u.y * 11.0), u.y + .13 * sin(14.0 * u.x - t));
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

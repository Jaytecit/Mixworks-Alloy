// F12-04 - Shadow Telegraph / Opposed Lamps.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 11.00000;
const float LIB_B = 12.00000;
const vec2 LIB_AXIS = vec2(0.82713, 0.56201);
const vec2 LIB_GRID = vec2(4.0, 4.0);
const float LIB_POWER = 0.880;
const vec3 LIB_INK = vec3(1.0000, 0.8000, 0.5800);

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
    return .59 * libraryEdge(p, .012);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 axis = libraryTurn(.63 * sin(motion * .93)) * LIB_AXIS;
    float split = (.061 + .086 * libraryBeat()) * LIB_POWER;
    vec4 red = libraryMedia(p - axis * split);
    vec4 green = libraryMedia(p - axis * split * .24);
    vec4 blue = libraryMedia(p + axis * split);
    vec3 beam = vec3((.24 + .76 * red.r) * red.a, (.24 + .76 * green.g) * green.a,
                     (.24 + .76 * blue.b) * blue.a);
    float edge = libraryEdge(p, .012);
    mark.shift = axis * edge * .075 * LIB_POWER;
    mark.shade = 1.0 - .44 * libraryWeight(max(beam.r, max(beam.g, beam.b)));
    mark.glow = beam * clamp(imprint, 0.0, 1.4) * (.56 + .20 * edge);
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

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float mid = middle();
    vec2 light = vec2(.26 + .16 * sin(motion * 1.25), .11 + .05 * mid);
    vec2 brick = p * vec2(LIB_A, LIB_B);
    brick.x += .5 * mod(floor(brick.y), 2.0);
    vec2 seam = fract(brick);
    float mortar = 1.0 - smoothstep(.014, .047, min(seam.y, seam.x));
    vec3 wall = mix(vec3(.41, .32, .24), vec3(.94, .75, .56),
                    .58 + .18 * grain(floor(brick)) + .11 * noise(p * 135.0));
    wall *= 1.0 - .54 * mortar;
    float shade = 0.0;
    float artShade = 0.0;
    for (int ray = 0; ray < 6; ++ray) {
        float rayDepth = float(ray) / 5.0;
        vec2 q = p + light * rayDepth;
        float rail = lineBand(fract((LIB_A * .73) * q.x - .85 * q.y) - .56, .050, .020);
        float lintel = lineBand(fract(q.y * 2.0 + .14) - .12, .015, .014);
        shade = max(shade, max(rail, lintel) * (1.0 - .32 * rayDepth));
        artShade = max(artShade, artwork(q) * (1.0 - .38 * rayDepth));
    }
    float occlusion = max(.73 * shade, clamp(1.68 * artShade, 0.0, .88));
    vec3 col = mix(wall, wall * vec3(.34, .43, .63), occlusion);
    col *= .86 + .16 * sin(4.3 * p.y + p.x * 2.2 - motion);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + abs(q.x) * 1.73 - .3, u.y + .13 * q.x);
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

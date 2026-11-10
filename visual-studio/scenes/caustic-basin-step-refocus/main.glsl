// F11-03 - Caustic Basin / Step Refocus.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 18.00000;
const float LIB_B = 11.00000;
const vec2 LIB_AXIS = vec2(0.87605, -0.48222);
const vec2 LIB_GRID = vec2(5.0, 4.0);
const float LIB_POWER = 1.050;
const vec3 LIB_INK = vec3(0.6100, 0.9800, 0.8100);

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

float libraryHeight(vec2 p) {
    vec4 pixel = libraryMedia(p);
    return (libraryLuma(pixel.rgb) - .29) * libraryWeight(pixel.a);
}

float librarySignal(vec2 p) {
    return .67 * max(0.0, libraryHeight(p));
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 dx = vec2(.0048, 0.0);
    vec2 dy = vec2(0.0, .008);
    float h = libraryHeight(p);
    vec2 normal = vec2(libraryHeight(p - dx) - libraryHeight(p + dx),
                       libraryHeight(p - dy) - libraryHeight(p + dy));
    float light = clamp(dot(normal, libraryTurn(.55 * sin(motion)) * LIB_AXIS) * 4.5, -.65, .71);
    mark.shift = LIB_POWER * (LIB_AXIS * .12 * h + .032 * normal);
    mark.shade = 1.0 + light + .41 * h;
    mark.glow = LIB_INK * max(0.0, light) * (.31 + .19 * libraryBeat());
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

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
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
    vec2 p = libraryCentered();
    float low = bass();
    vec2 bend = artworkSlope(libraryPosition, .020);
    vec2 ray = p + .13 * bend;
    ray += vec2(.026 * sin(p.y * 14.0 + motion * 1.1),
                .025 * sin(p.x * 11.0 - motion * 1.4)) * (1.0 + 1.9 * low);
    vec2 flow = vec2(LIB_A * ray.x + 2.1 * ray.y + motion * 1.2,
                     LIB_B * ray.y - 4.0 * ray.x - motion * .9);
    float ribbon = sin(flow.x + 1.35 * sin(flow.y)) + sin(flow.y + 1.0 * cos(flow.x));
    float shine = exp(-11.0 * abs(ribbon));
    float twinkle = shine * (.25 + .75 * pow(.5 + .5 * cos(flow.x - flow.y), 3.0));
    float shoal = noise(p * 6.0 + vec2(2.0, 0.0));
    float bed = noise(p * 90.0) * .28 + shoal * .72;
    vec3 sand = mix(vec3(.045, .23, .27), vec3(.25, .57, .52), bed);
    float ripple = lineBand(sin(p.y * 103.0 + 5.0 * shoal), .075, .18);
    sand += vec3(.08, .16, .13) * ripple;
    sand += vec3(.49, .77, .57) * (shine * .32 + twinkle * (.32 + .31 * low));
    return vec4(sand, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x, u.y + .16 * floor(q.x * 5.0) / 3.0);
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

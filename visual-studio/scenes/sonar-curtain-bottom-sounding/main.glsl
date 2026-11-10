// F20-03 - Sonar Curtain / Bottom Sounding.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.07800;
const float LIB_B = 8.00000;
const vec2 LIB_AXIS = vec2(-0.65639, 0.75443);
const vec2 LIB_GRID = vec2(5.0, 4.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(0.4500, 0.8600, 1.0000);

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
    float source = .09 + .84 * fract((.28 + LIB_A) * motion);
    vec3 col = vec3(.041, .086, .10);
    float graph = lineBand(fract(p.x * 16.0) - .03, .009, .022)
                + lineBand(fract(p.y * 11.0) - .03, .013, .023);
    col += vec3(.11, .27, .24) * graph;
    for (int i = 0; i < 5; ++i) {
        float k = float(i);
        float depth = .10 + .20 * k + LIB_A * sin(p.x * LIB_B + k * 2.7);
        float art = artwork(vec2(p.x, depth)) + .73 * artwork(p);
        float range = depth + .25 * art + .034 * sin(p.x * 19.0 + k * 3.0 - motion * .6);
        float arrival = source - (.075 + .095 * k) - .28 * art;
        float streak = exp(-12.0 * abs(p.x - arrival)) * smoothstep(-.03, .01, source - p.x);
        float returnLine = lineBand(p.y - range, .004 + .004 * hit, .012);
        float bottom = lineBand(p.y - depth, .001, .007);
        col += vec3(.10, .23, .19) * bottom;
        col += vec3(.37, .84, .51) * returnLine * streak * (.63 + .85 * hit);
    }
    col += vec3(.28, .73, .58) * lineBand(p.x - source, .003, .018) * (.56 + .44 * hit);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x, u.y + .14 * sin(6.0 * u.x));
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

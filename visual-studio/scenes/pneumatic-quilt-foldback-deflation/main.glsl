// F25-07 - Pneumatic Quilt / Foldback Deflation.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 9.00000;
const float LIB_B = -0.49000;
const vec2 LIB_AXIS = vec2(0.75599, 0.65458);
const vec2 LIB_GRID = vec2(5.0, 3.0);
const float LIB_POWER = 1.220;
const vec3 LIB_INK = vec3(0.8700, 0.6700, 0.9300);

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

vec4 renderFamily() {
    vec2 p = libraryCentered();
    vec2 grid = turn(LIB_B) * p * LIB_A;
    vec2 id = floor(grid);
    vec2 q = fract(grid) - .5;
    vec2 centre = turn(-LIB_B) * (id + .5) / LIB_A;
    vec2 uv = .5 + centre * RENDERSIZE.y / RENDERSIZE;
    float plug = artwork(uv);
    float leak = artwork(uv + vec2(.060, -.055));
    float low = bass();
    float pulse = .5 + .5 * sin(1.3 * id.x + .82 * id.y - 3.3 * motion + 1.4 * leak);
    float pressure = clamp(.27 + .36 * pulse + .41 * low + .72 * plug - .36 * leak, .10, 1.0);
    vec2 stretch = abs(q) / vec2(.39 + .088 * pressure, .40 + .08 * pressure);
    float squircle = dot(stretch * stretch, stretch * stretch);
    float pocket = 1.0 - smoothstep(.93, 1.02, squircle);
    float dome = sqrt(max(0.0, 1.0 - squircle)) * pressure;
    float seam = lineBand(squircle - .94, .02, .065);
    float mesh = noise(grid * 18.0) * .52 + .48 * noise(grid * 42.0);
    float flank = smoothstep(-.33, .42, q.y - q.x);
    vec3 textile = mix(vec3(.19, .16, .38), vec3(.64, .33, .62), .38 + .62 * grain(id));
    vec3 rubber = textile * (.46 + .30 * mesh + .55 * dome + .25 * flank);
    rubber += vec3(.44, .29, .47) * dome * dome * (1.10 - flank);
    vec3 col = mix(vec3(.086, .072, .13) + .07 * mesh, rubber, pocket);
    col += vec3(.24, .17, .29) * seam * (.45 + .34 * pressure);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + abs(q.x) * 1.66 - .29, u.y + .16 * q.x);
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

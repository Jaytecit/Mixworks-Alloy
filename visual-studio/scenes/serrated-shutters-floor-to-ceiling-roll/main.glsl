// F14-07 - Serrated Shutters / Floor to Ceiling Roll.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 17.00000;
const float LIB_B = 1.10000;
const vec2 LIB_AXIS = vec2(-0.90252, -0.43066);
const vec2 LIB_GRID = vec2(3.0, 5.0);
const float LIB_POWER = 1.135;
const vec3 LIB_INK = vec3(0.9800, 0.5700, 0.4000);

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
    return .49 * libraryWeight(max(pixel.r, max(pixel.g, pixel.b)) * pixel.a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 rail = LIB_AXIS * (.042 + .087 * sin(dot(p, LIB_AXIS.yx) * 11.0 - motion * 1.4));
    vec4 a = libraryMedia(p - rail);
    vec4 b = libraryMedia(p - rail * 2.4);
    vec4 c = libraryMedia(p + rail * .8);
    float ink = a.a + .72 * b.a + .38 * c.a;
    vec3 rgb = (a.rgb * a.a + .72 * b.rgb * b.a + .38 * c.rgb * c.a) / max(ink, .001);
    mark.colour = .10 + 1.18 * rgb;
    mark.tint = .91 * libraryWeight(ink * .68);
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

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float hit = percussion();
    float field = p.x * LIB_A + LIB_B * p.y;
    float blade = floor(field);
    float lip = fract(field);
    float art = artwork(vec2((blade + .5 - LIB_B * p.y) / LIB_A, p.y));
    float chase = pow(.5 + .5 * sin(blade * .83 - motion * 2.9), 2.0);
    float pivot = clamp(.22 + .40 * chase + .20 * hit + .64 * art, .1, .94);
    float cover = 1.0 - smoothstep(pivot - .008, pivot + .008, lip);
    float fold = lip / max(pivot, .05);
    float bevel = lineBand(lip - pivot, .007, .019);
    float set = sin(p.x * 40.0 + p.y * 49.0 + 2.0 * sin(p.x * 11.0 + motion));
    vec3 behind = mix(vec3(.082, .23, .31), vec3(.49, .79, .71), .32 + .29 * set + .18 * hit);
    float rib = lineBand(fract(p.y * 12.0 + blade * .11) - .04, .013, .031);
    vec3 metal = mix(vec3(.21, .09, .10), vec3(.87, .40, .24), .27 + .57 * clamp(fold, 0.0, 1.0));
    metal += vec3(.13, .18, .18) * (rib + .52 * noise(p * 134.0));
    vec3 col = mix(behind, metal, cover);
    col += vec3(.52, .24, .09) * bevel * (.60 + hit);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.y, .5 - q.x + .12 * sin(u.y * 7.0));
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

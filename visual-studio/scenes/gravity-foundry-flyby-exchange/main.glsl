// F23-05 - Gravity Foundry / Flyby Exchange.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.20600;
const float LIB_B = 2.90000;
const vec2 LIB_AXIS = vec2(0.08433, -0.99644);
const vec2 LIB_GRID = vec2(4.0, 3.0);
const float LIB_POWER = 1.135;
const vec3 LIB_INK = vec3(0.8700, 0.6700, 1.0000);

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
    float low = bass();
    vec2 grid = p * vec2(105.0, 94.0);
    float star = step(.968, grain(floor(grid))) * (1.0 - smoothstep(.10, .38, length(fract(grid) - .5)));
    float haze = noise(p * 6.0) * (.62 + .38 * noise(p * 17.0));
    vec3 col = vec3(.064, .044, .127) + vec3(.17, .12, .25) * haze + vec3(.45, .41, .64) * star;
    for (int pair = 0; pair < 4; ++pair) {
        float k = float(pair);
        vec2 centre = vec2((mod(k, 2.0) - .5) * .89, (floor(k * .5) - .5) * .56);
        vec2 uv = .5 + centre * RENDERSIZE.y / RENDERSIZE;
        float nearArt = artwork(uv + vec2(.075, 0.0));
        float farArt = artwork(uv - vec2(.075, 0.0));
        float well = nearArt + farArt + .60 * artwork(libraryPosition);
        centre += vec2(.14, .07) * (nearArt - farArt);
        vec2 q = turn(.27 + k * 1.35 + well * 2.1) * (p - centre);
        q.y *= 1.48;
        float r = length(q);
        float angle = atan(q.y, q.x + .00001);
        float orbit = LIB_A + .048 * sin(angle * 2.0 + k + 1.2 * low) + .092 * well;
        float path = lineBand(r - orbit, .002, .012);
        float phase = angle - motion * (LIB_B + .12 * k) + k * 2.7 + well * 2.1;
        float tail = exp(-2.1 * mod(phase, 6.2831853));
        float kernel = 1.0 - smoothstep(.014, .024, length(q - vec2(.057, 0.0)));
        float partner = 1.0 - smoothstep(.010, .018, length(q + vec2(.057, 0.0)));
        vec3 light = mix(vec3(.96, .52, .32), vec3(.44, .72, .97), mod(k, 2.0));
        col += light * (path * (.30 + .84 * tail) + .94 * kernel + .71 * partner);
        col += light * exp(-r * 14.0) * (.37 + .28 * low);
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + abs(q.x) * 1.72 - .30, u.y + .19 * q.x);
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

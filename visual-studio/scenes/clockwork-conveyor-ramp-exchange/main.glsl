// F22-05 - Clockwork Conveyor / Ramp Exchange.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.82000;
const float LIB_B = 1.17000;
const vec2 LIB_AXIS = vec2(-0.22367, -0.97467);
const vec2 LIB_GRID = vec2(3.0, 4.0);
const float LIB_POWER = 1.050;
const vec3 LIB_INK = vec3(0.8700, 0.8200, 0.4800);

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

float percussion() {
    return clamp(response * max(syn_OnBeat, max(.86 * syn_BassHits, 1.18 * syn_HighHits)), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float hit = percussion();
    float bolt = 1.0 - smoothstep(.04, .10, length(fract(p * vec2(27.0, 16.0)) - .5));
    vec3 col = vec3(.12, .16, .16) + vec3(.19, .20, .13) * (.31 * bolt + .40 * noise(p * 81.0));
    for (int lane = 0; lane < 4; ++lane) {
        float k = float(lane);
        float y = .12 + .229 * k;
        float nominalTurn = LIB_A - .195 * k;
        float gateArt = artwork(vec2(nominalTurn, y + .05)) + .66 * artwork(vec2(.48, y));
        float gate = smoothstep(.13, .38, gateArt);
        float turnX = nominalTurn - (.11 + .035 * hit) * gate;
        vec2 corner = vec2(turnX, y);
        vec2 delta = p - corner;
        float across = length(vec2(max(delta.x, 0.0), delta.y));
        float upright = length(vec2(delta.x, min(delta.y, 0.0)));
        bool vertical = upright < across;
        float off = min(across, upright);
        float along = vertical ? turnX + max(delta.y, 0.0) : min(p.x, turnX);
        float belt = 1.0 - smoothstep(.039, .045, off);
        float lip = lineBand(off - .043, .003, .004);
        float slats = lineBand(fract(along * 48.0 - motion * 4.0) - .25, .034, .041);
        float art = gateArt + .82 * artwork(p);
        float shift = step(.18, art + .08 * hit);
        float phase = fract(along * 6.0 - motion * LIB_B - .17 * k + .30 * shift);
        float payload = lineBand(phase - .48, .19 + .11 * art, .035)
                      * (1.0 - smoothstep(.017 + .014 * art, .021 + .017 * art, off));
        vec3 tread = vec3(.23, .25, .21) * (1.0 - .48 * slats);
        col = mix(col, tread, belt);
        col = mix(col, mix(vec3(.84, .61, .20), vec3(.32, .78, .63), shift), payload);
        col += vec3(.36, .40, .30) * lip * (.61 + .32 * hit);
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .29 * q.y, u.y - .14 * q.x);
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

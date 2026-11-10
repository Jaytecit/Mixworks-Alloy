// F05-04 - Dune Telegraph / Lee Wake.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 4.90000;
const float LIB_B = 1.10000;
const vec2 LIB_AXIS = vec2(-0.00239, -1.00000);
const vec2 LIB_GRID = vec2(3.0, 5.0);
const float LIB_POWER = 1.135;
const vec3 LIB_INK = vec3(0.8800, 0.6400, 0.3400);

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

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = turn(-.34) * libraryCentered();
    float mid = middle();
    float barrier = artwork(libraryPosition);
    float lee = artwork(libraryPosition - vec2(.085, .021)) + .57 * artwork(libraryPosition - vec2(.17, .045));
    p.x += .18 * lee - .12 * barrier;
    vec2 grid = vec2(p.x * LIB_A - motion * (.62 + .35 * mid),
                     p.y * 5.3 + .15 * sin(5.0 * p.x));
    grid.x += .48 * mod(floor(grid.y), 2.0);
    vec2 cell = fract(grid) - .5;
    float crescent = cell.x + .25 - LIB_B * cell.y * cell.y;
    float bank = exp(-11.0 * abs(crescent)) * (1.0 - smoothstep(.30, .49, abs(cell.y)));
    float slip = exp(-5.0 * max(crescent, 0.0)) * smoothstep(-.08, .045, crescent) * bank;
    float wind = .5 + .5 * sin(p.y * 139.0 + 6.0 * noise(p * 13.0) - p.x * 32.0 + motion * 3.0);
    float dust = noise(p * vec2(154.0, 320.0) - vec2(motion * 2.3, 0.0));
    vec3 col = mix(vec3(.40, .29, .23), vec3(.93, .73, .47), .21 + .47 * bank + .13 * wind);
    col -= vec3(.26, .24, .23) * slip;
    col += vec3(.20, .095, .033) * dust * (.48 + .42 * mid + .90 * lee);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .11 * sin(9.0 * u.y - t), u.y);
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

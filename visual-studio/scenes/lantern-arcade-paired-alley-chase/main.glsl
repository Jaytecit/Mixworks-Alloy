// F02-07 - Lantern Arcade / Paired Alley Chase.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 1.05000;
const float LIB_B = 1.04000;
const vec2 LIB_AXIS = vec2(0.99114, -0.13279);
const vec2 LIB_GRID = vec2(3.0, 5.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(1.0000, 0.6400, 0.3300);

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
    return .35 * libraryWeight(libraryLight(pixel));
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 a = libraryMedia(p);
    vec4 b = libraryMedia(p - LIB_AXIS * .105);
    float ink = a.a + .59 * b.a;
    vec3 rgb = (a.rgb * a.a + .59 * b.rgb * b.a) / max(ink, .001);
    float angle = 6.2831853 * (.61 * rgb.r + .32 * rgb.g - .38 * rgb.b);
    vec2 direction = libraryTurn(angle) * LIB_AXIS;
    float travel = sin(motion * (2.1 + 2.0 * libraryLuma(rgb)) - dot(p, direction) * 9.0);
    mark.shift = direction * travel * (.09 + .13 * libraryBeat()) * libraryWeight(ink) * LIB_POWER;
    mark.shade = 1.0 + .16 * libraryWeight(ink) * travel;
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

float boxDistance(vec2 p, vec2 halfSize) {
    vec2 q = abs(p) - halfSize;
    return length(max(q, 0.0)) + min(0.0, max(q.x, q.y));
}

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryCentered();
    vec2 drift = p - vec2(.10 * sin(motion * .42), -.065);
    float mid = middle();
    float wallDepth = min(.52 / max(abs(drift.x), .001),
                          .34 / max(abs(drift.y), .001));
    bool wall = .52 * abs(drift.y) < .34 * abs(drift.x);
    float height = wall ? drift.y * wallDepth : drift.x * wallDepth;
    float travel = wallDepth * LIB_A + motion * LIB_B;
    float bay = floor(travel);
    float rib = lineBand(fract(travel) - .05, .028, .045);
    vec2 section = vec2(fract(travel) - .48, height);
    float art = artwork(libraryPosition);
    float window = lineBand(boxDistance(section, vec2(.24, .11 + .19 * art)), 0.0, .025);
    window = max(window, 1.0 - smoothstep(0.0, .022,
                 boxDistance(section, vec2(.23 + .25 * art, .065 + .19 * art))));
    float live = .25 + .75 * pow(.5 + .5 * cos(bay * 1.8 - motion * 3.3 + 3.0 * mid), 3.0);
    vec3 brick = mix(vec3(.15, .13, .21), vec3(.44, .25, .25), .48 + .35 * sin(bay * 2.1));
    brick *= .70 + .32 * noise(vec2(travel * 23.0, height * 72.0));
    vec3 col = brick * (wall ? 1.0 : .67) + rib * vec3(.30, .18, .13);
    if (wall) {
        col += window * live * vec3(1.0, .62, .26);
    } else if (drift.y < 0.0) {
        float shaft = artwork(libraryPosition + vec2(sign(drift.x) * .19, .18));
        col += vec3(.66, .37, .14) * live * (.19 + .86 * shaft)
             * lineBand(section.x + .20 * sin(6.0 * height), .11, .09);
    }
    float fog = 1.0 - exp(-.13 * wallDepth);
    return vec4(mix(col, vec3(.095, .14, .21) + mid * vec3(.08, .04, .02), fog), 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + (mod(u.x * 2.0, 1.0) - .5) * .82, u.y + .11 * sign(q.x) * sin(t));
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

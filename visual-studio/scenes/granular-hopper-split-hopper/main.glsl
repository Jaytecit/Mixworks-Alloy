// F24-03 - Granular Hopper / Split Hopper.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 2.00000;
const float LIB_B = 0.69000;
const vec2 LIB_AXIS = vec2(-0.92672, -0.37576);
const vec2 LIB_GRID = vec2(3.0, 3.0);
const float LIB_POWER = 0.880;
const vec3 LIB_INK = vec3(0.9900, 0.6900, 0.3400);

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
    return .63 * libraryWeight(libraryMedia(p + LIB_AXIS * .13).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 direction = libraryTurn(.56 * sin(motion * .91)) * LIB_AXIS;
    float span = (.16 + .20 * libraryBeat()) * LIB_POWER;
    float shadow = 0.0;
    for (int i = 1; i <= 5; ++i) {
        float depth = float(i) / 5.0;
        vec4 pixel = libraryMedia(p - depth * direction * span);
        shadow = max(shadow, libraryWeight(pixel.a) * (1.0 - .29 * depth));
    }
    mark.shade = 1.0 - .94 * shadow;
    mark.glow = LIB_INK * libraryEdge(p - span * direction, .009) * .37;
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
    float block = artwork(p);
    float catchment = artwork(p + vec2(0.0, .22)) + .72 * artwork(p + vec2(0.0, .37));
    float bay = floor(p.x * LIB_A);
    float side = fract(p.x * LIB_A) - .5;
    float neck = .10 * sin(bay * 3.3 + motion) + .45 * block;
    float floorY = .10 + .26 * max(0.0, .48 - abs(side - neck))
                 + .034 * sin(p.x * 17.0) + .21 * catchment;
    float heap = 1.0 - smoothstep(-.009, .014, p.y - floorY);
    float wall = abs(side - neck) - max(.046, LIB_B * (p.y - .31));
    float metal = lineBand(wall, .011, .015) * smoothstep(.29, .37, p.y);
    vec2 fall = vec2(p.x * 183.0 + .68 * sin(p.y * 28.0 + motion * 5.0),
                     (p.y + motion * .24 + .09 * hit) * 107.0);
    vec2 grid = floor(fall);
    float seed = grain(grid);
    float chute = 1.0 - smoothstep(.038, .085 + .07 * hit, abs(side - neck));
    float above = (1.0 - smoothstep(.0, .055, wall)) * smoothstep(.40, .70, p.y);
    float particles = step(.76 - .08 * hit, seed) * max(chute, .76 * above);
    float gravel = grain(floor(p * vec2(221.0, 117.0)));
    float grate = lineBand(fract(p.x * 92.0 + p.y * 36.0) - .5, .03, .036) * smoothstep(.12, .51, block);
    vec3 col = vec3(.11, .083, .10) + vec3(.14, .10, .11) * noise(p * 23.0);
    col = mix(col, mix(vec3(.35, .20, .10), vec3(.83, .59, .30), gravel), heap);
    col += vec3(.79, .51, .23) * particles * (.58 + .43 * seed) * (1.0 - .68 * block);
    col += vec3(.49, .39, .35) * (metal + .44 * grate);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + abs(q.x) * 1.72 - .33, u.y + .15 * q.x);
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

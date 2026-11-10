// F11-04 - Caustic Basin / Pebble Scatter.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 21.00000;
const float LIB_B = 21.00000;
const vec2 LIB_AXIS = vec2(0.95915, 0.28290);
const vec2 LIB_GRID = vec2(3.0, 5.0);
const float LIB_POWER = 1.220;
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

float librarySignal(vec2 p) {
    vec2 id = (floor(p * LIB_GRID * 3.0) + .5) / (LIB_GRID * 3.0);
    return .47 * libraryWeight(libraryMedia(id).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 density = LIB_GRID * 3.0;
    vec2 grid = p * density;
    vec2 cell = floor(grid);
    float coverage = 0.0;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 id = cell + vec2(float(i), float(j));
            vec2 centre = id + .5;
            vec4 core = libraryMedia(centre / density);
            float grip = libraryWeight(core.a);
            if (grip < .015) continue;
            float seed = libraryHash(id);
            float jump = .20 + .33 * sin(motion * 3.8 + seed * 9.0) + .54 * libraryBeat();
            vec2 slip = libraryTurn(seed * 4.0 - 2.0) * LIB_AXIS * clamp(jump, -.15, 1.0) * grip * .90;
            vec2 chip = libraryTurn((seed - .5) * jump) * (grid - centre - slip);
            float shape = max(abs(chip.x), abs(chip.y)) + .31 * min(abs(chip.x), abs(chip.y));
            vec4 ink = libraryMedia((centre + chip) / density);
            float face = (1.0 - smoothstep(.35, .41, shape)) * libraryWeight(ink.a);
            if (face <= coverage) continue;
            coverage = face;
            mark.shift = (chip + centre - grid) / density;
            mark.colour = mix(LIB_INK, ink.rgb, .72) * (1.14 + .49 * chip.y);
            mark.tint = .87 * face;
            mark.shade = 1.0 - .40 * face;
            mark.glow = LIB_INK * face * smoothstep(.22, .37, shape) * .45;
        }
    }
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
    return vec2(u.x + .10 * sin(16.0 * u.y - t), u.y);
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

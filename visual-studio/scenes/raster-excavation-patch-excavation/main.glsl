// F19-06 - Raster Excavation / Patch Excavation.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 30.00000;
const float LIB_B = 23.00000;
const vec2 LIB_AXIS = vec2(-0.35749, -0.93392);
const vec2 LIB_GRID = vec2(4.0, 5.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(0.6500, 0.9500, 0.5700);

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
    float art = artwork(p);
    float sweep = 1.28 * fract(motion * .55) - .14;
    float scan = p.y + .075 * sin(p.x * 7.0 + motion) - sweep;
    float cut = 1.0 - smoothstep(-.015, .017, scan);
    float lattice = floor(p.x * LIB_A) + .48 * floor(p.y * LIB_B);
    float bed = cut * (2.1 + 1.15 * hit) + 3.60 * art
              + .70 * grain(vec2(lattice, floor(p.y * 25.0)));
    float tier = floor(bed);
    vec2 voxel = fract(p * vec2(LIB_A, LIB_B));
    float rim = 1.0 - smoothstep(.018, .12, min(voxel.x, voxel.y));
    float stripe = .5 + .5 * sin(p.x * (39.0 + 20.0 * tier) + p.y * 57.0 + tier * 3.1);
    vec3 rock = .51 + .39 * cos(vec3(.0, 2.1, 4.1) + tier * 1.66 + .65);
    vec3 col = rock * (.45 + .42 * stripe + .14 * rim);
    col += vec3(.31, .65, .44) * lineBand(scan, .005 + .009 * hit, .024);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .10 * mod(floor(u.y * 4.0), 2.0), u.y + .11 * mod(floor(u.x * 5.0), 2.0));
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

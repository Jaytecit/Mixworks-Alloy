// F08-03 - Plankton Census / Baitball Escape.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.08500;
const float LIB_B = 0.05200;
const vec2 LIB_AXIS = vec2(0.13718, -0.99055);
const vec2 LIB_GRID = vec2(5.0, 4.0);
const float LIB_POWER = 1.220;
const vec3 LIB_INK = vec3(0.4400, 0.9300, 0.8300);

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

float middle() {
    return clamp(response * (1.75 * syn_MidLevel + .85 * syn_MidHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float mid = middle();
    float art = artwork(p);
    float channel = .48 + .15 * sin(6.1 * p.x - .5 * motion);
    float water = noise(p * vec2(9.0, 27.0) - vec2(motion * .6, 0.0));
    vec3 col = mix(vec3(.058, .11, .22), vec3(.13, .40, .44),
                   .20 + .32 * water + .35 * exp(-7.0 * abs(p.y - channel)));
    for (int lane = 0; lane < 5; ++lane) {
        float k = float(lane);
        float way = mod(k, 2.0) * 2.0 - 1.0;
        float route = channel + (k - 2.0) * (LIB_A - .033 * mid);
        float cover = artwork(p - vec2(.075 * way, 0.0));
        float jet = art * (.085 + .027 * mid) + .14 * cover;
        float y = p.y - route - way * jet;
        vec2 grid = vec2((p.x - way * motion * (.095 + .027 * k)) * 31.0,
                         y * (43.0 + 3.0 * mid));
        grid.x += .51 * mod(floor(grid.y), 2.0);
        vec2 id = floor(grid);
        vec2 q = fract(grid) - .5;
        float seed = grain(id + 57.0 * k);
        q.y += .10 * sin(seed * 15.0 + motion * 5.0 + q.x * 4.0);
        float body = 1.0 - smoothstep(.72, 1.1, length(q / vec2(.35, .12)));
        float tail = lineBand(q.y - .27 * abs(q.x), .024, .049)
                   * smoothstep(-.48, -.10, way * q.x) * (1.0 - smoothstep(-.06, .02, way * q.x));
        float school = exp(-pow(abs(y) / (LIB_B - .024 * mid), 2.0)) * smoothstep(.11, .49, seed);
        vec3 tint = mix(vec3(.44, .96, .75), vec3(1.0, .58, .23), .5 + .5 * way);
        col += tint * min(1.0, body + tail) * school * (.43 + .41 * seed);
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(a, r * 1.56);
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

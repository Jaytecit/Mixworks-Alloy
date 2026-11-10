// F04-06 - Sediment Clock / Bar Migration.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 10.00000;
const float LIB_B = 0.63000;
const vec2 LIB_AXIS = vec2(0.95439, -0.29856);
const vec2 LIB_GRID = vec2(4.0, 5.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(0.8900, 0.5700, 0.2500);

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

vec4 storeSediment() {
    if (FRAMECOUNT == 0.0 || clear_bed > .5) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(bedHistory, libraryPosition);
    float dt = min(iTimeDelta, .066);
    vec2 upstream = libraryPosition - dt * vec2(.075 + .053 * bass(), -.16);
    float inside = step(0.0, upstream.x) * step(upstream.y, 1.0);
    float previous = texture(bedHistory, upstream).r * inside * exp(-.72 * dt);
    float grains = .45 + .55 * grain(floor(libraryPosition * vec2(128.0, 79.0)));
    return vec4(clamp(previous + 3.1 * dt * artwork(libraryPosition) * grains, 0.0, 1.0), 0.0, 0.0, 1.0);
}

vec4 renderFamily() {
    if (PASSINDEX == 0) return storeSediment();
    vec2 p = libraryPosition;
    float low = bass();
    float deposit = artwork(p) + .36 * artwork(p + vec2(0.0, .12))
                  + .80 * texture(bedHistory, p).r;
    float front = .53 + .21 * sin(motion * .41) + .11 * low;
    float shelf = LIB_B - .36 * smoothstep(front - .23, front + .30, p.x)
                + .053 * sin(p.x * 11.0 + motion) + .20 * deposit;
    float slope = smoothstep(front - .3, front + .32, p.x);
    float below = shelf - p.y;
    float depth = below * (LIB_A + 12.0 * low) + 1.6 * slope + .42 * noise(p * 22.0);
    float bed = floor(depth);
    float seam = lineBand(fract(depth) - .025, .018, .080);
    float cobbles = grain(floor(p * vec2(155.0, 120.0)));
    vec3 clay = mix(vec3(.23, .10, .075), vec3(.87, .65, .35), .5 + .45 * sin(bed * 1.67));
    clay *= .63 + .31 * cobbles + .19 * (1.0 - seam);
    clay += vec3(.70, .38, .14) * deposit * smoothstep(.5, .92, cobbles);
    float stream = sin(p.y * 62.0 + 4.0 * sin(p.x * 10.0 - motion * 1.5)
                  + 2.2 * noise(p * 12.0));
    vec3 water = vec3(.06, .18, .22) + vec3(.06, .20, .18) * (.50 + .50 * stream);
    vec3 col = mix(water, clay, smoothstep(-.005, .005, below));
    col += vec3(.48, .40, .21) * lineBand(below, .002, .012) * (.36 + .45 * low);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x + .19 * sin(5.0 * u.y - t * .72), u.y + .11 * q.x);
}

vec4 renderMain() {
    libraryPosition = _uv;
    if (PASSINDEX < 1) return renderFamily();
    LibraryMark art = emptyMark();
    if (syn_MediaType >= .5 && imprint > 0.0) art = libraryMark(_uv);
    libraryPosition = libraryMirror(libraryLayout(_uv) + art.shift);
    vec4 surface = renderFamily();
    float material = .50 + .50 * smoothstep(.09, .68, libraryLuma(surface.rgb));
    vec3 pigment = (.18 + surface.rgb * .97) * art.colour;
    vec3 colour = mix(surface.rgb, pigment, clamp(material * art.tint, 0.0, 1.0));
    return vec4(colour * art.shade + material * art.glow, surface.a);
}

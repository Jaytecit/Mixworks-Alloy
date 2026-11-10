// F18-03 - Barograph / Opposed Paper Feeds.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 51.00000;
const float LIB_B = 42.00000;
const vec2 LIB_AXIS = vec2(-0.09587, 0.99539);
const vec2 LIB_GRID = vec2(3.0, 3.0);
const float LIB_POWER = 1.220;
const vec3 LIB_INK = vec3(0.7700, 0.4500, 0.3000);

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

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
}

float treble() {
    return clamp(response * (3.3 * syn_HighLevel + 1.15 * syn_HighHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 writeTrace() {
    vec2 p = libraryPosition;
    if (reset_trace > .5 || FRAMECOUNT == 0.0) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(chart, p);
    float shift = min(iTimeDelta, .066) * (.16 + .16 * paper_rate);
    vec4 trace = p.x > shift ? texture(chart, p - vec2(shift, 0.0)) : vec4(0.0);
    if (p.x > shift) return trace * exp(-.29 * iTimeDelta);

    float lane = floor(p.y * 7.0);
    float age = 1.0 - p.x / max(shift, .00001);
    float wobble = sin(motion * (2.6 + .31 * lane) + lane * 1.6);
    float level = mix(bass(), treble(), lane / 6.0);
    float sampleY = (lane + .5) / 7.0;
    float art = .72 * artwork(vec2(.5 + .28 * wobble, sampleY)) +
                .45 * artwork(vec2(.38, sampleY));
    float nib = .50 + .077 * wobble + (.12 + .28 * level) * sin(lane + motion * 9.0)
                + .46 * art + .065 * age * cos(lane + motion * 9.0);
    float mark = lineBand(fract(p.y * 7.0) - nib, .020, .021) * (1.0 - .52 * art);
    return vec4(mark, mark * (.24 + .66 * level), mark * art, 1.0);
}

vec4 renderFamily() {
    if (PASSINDEX == 0) return writeTrace();
    vec2 p = libraryPosition;
    vec2 grid = p * vec2(LIB_A, LIB_B);
    grid.x -= motion * 11.0;
    float graph = 1.0 - smoothstep(.012, .092, min(fract(grid.x), fract(grid.y)));
    float lane = lineBand(fract(p.y * 7.0) - .5, .009, .022);
    vec4 trace = texture(chart, p);
    float guide = lineBand(fract(p.y * 7.0) - .50 - .06 * sin(motion * 3.0 - p.x * (LIB_A * .58)), .006, .016);
    vec3 paper = vec3(.88, .79, .65) * (.94 + .11 * noise(p * 142.0));
    paper = mix(paper, vec3(.65, .37, .29), .32 * graph + .33 * lane);
    vec3 pen = mix(vec3(.15, .19, .22), vec3(.75, .21, .14), clamp(trace.g + trace.b, 0.0, 1.0));
    paper = mix(paper, vec3(.42, .35, .30), .34 * guide * (1.0 - trace.r));
    return vec4(mix(paper, pen, trace.r), 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x * (q.y < 0.0 ? -1.0 : 1.0), u.y + .13 * q.x);
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

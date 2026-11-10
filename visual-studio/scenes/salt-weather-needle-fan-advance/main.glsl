// F07-02 - Salt Weather / Needle Fan Advance.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 11.00000;
const float LIB_B = 19.00000;
const vec2 LIB_AXIS = vec2(-0.82056, -0.57156);
const vec2 LIB_GRID = vec2(3.0, 4.0);
const float LIB_POWER = 0.965;
const vec3 LIB_INK = vec3(0.7200, 0.8900, 1.0000);

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
    return .68 * libraryEdge(p, .017);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 perpendicular = vec2(-LIB_AXIS.y, LIB_AXIS.x);
    float thread = dot(p, perpendicular) * (8.0 + LIB_GRID.y * 6.0)
                 + .68 * sin(14.0 * dot(p, LIB_AXIS) - motion * 2.1);
    float ribbon = 1.0 - smoothstep(.08, .27, abs(fract(thread) - .5));
    vec2 sourceUV = p - LIB_AXIS * (.11 + .08 * sin(thread + motion));
    vec4 art = libraryMedia(sourceUV);
    float strand = ribbon * libraryWeight(libraryLight(art) + .27 * art.a);
    mark.shift = perpendicular * .086 * strand * LIB_POWER;
    mark.shade = 1.0 - .31 * strand;
    mark.glow = mix(LIB_INK, art.rgb, .44) * strand * (.71 + .43 * libraryBeat());
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

float treble() {
    return clamp(response * (3.3 * syn_HighLevel + 1.15 * syn_HighHits), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float salt = artwork(p);
    float highs = treble();
    float front = .60 +  .21 * sin(motion * (LIB_A * .048)) + .17 * highs
                + .09 * sin(p.x * 9.0) + .31 * salt;
    vec2 rock = p * vec2(LIB_A, LIB_B);
    vec2 id = floor(rock);
    vec2 q = fract(rock) - .5;
    vec2 plane = abs(turn(PI * floor(grain(id) * 6.0) / 6.0 + .93 * salt) * q);
    float hex = max(plane.x, .5 * plane.x + .866 * plane.y);
    float grow = smoothstep(-.06, .08, front - p.y + .11 * grain(id));
    float facet = 1.0 - smoothstep(.09 + .32 * grow, .17 + .33 * grow, hex);
    float split = lineBand(plane.x - .58 * plane.y, .01, .032);
    float gloss = max(split, lineBand(hex - .35 * grow, .012, .055));
    float ice = grow * facet;
    vec3 brine = vec3(.10, .22, .23) + vec3(.15, .21, .16) * noise(p * 64.0);
    vec3 crystal = mix(vec3(.40, .54, .68), vec3(.94, .97, .99), .32 + .62 * gloss);
    vec3 col = mix(brine, crystal, ice);
    col += vec3(.43, .63, .68) * lineBand(p.y - front, .001, .019) * (.20 + .52 * highs);
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + (a - .5) * .91, r * 1.61);
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

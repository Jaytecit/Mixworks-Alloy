// F09-02 - Fumarole Field / Basin Chimneys.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.15000;
const float LIB_B = 0.08300;
const vec2 LIB_AXIS = vec2(-0.33574, -0.94196);
const vec2 LIB_GRID = vec2(5.0, 5.0);
const float LIB_POWER = 1.135;
const vec3 LIB_INK = vec3(0.9100, 0.6300, 0.3900);

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

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryPosition;
    float low = bass();
    float ridge = .25 + .048 * sin(p.x * 20.0) + .04 * sin(p.x * 8.0 - 1.2);
    float ash = noise(p * vec2(63.0, 25.0));
    float ground = 1.0 - smoothstep(-.006, .014, p.y - ridge);
    vec3 col = mix(vec3(.15, .18, .25) + vec3(.12, .14, .16) * ash,
                   vec3(.22, .14, .10) + vec3(.28, .16, .08) * ash, ground);
    float obstruction = artwork(p);
    for (int vent = 0; vent < 8; ++vent) {
        float k = float(vent);
        float x = (k + .5) / 8.0;
        float base = .25 + .048 * sin(x * 20.0) + .04 * sin(x * 8.0 - 1.2);
        float h = p.y - base;
        float valve = artwork(vec2(x, .35)) + .78 * artwork(vec2(x, .55));
        float surge = .44 + .32 * sin(k * 1.46 + motion * 2.8) + .62 * low;
        float neck = x + (LIB_A + .25 * valve) * h * h * sin(4.0 * h - motion + k)
                     + .13 * h * obstruction;
        float width = .012 + h * (LIB_B + .12 * valve);
        float density = exp(-pow(abs(p.x - neck) / max(width, .006), 2.0))
                      * smoothstep(-.018, .026, h) * exp(-max(h, 0.0) * 1.4);
        vec2 vapor = vec2(p.x * 41.0, p.y * 12.0 - motion * (1.6 + .4 * low));
        float billow = smoothstep(.20, .87, noise(vapor + k * 14.0) + .24 * ash);
        col = mix(col, mix(vec3(.52, .31, .20), vec3(.86, .79, .68), smoothstep(.01, .40, h)),
                  clamp(density * surge * billow * (1.0 - .56 * valve), 0.0, .91));
        col += vec3(.78, .37, .13) * lineBand(p.x - x, .004, .014) * lineBand(h, .005, .02) * surge;
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(u.x, u.y + .17 * sin(u.x * 5.0));
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

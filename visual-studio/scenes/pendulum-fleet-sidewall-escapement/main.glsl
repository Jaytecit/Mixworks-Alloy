// F21-03 - Pendulum Fleet / Sidewall Escapement.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.31000;
const float LIB_B = 1.70000;
const vec2 LIB_AXIS = vec2(-0.85524, 0.51823);
const vec2 LIB_GRID = vec2(3.0, 3.0);
const float LIB_POWER = 1.050;
const vec3 LIB_INK = vec3(0.9600, 0.7300, 0.4200);

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

float segment(vec2 p, vec2 a, vec2 b) {
    vec2 axis = b - a;
    vec2 rel = p - a;
    return length(rel - axis * clamp(dot(rel, axis) / dot(axis, axis), 0.0, 1.0));
}

float bass() {
    return clamp(response * (.87 * syn_BassLevel + .55 * syn_BassHits + .28 * syn_OnBeat), 0.0, 1.0);
}

// Studio has already positioned and graded this media in output space.
float artwork(vec2 uv) { return librarySignal(uv); }

vec4 renderFamily() {
    vec2 p = libraryCentered();
    float low = bass();
    float grid = lineBand(fract(p.x * 18.0) - .025, .009, .022)
               + lineBand(fract(p.y * 18.0) - .025, .009, .022);
    vec3 col = vec3(.072, .10, .14) + vec3(.17, .24, .29) * (.29 * grid + .43 * noise(p * 6.0));
    for (int row = 0; row < 3; ++row) {
        float k = float(row);
        float ceiling = .51 - .326 * k;
        float rail = lineBand(p.y - ceiling, .004, .007);
        col += vec3(.27, .29, .33) * rail * (.6 + .11 * k);
        for (int arm = 0; arm < 6; ++arm) {
            float j = float(arm);
            vec2 pivot = vec2((j - 2.5) * .30 + .05 * mod(k, 2.0), ceiling);
            vec2 centre = .5 + (pivot - vec2(0.0, .18)) * RENDERSIZE.y / RENDERSIZE;
            float art = artwork(centre) + .62 * artwork(centre + vec2(.04, -.10));
            pivot += vec2(.11, -.045) * art;
            float sweep = sin(motion * (LIB_B - .22 * k) + j * 1.7 + k * 1.4 + 2.2 * art);
            float angle = sweep * (.51 + .45 * low + .40 * art);
            float armLength = LIB_A + .025 * sin(1.8 * j + k);
            vec2 weight = pivot + armLength * vec2(sin(angle), -cos(angle));
            float cable = 1.0 - smoothstep(.0023, .0060, segment(p, pivot, weight));
            vec2 knob = p - weight;
            float disk = 1.0 - smoothstep(.039, .044, length(knob));
            float rim = lineBand(length(knob) - .037, .003, .004);
            float stud = 1.0 - smoothstep(.006, .009, length(p - pivot));
            vec3 brass = mix(vec3(.66, .42, .23), vec3(.27, .68, .77), .5 + .46 * sin(k * 2.7));
            col = mix(col, vec3(.64, .65, .59), cable * (.69 + .10 * k));
            col = mix(col, brass * (.70 + 7.4 * knob.y + .31 * rim), disk);
            col += vec3(.62, .53, .37) * stud;
        }
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(1.0 - u.y, u.x + .08 * sin(u.y * 10.0));
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

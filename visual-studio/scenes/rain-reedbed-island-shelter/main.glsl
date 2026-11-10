// F06-03 - Rain Reedbed / Island Shelter.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = 0.77000;
const float LIB_B = 35.00000;
const vec2 LIB_AXIS = vec2(-0.46390, -0.88589);
const vec2 LIB_GRID = vec2(3.0, 3.0);
const float LIB_POWER = 1.050;
const vec3 LIB_INK = vec3(0.6400, 0.8700, 0.4900);

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
    float art = artwork(p);
    vec3 col = vec3(.043, .16, .17) + vec3(.10, .24, .22) * noise(p * vec2(7.0, 23.0));
    float rain = pow(.5 + .5 * sin(p.x * 268.0 + p.y * 21.0 + motion * 21.0), 15.0);
    col += vec3(.14, .27, .26) * rain * (.31 + hit * .68);
    // Draw rear rows first so each lower bank can occlude the one behind it.
    for (int layer = 0; layer < 5; ++layer) {
        float k = float(layer);
        float root = LIB_A - .22 * k;
        float height = .31 + .036 * k;
        float h = (p.y - root) / height;
        if (h < 0.0 || h > 1.0) continue;
        float front = fract(motion * .54 + k * .17);
        float gust = exp(-16.0 * abs(p.x - front)) * (.11 + .52 * hit);
        float bend = h * h * (.026 * sin(p.x * 11.0 - motion * 3.5 + k)
                              + .21 * gust + .14 * art);
        float stalk = (p.x - bend) * (LIB_B - 4.0 * k) + k * 5.3;
        float seed = grain(vec2(floor(stalk), k));
        float stem = lineBand(fract(stalk) - .5, .035 + .04 * k, .036);
        float tip = (1.0 - smoothstep(.54 + .35 * seed, .60 + .35 * seed, h))
                    * (1.0 - smoothstep(.19, .66, art) * .83);
        vec3 reed = mix(vec3(.20, .36, .27), vec3(.66, .74, .34), .55 * seed + .08 * k);
        col = mix(col, reed * (.65 + .49 * h), stem * tip);
    }
    return vec4(col, 1.0);
}

vec2 libraryLayout(vec2 u) {
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return vec2(.5 + sign(q.x) * r * 1.10, u.y + .08 * cos(10.0 * u.x));
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

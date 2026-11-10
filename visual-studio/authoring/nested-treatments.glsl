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

// @treatment cut
float librarySignal(vec2 p) {
    return .73 * libraryWeight(libraryMedia(p).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    float solid = libraryWeight(libraryMedia(p).a);
    float rim = libraryEdge(p, .011);
    mark.shift = LIB_AXIS * (.082 * solid + .024 * rim) * LIB_POWER;
    mark.shade = 1.0 - .81 * solid;
    mark.glow = LIB_INK * rim * (.30 + .39 * libraryBeat());
    return mark;
}

// @treatment relief
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

// @treatment pigment
float librarySignal(vec2 p) {
    vec4 pixel = libraryMedia(p);
    return .49 * libraryWeight(max(pixel.r, max(pixel.g, pixel.b)) * pixel.a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec2 rail = LIB_AXIS * (.042 + .087 * sin(dot(p, LIB_AXIS.yx) * 11.0 - motion * 1.4));
    vec4 a = libraryMedia(p - rail);
    vec4 b = libraryMedia(p - rail * 2.4);
    vec4 c = libraryMedia(p + rail * .8);
    float ink = a.a + .72 * b.a + .38 * c.a;
    vec3 rgb = (a.rgb * a.a + .72 * b.rgb * b.a + .38 * c.rgb * c.a) / max(ink, .001);
    mark.colour = .10 + 1.18 * rgb;
    mark.tint = .91 * libraryWeight(ink * .68);
    return mark;
}

// @treatment stamps
vec4 libraryStamp(vec2 p) {
    vec2 tile = p * LIB_GRID;
    tile.x += .45 * mod(floor(tile.y), 2.0);
    vec2 id = floor(tile);
    vec2 uv = fract(tile) - .5;
    uv = libraryTurn(.15 * (mod(id.x + id.y, 3.0) - 1.0)) * uv;
    uv.x *= mod(id.x + id.y, 2.0) < .5 ? 1.0 : -1.0;
    uv *= 1.36 - .24 * libraryBeat();
    return libraryMedia(.5 + uv);
}

float librarySignal(vec2 p) {
    return .57 * libraryWeight(libraryStamp(p).a);
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 image = libraryStamp(p);
    float face = libraryWeight(image.a);
    float edge = libraryWeight(libraryStamp(p - .011 * LIB_AXIS).a) - face;
    mark.shift = LIB_AXIS * .029 * LIB_POWER * face;
    mark.shade = 1.0 - .73 * face * (.48 + .52 * libraryLuma(image.rgb));
    mark.glow = LIB_INK * max(edge, 0.0) * .76;
    return mark;
}

// @treatment flow
float librarySignal(vec2 p) {
    vec4 pixel = libraryMedia(p);
    return .35 * libraryWeight(libraryLight(pixel));
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 a = libraryMedia(p);
    vec4 b = libraryMedia(p - LIB_AXIS * .105);
    float ink = a.a + .59 * b.a;
    vec3 rgb = (a.rgb * a.a + .59 * b.rgb * b.a) / max(ink, .001);
    float angle = 6.2831853 * (.61 * rgb.r + .32 * rgb.g - .38 * rgb.b);
    vec2 direction = libraryTurn(angle) * LIB_AXIS;
    float travel = sin(motion * (2.1 + 2.0 * libraryLuma(rgb)) - dot(p, direction) * 9.0);
    mark.shift = direction * travel * (.09 + .13 * libraryBeat()) * libraryWeight(ink) * LIB_POWER;
    mark.shade = 1.0 + .16 * libraryWeight(ink) * travel;
    return mark;
}

// @treatment fragments
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

// @treatment relay
vec4 libraryTick() {
    if (FRAMECOUNT == 0.0 || clear_nested > .5) return vec4(3.0, 0.0, 0.0, 0.0);
    vec4 last = texelFetch(nestedHistory, ivec2(_xy), 0);
    if (iTimeDelta <= 0.0) return last;
    float trigger = step(.46, libraryBeat());
    if (trigger > last.b) {
        float delay = libraryWeight(libraryLight(libraryMedia(_uv)));
        return vec4(0.0, .02 + .32 * delay, trigger, delay);
    }
    return vec4(min(3.0, last.r + iTimeDelta), last.g, trigger, last.a);
}

float librarySignal(vec2 p) {
    vec4 clock = texture(nestedHistory, p);
    return .51 * clock.a * exp(-9.0 * max(clock.r - .38, 0.0));
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 clock = texture(nestedHistory, p);
    vec2 local = fract(p * vec2(textureSize(nestedHistory, 0))) - .5;
    float age = clock.r - clock.g;
    float pulse = exp(-13.0 * max(age, 0.0)) * step(0.0, age);
    float wave = exp(-15.0 * abs(length(local) - max(age, 0.0) * 2.7));
    float wait = (1.0 - step(0.0, age)) * smoothstep(.014, .09, clock.r);
    mark.shift = LIB_AXIS * .12 * clock.a * pulse * LIB_POWER;
    mark.shade = 1.0 - .68 * clock.a * wait;
    mark.glow = LIB_INK * clock.a * (pulse * wave * 1.18 + wait * exp(-9.0 * length(local)) * .30);
    return mark;
}

// @treatment memory
vec4 libraryTick() {
    if (FRAMECOUNT == 0.0 || clear_nested > .5) return vec4(0.0);
    if (iTimeDelta <= 0.0) return texture(nestedHistory, _uv);
    float dt = min(iTimeDelta, .064);
    vec2 current = libraryTurn(.70 * sin(dot(_uv, LIB_AXIS.yx) * 10.0 - motion)) * LIB_AXIS;
    vec2 upstream = _uv - current * dt * (.12 + .14 * libraryBeat()) * LIB_POWER;
    float inside = step(0.0, upstream.x) * step(upstream.x, 1.0)
                 * step(0.0, upstream.y) * step(upstream.y, 1.0);
    vec4 trace = texture(nestedHistory, upstream) * exp(-dt * .87) * inside;
    vec4 art = libraryMedia(_uv);
    float deposit = dt * (1.05 + 4.2 * libraryBeat()) * libraryWeight(art.a);
    return clamp(trace + vec4(.27 + .73 * art.rgb, 1.0) * deposit, 0.0, 1.0);
}

float librarySignal(vec2 p) {
    return .58 * texture(nestedHistory, p).a;
}

LibraryMark libraryMark(vec2 p) {
    LibraryMark mark = emptyMark();
    vec4 trace = texture(nestedHistory, p);
    mark.shift = LIB_AXIS * .065 * trace.a * LIB_POWER;
    mark.colour = .24 + trace.rgb / max(trace.a, .08);
    mark.tint = .77 * trace.a;
    mark.glow = trace.rgb * LIB_INK * .27;
    return mark;
}

// @treatment shadow
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

// @treatment prism
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

// @treatment filaments
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

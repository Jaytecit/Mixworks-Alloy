// Four competing populations advect through a persistent liquid surface.
const vec2 centre = vec2(.5);

vec4 grain(vec2 p) {
    vec4 key = vec4(dot(p, vec2(43.7, 127.1)), dot(p, vec2(73.3, 61.9)),
                    dot(p, vec2(19.7, 92.9)), dot(p, vec2(87.1, 23.3)));
    return fract(sin(key) * 29817.53);
}

vec4 populations(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3. - 2. * f);
    vec4 seed = mix(mix(grain(cell), grain(cell + vec2(1, 0)), f.x),
                    mix(grain(cell + vec2(0, 1)), grain(cell + vec2(1)), f.x), f.y);
    seed = pow(.16 + seed, vec4(4.));
    return seed / dot(seed, vec4(1.));
}

vec4 field(vec2 uv, float scale) {
    return textureLod(cells, uv, scale);
}

vec4 neighbourhood(vec2 uv, vec2 pixel, float radius, float scale) {
    vec2 x = vec2(radius * pixel.x, 0);
    vec2 y = vec2(0, radius * pixel.y);
    return (field(uv + x, scale) + field(uv - x, scale)
          + field(uv + y, scale) + field(uv - y, scale) + 2. * field(uv, scale)) / 6.;
}

vec2 foldPyramid(vec2 uv) {
    vec2 aspect = vec2(16. / 9., 1.);
    vec2 p = (uv - centre) * aspect;
    float sector = PI / 3.;
    float angle = atan(p.y, p.x) + PI / 6.;
    angle = abs(mod(angle, 2. * sector) - sector) - PI / 6.;
    return centre + length(p) * vec2(cos(angle), sin(angle)) / aspect;
}

vec4 evolve(vec2 uv) {
    vec2 pixel = 1. / vec2(textureSize(cells, 0));
    vec2 p = uv - centre;
    vec4 seed = populations(vec2(13., 7.) * uv)
              * .86 + populations(vec2(29., 17.) * uv + 3.) * .14;
    if (FRAMECOUNT == 0.) return seed;

    float stepSize = min(iTimeDelta * 30., 2.);
    if (stepSize == 0.) return field(uv, 0.);
    float beatWave = mix(syn_OnBeat, syn_BPMSin, step(1., syn_BPM));
    float drift = mix(.5 + 1.6 * syn_BassLevel - syn_HighHits,
                      (2. * beatWave - 1.) * 2.4, bpm_oscillate);
    vec2 gyre = vec2(-p.y, .58 * p.x) * sin(15. * p.x + .2 * TIME);
    vec2 current = .85 * gyre + vec2(.18 * cos(16. * p.y), drift);
    current.y += shock * (18. * exp(-18. * abs(p.y)) + 3.);
    vec2 source = uv - pixel * current * stepSize
                + p * pow(tunnel, vec2(3.)) * .034 * stepSize;
    vec4 state = field(source, 0.);
    vec4 nearField = neighbourhood(source, pixel, 6., 2.);
    vec4 farField = neighbourhood(source, pixel, 25., 4.);
    vec4 crowd = state * .76 - farField * .94;
    vec4 cycle = state.gbar - state.argb;
    vec4 fitness = crowd + (.12 + shimmer * .5) * cycle;
    state += stepSize * (.28 * (nearField - state)
                       + state * (fitness - dot(state, fitness)) + .003 * (seed - state));

    if (syn_MediaType > 0.) {
        vec4 media = texture(syn_Media, uv);
        float mask = media.a * dot(media.rgb, vec3(.2126, .7152, .0722));
        vec4 food = mix(vec4(1, 0, 0, 0), vec4(0, 0, 1, 0), media_color * .5);
        state = mix(state, food, mask * .044 * stepSize);
    }
    float refill = reset_center * (1. - smoothstep(.04, .33, length(_uvc)));
    state = mix(state, populations(uv * vec2(27., 15.) + TIME), .26 * refill);
    state = max(state, vec4(.0001));
    return state / dot(state, vec4(1.));
}

float relief(vec4 mixOfCells) {
    return length(mixOfCells);
}

vec3 shade(vec2 uv) {
    // Cam Look moves the viewpoint across the fluid. Tunnel changes its flow.
    if (mirror > .5) uv.x = .5 - abs(uv.x - .5);
    if (pyramid > .5) uv = foldPyramid(uv);
    vec2 p = uv - centre;
    uv = centre + (p + cam_look * vec2(.22, .20)) / (1. + dot(p, cam_look) * .42);

    vec2 pixel = 1. / vec2(textureSize(cells, 0));
    vec4 local = field(uv, .5);
    vec4 body = neighbourhood(uv, pixel, 9., 2.);
    float skin = relief(local);
    float bodyHeight = relief(body);
    vec2 x = vec2(pixel.x * 3., 0);
    vec2 y = vec2(0, pixel.y * 3.);
    vec2 slope = vec2(relief(field(uv + x, 1.)) - relief(field(uv - x, 1.)),
                      relief(field(uv + y, 1.)) - relief(field(uv - y, 1.)));
    vec3 normal = normalize(vec3(-slope * 12., 1.));
    vec3 lamp = normalize(vec3(-.55 + .3 * sin(TIME * .23), .66, .82));
    float diffuse = pow(max(0., dot(normal, lamp)), 2.);
    float gloss = pow(max(0., dot(normal, normalize(lamp + vec3(0, 0, 1)))), 23.);
    float membrane = exp(-19. * abs(skin - .68)) * smoothstep(.019, .090, length(slope));
    float volume = smoothstep(.505, .78, bodyHeight);

    vec3 cool = vec3(.12, .39, .60);
    vec3 warm = vec3(.91, .52, .087);
    vec3 rim = vec3(.29, .67, .97);
    if (palette == 1.) {
        cool = vec3(.025, .30, .16); warm = vec3(.13, .59, .33); rim = vec3(.04, .77, .50);
    } else if (palette == 2.) {
        cool = vec3(.11, .38, .24); warm = vec3(.88, .34, .11); rim = vec3(.98, .69, .23);
    } else if (palette == 3.) {
        cool = vec3(.27, .075, .16); warm = vec3(.86, .13, .09); rim = vec3(1., .42, .27);
    } else if (palette == 4.) {
        cool = vec3(.02, .32, .31); warm = vec3(1., .22, .035); rim = vec3(.98, .60, .15);
    }
    vec3 pigment = mix(cool, warm, smoothstep(.12, .87, body.g + .7 * body.a));
    float wash = .67 + .29 * sin(8. * uv.x - 4. * uv.y + TIME * .37);
    float energy = .63 + .67 * sqrt(syn_Level) + .30 * syn_BassLevel;
    vec3 colour = pigment * wash * (.018 + volume * (.26 + 1.04 * diffuse)) * energy;
    colour += rim * membrane * (.23 + .73 * gloss + .38 * syn_HighHits);
    colour += mix(rim, vec3(.81, .52, .96), .56) * shimmer * (.29 * volume + .50 * membrane);
    colour += rim * shock * .24 * exp(-11. * abs(p.y)) * (.2 + volume);
    return colour;
}

vec4 renderMain() {
    if (PASSINDEX == 0) return evolve(_uv);
    return vec4(shade(_uv), 1.);
}

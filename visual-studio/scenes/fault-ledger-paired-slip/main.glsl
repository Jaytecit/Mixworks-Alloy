// F01-03 - Fault Ledger / Paired Fault Slip. Original Mixworks Studio scene.

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 id = floor(p);
    vec2 f = fract(p);
    f *= f * (3.0 - 2.0 * f);
    return mix(mix(hash21(id), hash21(id + vec2(1.0, 0.0)), f.x),
               mix(hash21(id + vec2(0.0, 1.0)), hash21(id + vec2(1.0)), f.x), f.y);
}

float lineBand(float distanceToLine, float width, float feather) {
    return 1.0 - smoothstep(width, width + feather, abs(distanceToLine));
}

float bass() {
    return clamp(response * (.86 * syn_BassLevel + .60 * syn_BassHits + .30 * syn_OnBeat), 0.0, 1.0);
}

// Studio applies media position, scale, grade and injection before this sample.
vec4 spriteAt(vec2 uv) {
    if (syn_MediaType < .5 || imprint <= 0.0 ||
        any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec4(0.0);
    return texture(syn_Media, uv);
}

vec4 veinPigment(vec2 p, float side) {
    vec2 centre = vec2(.49, .51);
    vec2 flow = vec2((p.x - centre.x) * .66, (p.y - centre.y) * .86);
    float along = sin(p.y * 21.0 + p.x * 8.0 + .9 * side);
    flow.x += side * .16 * sin(2.9 * p.y + motion * .64) + .074 * along;
    flow.y += .082 * sin(p.x * 11.5 - side * motion * .58);
    vec4 a = spriteAt(centre + flow);
    vec4 b = spriteAt(centre + flow - vec2(.17 * side, .07 * along));
    float weight = a.a + .63 * b.a;
    return vec4((a.rgb * a.a + .63 * b.rgb * b.a) / max(weight, .001),
                clamp(weight * imprint, 0.0, 1.0));
}

vec4 renderMain() {
    vec2 p = _uv;
    float low = bass();
    float joint = .49 + .085 * sin(5.6 * p.y + .37 * motion) + .029 * sin(p.y * 20.0);
    float gap = p.x - joint;
    float side = gap < 0.0 ? -1.0 : 1.0;
    float slip = side * (.032 * sin(motion * 1.72) + .084 * low);
    vec2 rock = p + vec2(0.0, slip);
    float flow = rock.x * 11.4 + side * rock.y * 9.0
               + 1.06 * sin(rock.y * 12.0 + rock.x * 7.0) + .60 * noise(rock * 24.0);
    float band = fract(flow);
    float metal = lineBand(band - .40, .083 + .050 * low, .083);
    float second = lineBand(band - .79, .020, .051);
    float grit = noise(rock * 125.0);
    vec4 pigment = veinPigment(rock, side);
    vec3 rockColour = mix(vec3(.18, .14, .16), vec3(.52, .47, .39),
                          .22 + .32 * sin(floor(flow) * 2.1) + .34 * grit);
    vec3 crystal = mix(vec3(.70, .56, .32), .14 + .86 * pigment.rgb, pigment.a);
    vec3 col = rockColour + crystal * metal * (.55 + .43 * grit) + crystal * second * .26;
    float slit = lineBand(gap, .005 + .025 * low, .010);
    col = mix(col, vec3(.039, .033, .048), slit);
    col += crystal * exp(-37.0 * abs(gap)) * (.19 + .58 * low) * (1.0 - slit);
    return vec4(col, 1.0);
}

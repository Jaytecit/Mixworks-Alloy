// Original scene for Mixworks Visual Studio.
vec4 renderMain() {
    vec2 p = _uvc - camera_look * vec2(.24, .18);
    p += .025 * vec2(sin(TIME * .72), cos(TIME * .56));
    float angle = atan(p.y, p.x) + twist * length(p) * 2.8;
    float sector = angle * segments / (2. * PI);
    float side = cos((fract(sector) - .5) * 2. * PI / segments);
    float radial = max(.003, length(p) * side);
    float pulse = audio_depth * (.8 * syn_BassLevel + .25 * syn_OnBeat);
    float depth = radius * (.38 + .048 * ripples * sin(6. * angle + TIME * 2.)) / radial;
    float travel = depth + speed + pulse * .16 + .025 * syn_BPMSin;
    float ring = abs(fract(travel) - .5);
    float rail = min(fract(sector), 1. - fract(sector));
    float edge = exp(-rail * 85.);
    float rung = exp(-ring * (36. + 7. * depth)) * (1. - smoothstep(.13, .8, fwidth(travel)));
    float fog = exp(-depth * .33);
    float seams = .7 + .3 * cos(6.283185 * (fract(sector) - .5));
    vec3 walls = mix(vec3(.028, .045, .08), .22 * edge_colour, seams * .72);
    vec3 c = walls * fog * (.82 + .32 * cos(travel * 6.283185));
    if (syn_MediaType > 0.) {
        vec2 panel = vec2(fract(sector), 1. - fract(travel));
        vec4 art = texture(syn_Media, panel);
        float inset = smoothstep(.03, .13, min(panel.x, 1. - panel.x))
                    * smoothstep(.03, .13, min(panel.y, 1. - panel.y));
        c = mix(c, art.rgb * fog * (.65 + .35 * seams), art.a * inset);
    }
    c += edge_colour * (edge * .65 + rung * .35) * fog;
    float lamps = exp(-abs(rail - .12) * 110.) * exp(-ring * 26.);
    vec3 accent = mix(edge_colour.bgr, vec3(1., .39, .095), warm_lights);
    c += accent * lamps * fog * (1.1 + 5. * (1. - exp(-.6 * pulse)));
    c += edge_colour * .19 * exp(-radial * 12.) * (1. + pulse);
    c += vec3(.004, .009, .021);
    if (alloy_MilkDropActive > .5) {
        // Cylinder coordinates carry the live image through the tunnel in perspective.
        float azimuth = angle / (2. * PI);
        float flow = travel * .56 - .23 * speed + .075 * sin(angle * 4.0 + depth * 2.2);
        vec2 wrap = fract(vec2(azimuth + .075 * travel, flow));
        vec3 current = alloy_MilkDropAmount * texture(alloy_MilkDrop, wrap).rgb;
        float brilliance = max(current.r, max(current.g, current.b));
        float glass = .55 + .45 * pow(abs(cos(3.141593 * sector)), 2.);
        float transmission = (1. - .70 * smoothstep(.16, .68, fwidth(flow))) * fog;
        c += current * (.72 + .82 * brilliance + .35 * pulse) * glass * transmission;
        c += current.bgr * (edge + rung * .45) * .58 * fog;
    }
    return vec4(c, 1.);
}

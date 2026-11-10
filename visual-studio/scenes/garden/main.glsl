// Original scene for Mixworks Visual Studio.
mat2 rotate2(float a) { return mat2(cos(a), sin(a), -sin(a), cos(a)); }

vec4 renderMain() {
    if (PASSINDEX == 1) {
        vec3 garden = texture(prism, _uv).rgb;
        vec3 memory = texture(syn_FinalPass, .5 + (_uv - .5) * 1.011).rgb;
        return vec4(garden + trail * max(memory - .035, 0.) * .57, 1.);
    }
    vec2 p = _uvc - position;
    float bass = syn_BassLevel * breath;
    float a = atan(p.y, p.x) + .21 * TIME;
    float slice = 2. * PI / petals;
    float phase = mod(a + .5 * slice, slice) - .5 * slice;
    float r = length(p);
    vec2 q = r * vec2(cos(phase), sin(phase)) - vec2(.22 * spread + .025 * bass, 0.);
    q *= rotate2(.45);
    float petal = length(q / vec2(.155, .087)) - 1.;
    float inside = 1. - smoothstep(-.012, .025, petal);
    float edge = exp(-abs(petal) * 27.);
    float glass = .5 + .5 * cos(43. * q.x - 21. * q.y + 2.6 * sin(TIME * .4 + r * 22.));
    vec3 cyan = vec3(.14, .75, .91);
    vec3 sheen = mix(tint, cyan, glass);
    float core = exp(-r * 15. / (1. + .34 * bass));
    float mist = exp(-r * 3.3) * (.63 + .37 * cos(11. * r + sin(a * petals)));
    vec3 col = vec3(.018, .016, .036) + .1 * mist * mix(tint, cyan, .45) + .055 * core * tint;
    col += sheen * inside * (.11 + glass * .25 + .45 * bloom * (glass + bass));
    col += mix(sheen, vec3(.94, .85, 1.), .45) * edge * (.38 + .38 * bloom);
    float caustic = exp(-abs(sin(32. * q.x + 20. * q.y + glass * 2.)) * 7.);
    col += .16 * sheen * inside * caustic * (.4 + bloom);
    vec2 offset = vec2(sin(petal * 3. + a), cos(a * 2.)) * refraction * .06;
    vec4 art = texture(syn_Media, _uv + offset);
    col += art.rgb * art.a * inside * (.5 + .3 * glass);
    float orbit = exp(-abs(r - .36 * spread) * 240.) * (.5 + .5 * cos(a * petals));
    col += tint * (orbit * .21 + core * .18);
    return vec4(col, 1.);
}

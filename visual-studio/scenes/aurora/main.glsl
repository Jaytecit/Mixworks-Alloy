// Original scene for Mixworks Visual Studio.
mat2 turn(float a) { return mat2(cos(a), sin(a), -sin(a), cos(a)); }

vec4 renderMain() {
    vec2 p = _uvc / zoom;
    if (digital > .5) p = floor(p * 90.) / 90.;
    p *= turn(.10 * sin(TIME * .45));
    float t = TIME * .65 + .035 * syn_BPMSin;
    float r = length(p);
    float a = atan(p.y, p.x);
    float beat = .65 * syn_BassLevel + .35 * syn_OnBeat;
    float radius = .247 + .017 * beat;
    float braid = 0.;
    float silk = 0.;
    for (int i = 0; i < 14; ++i) {
        float k = float(i) / 13.;
        float wave = .046 * cos(3. * a + t + 2.5 * k)
                   + .017 * sin(8. * a - 1.3 * t + 7. * k);
        wave += .027 * ripple * sin(19. * a + 3. * t + 8. * k);
        float d = abs(r - radius - wave - (k - .5) * .18);
        float thread = exp(-d * (410. - 165. * syn_HighLevel));
        braid += thread * (.56 + .44 * sin(2. * a - k * 5. + t));
        silk += exp(-d * 50.) / 14.;
    }
    float seam = smoothstep(.91, .999, .5 + .5 * sin(a * 80. + t + r * 28.));
    float halo = exp(-abs(r - radius) * 10.);
    vec3 ink = .5 + .5 * cos(6.283185 * (vec3(.01, .26, .54) + .16 * a + palette));
    ink = mix(vec3(dot(ink, vec3(.2126, .7152, .0722))), ink, saturation);
    vec3 col = vec3(.013, .021, .037) + ink * (.12 * silk + .045 * halo);
    col += braid * (.52 + brighter * .60) * mix(ink, vec3(.73, .88, 1.), .28);
    col += rays * seam * halo * (.2 + .65 * syn_HighHits) * ink;
    vec2 historyUV = .5 + (_uv - .5) * (1.014 + .02 * ripple) * turn(.006);
    historyUV.y += glitchdown * .025 * sin(floor(_uv.x * 24.) * 17. + floor(glitch_speed * 12.));
    vec3 history = texture(syn_FinalPass, historyUV).rgb;
    col += max(history - .028, 0.) * feedback * .61;
    vec4 art = texture(syn_Media, .5 + (p * turn(.18 * braid)) * vec2(.8, 1.42));
    col += art.rgb * art.a * min(.53, silk + .25 * braid);
    col += fire_pulse * exp(-abs(r - .32) * 29.) * vec3(.96, .53, .22);
    col *= (1. - .8 * darker) * (1. - .35 * smoothstep(.35, .9, r));
    return vec4(col, 1.);
}

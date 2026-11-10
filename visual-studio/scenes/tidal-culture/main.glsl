vec4 tickTide() {
    vec4 previous = texelFetch(tideClock, ivec2(0), 0);
    if (FRAMECOUNT == 0.0) {
        previous = vec4(0.0, -1.0, 0.0, 0.0);
    }
    if (iTimeDelta <= 0.0) {
        return previous;
    }
    float attack = step(0.50, max(syn_OnBeat, 0.85 * syn_BassHits));
    float age = previous.g < 0.0 ? -1.0 : min(previous.g + iTimeDelta, 4.0);
    if (attack > previous.r && (age < 0.0 || age >= 0.16)) {
        age = 0.0;
    }
    return vec4(attack, age, 0.0, 0.0);
}

float grain(vec2 p) {
    return fract(sin(dot(p, vec2(129.13, 317.07))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(grain(cell), grain(cell + vec2(1, 0)), f.x),
               mix(grain(cell + vec2(0, 1)), grain(cell + vec2(1)), f.x), f.y);
}

float nutrients(vec2 uv) {
    vec2 bend = vec2(noise(uv * 8.0 + 5.3), noise(uv * 6.0 - 9.7));
    return noise(uv * vec2(51.0, 33.0) + 8.0 * bend);
}

vec4 initialCulture(vec2 p) {
    // A warped mix of long folds and small seeds gives the plate a live starting state.
    vec2 bend = vec2(noise(p * 5.6 + 6.7), noise(p * 4.0 - 4.2));
    vec2 q = p + 0.25 * bend;
    float strata = sin(33.0 * q.x + 21.0 * q.y + 4.7 * sin(q.y * 8.0 - q.x * 4.0));
    float grains = noise(q * vec2(61.0, 48.0));
    float ridge = 1.0 - smoothstep(0.07, 0.54, abs(strata + 0.65 * (grains - 0.5)));
    float spores = smoothstep(0.55, 0.80, noise(q * vec2(34.0, 42.0) + 12.0));
    float seed = clamp(max(ridge * (0.48 + 0.46 * grains), spores), 0.0, 1.0);
    return vec4(0.98 - 0.47 * seed, 0.38 * seed, 0.0, nutrients(_uv));
}

const vec2 waveAxis = vec2(0.78, 0.63);

float pressure(vec2 p, float age) {
    float front = -0.75 + 2.7 * max(age, 0.0);
    return beat_pressure * step(0.0, age) * exp(-9.0 * abs(dot(p, waveAxis) - front))
           * (1.0 - smoothstep(0.53, 0.64, age));
}

vec4 moveCulture(vec2 uv, vec2 p, float age) {
    if (FRAMECOUNT == 0.0) {
        return initialCulture(p);
    }
    if (iTimeDelta <= 0.0) {
        return texelFetch(culture, ivec2(_xy), 0);
    }
    float ticks = min(iTimeDelta * 30.0, 1.6);
    float bass = clamp(syn_BassLevel, 0.0, 1.0);
    float turn = sin(p.y * 8.0 + p.x * 4.0 + TIME * 0.55);
    vec2 drift = vec2(0.33 + 0.38 * turn, 0.26 * sin(p.x * 9.0 - p.y * 5.0 - TIME * 0.42));
    drift *= current * (0.44 + 1.28 * bass);
    drift += waveAxis * (3.4 * pressure(p, age));
    vec4 carried = texture(culture, uv - drift * ticks / vec2(textureSize(culture, 0)));
    // The nutrient bed is fixed. Reagents and the sprite's chemical trace travel over it.
    carried.a = texelFetch(culture, ivec2(_xy), 0).a;
    return carried;
}

vec4 react(sampler2D source, vec2 uv, float push) {
    vec4 local = texelFetch(source, ivec2(_xy), 0);
    float dt = min(iTimeDelta * 30.0, 1.35) * 0.88;
    if (dt <= 0.0) {
        return local;
    }
    vec2 texel = 1.0 / vec2(textureSize(source, 0));
    vec2 x = vec2(texel.x, 0.0);
    vec2 y = vec2(0.0, texel.y);
    vec2 axial = texture(source, uv + x).rg + texture(source, uv - x).rg
               + texture(source, uv + y).rg + texture(source, uv - y).rg;
    vec2 diagonal = texture(source, uv + x + y).rg + texture(source, uv + x - y).rg
                  + texture(source, uv - x + y).rg + texture(source, uv - x - y).rg;
    vec2 laplacian = 0.20 * axial + 0.05 * diagonal - local.rg;

    vec4 sprite = texture(syn_Media, uv);
    float luminance = dot(sprite.rgb, vec3(0.2126, 0.7152, 0.0722));
    float stencil = sprite.a * imprint;
    float grain = local.a;
    float feed = mix(0.028, 0.041, branching) - 0.003 * stencil + 0.004 * push;
    float kill = mix(0.0545, 0.0595, branching) + 0.003 * stencil * (1.0 - luminance);
    float conversion = local.r * local.g * local.g;
    vec2 growth = vec2(-conversion + feed * (1.0 - local.r), conversion - (feed + kill) * local.g);
    // The sprite sets a local reagent dose. Its RGB never goes to the display.
    // Nutrient pockets sustain new colonies and break up the footprint of solid artwork.
    float replenishment = 0.013 * smoothstep(0.53, 0.85, grain);
    growth += replenishment * (vec2(0.52, 0.35) - local.rg);
    float dose = stencil * (0.008 + 0.051 * luminance) * (0.28 + 0.72 * grain) * (1.0 + 0.75 * push);
    growth += dose * (vec2(0.48, mix(0.12, 0.41, grain)) - local.rg);
    float stain = stencil * (0.4 + 0.6 * luminance);
    if (alloy_MilkDropActive > .5) {
        // Light acts on the stored reagents. Its colours never enter the final shading.
        vec2 inlet = uv + .034 * vec2(sin(uv.y * 16.0 + TIME), cos(uv.x * 12.0 - TIME));
        vec3 light = texture(alloy_MilkDrop, inlet).rgb;
        float nutrient = dot(light, vec3(.25, .60, .15));
        float infusion = alloy_MilkDropAmount * smoothstep(.16, .72, nutrient);
        float colonies = .35 + .65 * smoothstep(.23, .72, grain);
        float catalyst = infusion * colonies * (.11 + .24 * push);
        growth += catalyst * (vec2(.19, .72) - local.rg);
        stain = max(stain, infusion * (.43 + .57 * grain));
    }
    vec2 next = clamp(local.rg + dt * (vec2(1.0, 0.49) * laplacian + growth), 0.0, 1.0);
    float trace = mix(local.b, stain, 0.025 * dt);
    return vec4(next, trace, grain);
}

float surface(vec2 uv) {
    vec2 agents = texture(culture, uv).rg;
    return agents.g + 0.26 * (1.0 - agents.r);
}

vec3 shadeCulture(vec2 uv, vec2 p, float age) {
    vec4 field = texture(culture, uv);
    float bed = field.g + 0.26 * (1.0 - field.r);
    vec2 pixel = 1.0 / vec2(textureSize(culture, 0));
    vec2 x = vec2(pixel.x * 1.6, 0.0);
    vec2 y = vec2(0.0, pixel.y * 1.6);
    vec2 slope = vec2(surface(uv + x) - surface(uv - x), surface(uv + y) - surface(uv - y));
    vec3 normal = normalize(vec3(-slope * (3.0 + relief * 13.0), 1.0));
    vec3 lamp = normalize(vec3(-0.58, 0.63, 0.78));
    float diffuse = max(dot(normal, lamp), 0.0);
    float specular = pow(max(dot(normal, normalize(lamp + vec3(0, 0, 1))), 0.0), 19.0);
    float colonies = smoothstep(0.016, 0.37, bed);
    float lip = exp(-18.0 * abs(bed - 0.17)) * smoothstep(0.012, 0.12, length(slope));
    float push = pressure(p, age);
    float highs = clamp(syn_HighLevel + 0.55 * syn_HighHits, 0.0, 1.0);

    vec3 ocean = vec3(0.026, 0.092, 0.123);
    vec3 tide = vec3(0.060, 0.59, 0.58);
    vec3 copper = vec3(1.00, 0.41, 0.15);
    if (palette == 1.0) {
        ocean = vec3(0.040, 0.11, 0.09); tide = vec3(0.30, 0.69, 0.27); copper = vec3(1.0, 0.85, 0.35);
    } else if (palette == 2.0) {
        ocean = vec3(0.067, 0.047, 0.14); tide = vec3(0.39, 0.37, 0.80); copper = vec3(0.99, 0.44, 0.61);
    }
    float mineralMix = smoothstep(0.24, 0.54, bed + 0.12 * field.b) * (0.54 + 0.43 * field.a);
    vec3 mineral = mix(tide, copper, mineralMix);
    float basin = 0.75 + 0.25 * cos(p.x * 5.0 - p.y * 3.0 + bed * 3.0);
    vec3 colour = ocean * (0.70 + 0.36 * basin + 0.40 * push);
    colour += mineral * colonies * (0.19 + 0.82 * diffuse + 0.30 * push);
    colour += copper * field.b * colonies * (0.10 + 0.30 * lip);
    colour += mix(tide, vec3(0.82, 1.0, 0.95), 0.40) * lip * (0.20 + 0.45 * specular + 0.30 * highs + 0.55 * push);
    if (alloy_MilkDropActive > .5) {
        vec3 base = colour;
        float slopeMask = smoothstep(.008, .085, length(slope));
        float membrane = exp(-26.0 * abs(bed - .36)) * slopeMask;
        float innerRim = exp(-41.0 * abs(bed - .54)) * slopeMask;
        float pigment = smoothstep(.12, .54, field.b);
        vec3 cold = vec3(.035, .65, .69);
        vec3 warm = mix(vec3(.62, .18, .72), vec3(1.0, .40, .19), smoothstep(.42, .73, field.a));
        vec3 fluorescence = mix(cold, warm, pigment);
        float granular = .68 + .32 * noise(uv * vec2(230.0, 143.0));
        float interior = smoothstep(.20, .62, bed);
        colour = ocean * .42 + fluorescence * interior * (.17 + .64 * diffuse) * granular;
        colour += cold * lip * (.44 + .65 * highs);
        colour += fluorescence * membrane * (1.0 + .50 * highs + .84 * push);
        colour += mix(fluorescence, vec3(1.0, .82, .78), .56)
                * innerRim * (.41 + .70 * specular + .41 * push);
        colour = mix(base, colour, alloy_MilkDropAmount);
    }
    return 1.0 - exp(-1.36 * colour);
}

vec4 renderMain() {
    if (PASSINDEX == 0) {
        return tickTide();
    }
    float age = texelFetch(tideClock, ivec2(0), 0).g;
    if (PASSINDEX == 1) {
        return moveCulture(_uv, _uvc, age);
    }
    if (PASSINDEX == 2) {
        return react(transported, _uv, pressure(_uvc, age));
    }
    if (PASSINDEX == 3) {
        return react(reaction, _uv, pressure(_uvc, age));
    }
    return vec4(shadeCulture(_uv, _uvc, age), 1.0);
}

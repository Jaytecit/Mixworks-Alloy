vec2 aspect() {
    return vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
}

float seedAlpha(vec2 uv) {
    if (syn_MediaType > 0.0) {
        return texture(syn_Media, uv).a;
    }
    vec2 p = (uv - 0.5) * aspect();
    float angle = atan(p.y, p.x + 0.00001);
    float outer = 0.157 + 0.044 * cos(5.0 * angle) + 0.013 * sin(3.0 * angle);
    float r = length(p);
    return demo_seed * smoothstep(0.055, 0.059, r)
                     * (1.0 - smoothstep(outer - 0.004, outer, r));
}

vec4 pulseAges() {
    vec4 history = texelFetch(pulses, ivec2(0), 0);
    if (FRAMECOUNT == 0.0) {
        history = vec4(0.0, -1.0, -1.0, -1.0);
    }
    if (iTimeDelta <= 0.0) {
        return history;
    }
    float attack = step(0.52, max(syn_OnBeat, syn_BassHits * 0.85));
    vec3 ages = mix(history.gba, history.gba + iTimeDelta, step(vec3(0.0), history.gba));
    if (attack > history.r && (ages.x < 0.0 || ages.x > 0.16)) {
        ages = vec3(0.0, ages.xy);
    }
    return vec4(attack, ages);
}

vec4 seedOutline() {
    // Store the estimated alpha crossing in each inside texel that touches air.
    const float threshold = 0.16;
    float centre = seedAlpha(_uv);
    if (centre < threshold) {
        return vec4(0.0);
    }
    vec2 pixel = 1.0 / RENDERSIZE;
    vec2 nearest = vec2(0.0);
    float best = 2.0;
    for (int axis = 0; axis < 4; ++axis) {
        vec2 direction = axis < 2 ? vec2(float(2 * axis - 1), 0.0)
                                  : vec2(0.0, float(2 * axis - 5));
        float neighbour = seedAlpha(_uv + direction * pixel);
        if (neighbour < threshold) {
            float crossing = (centre - threshold) / (centre - neighbour);
            if (crossing < best) {
                nearest = _uv + direction * pixel * crossing;
                best = crossing;
            }
        }
    }
    return vec4(nearest, 0.0, float(best <= 1.0));
}

vec4 spreadEdge(sampler2D seeds, int stride) {
    ivec2 pixel = ivec2(_xy);
    ivec2 size = ivec2(RENDERSIZE);
    vec4 nearest = vec4(0.0);
    float best = 4.0;
    // Jump flooding carries the nearest outline coordinate into empty space.
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            ivec2 neighbour = clamp(pixel + ivec2(x, y) * stride, ivec2(0), size - 1);
            vec4 candidate = texelFetch(seeds, neighbour, 0);
            vec2 delta = (candidate.xy - _uv) * aspect();
            float distanceSquared = dot(delta, delta);
            if (candidate.a > 0.5 && distanceSquared < best) {
                best = distanceSquared;
                nearest = candidate;
            }
        }
    }
    return nearest;
}

vec3 wave(float age, float distanceToEdge, vec2 seed) {
    float progress = age / travel_time;
    if (progress < 0.0 || progress >= 1.0) {
        return vec3(0.0);
    }
    float radius = reach * progress * (1.24 - 0.24 * progress);
    float grain = sin(seed.x * 247.0 + sin(seed.y * 128.0))
                * sin(seed.y * 211.0 + seed.x * 36.0);
    float front = radius - distanceToEdge;
    front += grain * detail * 0.009 * smoothstep(0.0, 0.05, radius);
    float crest = exp(-abs(front) * 380.0);
    float glow = exp(-abs(front) * 76.0);
    float ribs = 0.5 + 0.5 * cos(front * 420.0 + grain * detail * 3.0);
    float wake = smoothstep(0.0, 0.006, front) * exp(-max(front, 0.0) * 34.0);
    float filament = wake * ribs * mix(0.26, 0.62, detail);
    float fade = 1.0 - smoothstep(0.48, 1.0, progress);
    vec3 tint = mix(seed_colour, bloom_colour, smoothstep(0.04, 0.66, progress));
    return fade * (tint * (0.36 * glow + filament) + mix(tint, vec3(1.0), 0.46) * crest);
}

float mapLine(float coordinate, float frequency, float width) {
    float distanceToLine = abs(fract(coordinate * frequency + 0.5) - 0.5);
    return 1.0 - smoothstep(width, width + max(fwidth(coordinate * frequency), 0.02), distanceToLine);
}

vec3 contourField(vec2 p, float age) {
    float bass = clamp(syn_BassLevel, 0.0, 1.0);
    float highs = clamp(syn_HighLevel + 0.32 * syn_HighHits, 0.0, 1.0);
    float beat = step(0.0, age) * exp(-5.2 * max(age, 0.0));
    vec2 q = p + vec2(0.10 * sin(6.7 * p.y + 2.2 * p.x),
                      0.055 * sin(8.9 * p.x - 3.1 * p.y));
    q.y += 0.036 * bass * sin(TIME * 1.9 + 5.2 * p.x);
    // Two offset basins connect into a single engraved elevation field.
    float left = length((q - vec2(-0.57, 0.22)) * vec2(1.0, 1.18));
    float right = length((q - vec2(0.59, -0.23)) * vec2(0.90, 1.24));
    float height = -log(exp(-7.0 * left) + exp(-7.0 * right)) / 7.0;
    height += 0.022 * sin(15.0 * q.x + 6.8 * q.y);
    float ring = -0.055 + 1.5 * max(age, 0.0);
    float sweep = step(0.0, age) * exp(-10.5 * abs(height - ring))
                * (1.0 - smoothstep(0.65, 0.84, age));
    height += 0.025 * sweep + 0.012 * beat;

    float major = mapLine(height, 7.5, 0.018);
    float minor = mapLine(height, 30.0, 0.022);
    float bands = 0.5 + 0.5 * cos(height * 24.0);
    float breathingRoom = 1.0 - 0.70 * exp(-dot(p * vec2(2.6, 3.35), p * vec2(2.6, 3.35)));
    vec3 ink = mix(vec3(0.027, 0.057, 0.080), 0.19 * seed_colour, bands);
    ink *= 0.64 + 0.31 * bass + 0.30 * beat;
    ink += seed_colour * (major * (0.17 + 0.16 * sweep) + minor * (0.075 + 0.085 * highs));
    ink += bloom_colour * sweep * (0.035 + 0.14 * major + 0.065 * minor);
    return breathingRoom * ink;
}

vec4 shadeBloom() {
    float vignette = exp(-4.0 * dot(_uvc, _uvc));
    vec3 colour = mix(vec3(0.012, 0.018, 0.030), vec3(0.030, 0.055, 0.069), vignette);
    vec3 ages = texelFetch(pulses, ivec2(0), 0).gba;
    colour += backdrop * contourField(_uvc, ages.x);
    vec4 edge = texture(nearestEdge, _uv);
    float alpha = seedAlpha(_uv);
    vec4 sprite = texture(syn_Media, _uv);
    vec3 fill = alpha * (syn_MediaType > 0.0 ? sprite.rgb : seed_colour * 0.32);
    colour = colour * (1.0 - artwork * alpha * 0.65) + artwork * fill;
    if (edge.a > 0.5) {
        float distanceToEdge = length((_uv - edge.xy) * aspect());
        float ink = exp(-distanceToEdge * 430.0);
        colour += seed_colour * (0.30 * ink + 0.12 * exp(-distanceToEdge * 85.0));
        vec3 bloom = wave(ages.x, distanceToEdge, edge.xy)
                   + wave(ages.y, distanceToEdge, edge.xy)
                   + wave(ages.z, distanceToEdge, edge.xy);
        colour += bloom * (1.0 - smoothstep(0.11, 0.23, alpha));
    }
    return vec4(1.0 - exp(-1.65 * colour), 1.0);
}

vec4 renderMain() {
    switch (PASSINDEX) {
        case 0: return pulseAges();
        case 1: return seedOutline();
        case 2: return spreadEdge(outline, 32);
        case 3: return spreadEdge(jump32, 16);
        case 4: return spreadEdge(jump16, 8);
        case 5: return spreadEdge(jump8, 4);
        case 6: return spreadEdge(jump4, 2);
        case 7: return spreadEdge(jump2, 1);
        default: return shadeBloom();
    }
}

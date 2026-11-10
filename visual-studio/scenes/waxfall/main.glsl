float hashColumn(float column) {
    return fract(sin(column * 139.71 + 23.1) * 43758.5453);
}

vec4 updateBeat() {
    vec4 previous = texelFetch(beatClock, ivec2(0), 0);
    if (FRAMECOUNT == 0.0) {
        previous = vec4(0.0, -1.0, 0.0, 0.0);
    }
    if (iTimeDelta <= 0.0) {
        return previous;
    }
    float attack = step(0.55, max(syn_OnBeat, syn_BassHits * 0.85));
    float age = previous.g < 0.0 ? -1.0 : min(previous.g + iTimeDelta, 5.0);
    float beat = previous.b;
    if (attack > previous.r && (age < 0.0 || age >= 0.17)) {
        age = 0.0;
        beat = mod(beat + 1.0, 64.0);
    }
    return vec4(attack, age, beat, 0.0);
}

// Each column has a broad sag with a narrower release channel at its centre.
vec3 channelAt(float x) {
    float lane = x * rivulets;
    float column = floor(lane);
    float width = hashColumn(column);
    float offset = abs(fract(lane) - mix(0.28, 0.72, width));
    float strand = 1.0 - smoothstep(0.04, 0.21 + width * 0.12, offset);
    return vec3(strand, width, column);
}

vec4 sourceWax(vec2 uv) {
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return vec4(0.0);
    }
    if (syn_MediaType > 0.0) {
        vec4 media = texture(syn_Media, uv);
        return vec4(media.rgb * media.a, media.a);
    }
    vec2 p = (uv - vec2(0.5, 0.56)) * vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
    float angle = atan(p.y, p.x + 0.00001);
    float radius = length(p);
    float rim = 0.148 + 0.007 * cos(angle * 11.0) + 0.004 * sin(angle * 17.0);
    float alpha = demo_stamp * (1.0 - smoothstep(rim - 0.002, rim + 0.002, radius));
    float emboss = exp(-abs(radius - 0.113) * 640.0);
    float crest = exp(-abs(abs(p.x) + abs(p.y) * 0.68 - 0.060) * 390.0);
    vec3 colour = mix(vec3(0.40, 0.095, 0.14), vec3(0.95, 0.42, 0.27), 0.6 - 2.2 * p.x + p.y);
    colour += vec3(0.36, 0.18, 0.08) * (emboss + crest);
    return vec4(colour * alpha, alpha);
}

vec4 softenedWax(vec2 uv, float age, vec3 channel) {
    float melt = smoothstep(0.0, 0.11, age) * (1.0 - smoothstep(0.15, 0.47, age));
    float bottom = 1.0 - smoothstep(0.29, 0.70, uv.y);
    float slope = 0.60 + 0.40 * sin(uv.x * 32.0 + 0.8);
    float sag = melt_depth * melt * bottom * (0.051 * slope + 0.105 * channel.x);
    return sourceWax(uv + vec2(0.0, sag));
}

vec4 flowWax(vec2 uv, vec4 beat, vec3 channel) {
    if (iTimeDelta <= 0.0) {
        return texture(moltenWax, uv);
    }
    // Trace the last frame upstream. The RGB channels carry premultiplied pigment.
    float speed = mix(0.17, 0.57, gravity) * mix(0.60, 1.36, channel.y);
    float curl = sin(uv.y * 18.0 + channel.z * 2.3 + TIME * 0.7);
    vec2 upstream = uv + iTimeDelta * vec2(0.013 * curl, speed);
    vec4 old = upstream.y < 1.0 ? texture(moltenWax, upstream) : vec4(0.0);
    old *= exp(-1.44 * iTimeDelta / drip_life);

    // A brief release window deposits coloured wax only at downward-facing lips.
    float release = smoothstep(0.0, 0.04, beat.g) * (1.0 - smoothstep(0.085, 0.19, beat.g));
    if (release <= 0.0 || melt_depth <= 0.0) {
        return old;
    }
    vec4 above = softenedWax(uv + vec2(0.0, 0.017), beat.g, channel);
    float below = softenedWax(uv - vec2(0.0, 0.007), beat.g, channel).a;
    float lip = max(above.a - below, 0.0);
    float choice = mix(0.32, 1.0, hashColumn(channel.z + beat.b * 31.0));
    float strength = melt_depth * release * lip * channel.x * choice;
    float coverage = 1.0 - exp(-strength * 85.0 * iTimeDelta);
    vec3 pigment = above.rgb / max(above.a, 0.001);
    return vec4(pigment * coverage, coverage) + old * (1.0 - coverage);
}

vec3 tintWax(vec3 pigment) {
    float value = dot(pigment, vec3(0.26, 0.62, 0.12));
    vec3 amber = vec3(1.0, 0.42, 0.16) * (0.18 + 0.82 * value);
    return mix(pigment, amber, warmth);
}

vec3 waxCurrents(vec2 p, float age) {
    float bass = clamp(syn_BassLevel, 0.0, 1.0);
    float highs = clamp(syn_HighLevel + 0.28 * syn_HighHits, 0.0, 1.0);
    float beat = step(0.0, age) * exp(-5.0 * max(age, 0.0));
    float front = 0.62 - 2.1 * max(age, 0.0);
    float surge = step(0.0, age) * exp(-6.5 * abs(p.y - front))
                * (1.0 - smoothstep(0.49, 0.64, age));

    // Broad sheets fold down the frame. Thin ridges catch the moving heat.
    float y = p.y + 0.16 * surge + 0.043 * bass * sin(TIME * 1.3 + p.x * 4.0);
    float bend = 0.098 * sin(7.0 * y + 2.0 * p.x)
               + 0.064 * sin(11.0 * y - 3.0 * p.x)
               + (0.026 + 0.038 * bass) * sin(17.0 * y + 5.1 * p.x);
    float flow = p.x + bend + 0.22 * y;
    float folds = 0.5 + 0.5 * sin(24.0 * flow + 1.6 * cos(y * 6.0));
    float grain = 0.5 + 0.5 * cos(94.0 * flow + 3.0 * sin(8.0 * y - p.x * 3.0));
    float glints = pow(grain, 11.0) * (0.32 + 0.68 * folds);
    float veins = pow(folds, 6.0);
    float breathingRoom = 1.0 - 0.69 * exp(-dot(p * vec2(2.9, 3.5), p * vec2(2.9, 3.5)));

    vec3 shadow = mix(vec3(0.075, 0.026, 0.071), vec3(0.24, 0.070, 0.035), folds);
    vec3 molten = shadow * (0.57 + 0.39 * surge + 0.27 * beat + 0.26 * bass);
    molten += vec3(0.46, 0.16, 0.067) * veins * (0.25 + 0.56 * surge);
    molten += vec3(0.58, 0.29, 0.14) * glints * (0.15 + 0.50 * surge + 0.30 * highs);
    return molten * breathingRoom;
}

vec4 shadeWax(vec2 uv, vec4 beat, vec3 channel) {
    vec2 p = _uvc - vec2(0.0, 0.06);
    float halo = exp(-dot(p * vec2(2.0, 2.55), p * vec2(2.0, 2.55)));
    vec3 scene = vec3(0.015, 0.012, 0.021) + halo * vec3(0.063, 0.028, 0.034);
    scene += backdrop * waxCurrents(p, beat.g);
    vec4 trail = texture(moltenWax, uv);
    vec2 pixel = 1.0 / vec2(textureSize(moltenWax, 0));
    float side = texture(moltenWax, uv - vec2(pixel.x * 1.7, 0.0)).a;
    float edgeLight = clamp((trail.a - side) * 2.8, 0.0, 1.0);
    vec3 pigment = tintWax(trail.rgb / max(trail.a, 0.001));
    float body = smoothstep(0.014, 0.62, trail.a);
    vec3 liquid = pigment * (1.00 + 0.31 * sin(uv.y * 23.0 + channel.z))
                  + vec3(0.96, 0.62, 0.38) * edgeLight;
    scene = mix(scene, liquid, body);
    scene += vec3(0.14, 0.045, 0.030) * sqrt(trail.a);

    vec4 wax = softenedWax(uv, beat.g, channel);
    float upper = softenedWax(uv + vec2(-0.002, 0.004), beat.g, channel).a;
    float rim = max(wax.a - upper, 0.0);
    vec3 artwork = mix(wax.rgb / max(wax.a, 0.001), tintWax(wax.rgb / max(wax.a, 0.001)), 0.45);
    artwork *= 0.90 + 0.18 * halo;
    artwork += vec3(1.0, 0.72, 0.53) * rim * 0.75;
    return vec4(mix(scene, artwork, wax.a), 1.0);
}

vec4 renderMain() {
    if (PASSINDEX == 0) {
        return updateBeat();
    }
    vec4 beat = texelFetch(beatClock, ivec2(0), 0);
    vec3 channel = channelAt(_uv.x);
    if (PASSINDEX == 1) {
        return flowWax(_uv, beat, channel);
    }
    return shadeWax(_uv, beat, channel);
}

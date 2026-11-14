#version 330 core

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float motion;
uniform float imprint;
uniform float cameraSweep;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_OnBeat;
uniform float syn_Hits;
uniform float syn_MediaType;
uniform sampler2D syn_Media;

out vec4 fragColor;

const vec3 TIDE = vec3(0.09, 0.85, 0.78);
const vec3 GOLD = vec3(1.0, 0.48, 0.18);
const vec3 KEY = vec3(-0.58, 0.65, 0.49);

vec3 lens;
float radius;
float bass;
float pulse;

mat2 rotate(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat2(c, s, -s, c);
}

float box(vec3 p, vec3 size) {
    vec3 q = abs(p) - size;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float torus(vec3 p, float ringRadius, float thickness) {
    return length(vec2(length(p.xy) - ringRadius, p.z)) - thickness;
}

float wave(vec2 p) {
    return 0.55 * sin(dot(p, vec2(2.2, 2.9)) + motion * 1.5)
         + 0.24 * sin(dot(p, vec2(-4.0, 1.4)) - motion * 2.6)
         + 0.13 * sin(dot(p, vec2(5.6, -2.1)) + motion * 2.9);
}

vec4 artwork(vec2 uv) {
    if (syn_MediaType <= 0.0 || imprint <= 0.0) return vec4(0.0);
    vec2 margin = min(uv, 1.0 - uv);
    float edge = smoothstep(0.0, 0.025, min(margin.x, margin.y));
    vec4 texel = texture(syn_Media, clamp(uv, 0.0, 1.0));
    return vec4(texel.rgb, texel.a * edge * imprint);
}

vec2 structure(vec3 p) {
    vec3 q = p - lens;
    vec2 hit = vec2(length(q) - radius, 1.0);

    q.xz = rotate(-0.33 + 0.12 * sin(motion * 0.32)) * q.xz;
    q.yz = rotate(0.17 + 0.14 * bass) * q.yz;
    float angle = atan(q.y, q.x);
    float collar = torus(q, 1.71 + 0.11 * bass, 0.076);
    float edge = torus(q - vec3(0.0, 0.0, 0.065), 1.71 + 0.11 * bass, 0.024);
    if (collar < hit.x) hit = vec2(collar, 2.0);
    if (edge < hit.x) hit = vec2(edge, 3.0);

    vec3 g = p - lens;
    g.yz = rotate(1.20 + 0.15 * sin(motion * 0.29)) * g.yz;
    g.xy = rotate(-0.28) * g.xy;
    float gimbal = torus(g, 2.05, 0.051 + 0.012 * pulse);
    if (gimbal < hit.x) hit = vec2(gimbal, 4.0);
    float fixture = box(vec3(length(q.xy) - 1.72, sin(angle * 12.0) * 0.14, q.z),
                        vec3(0.12, 0.035, 0.12));
    if (fixture < hit.x) hit = vec2(fixture * 0.65, 2.0);

    float rail = length(vec2(abs(p.x) - 3.69, p.y - 0.045)) - 0.034;
    if (rail < hit.x && abs(p.z + 5.0) < 11.0) hit = vec2(rail, 6.0);

    // The repeated ribs form a long vaulted chamber with a clear central aisle.
    float bay = clamp(round((p.z + 2.9) / 3.9), -4.0, 1.0);
    vec3 bayPoint = p - vec3(0.0, 0.0, bay * 3.9 - 2.9);
    float pilaster = box(vec3(abs(p.x) - 4.45, p.y - 1.65, bayPoint.z), vec3(0.32, 1.65, 0.37));
    float foot = box(vec3(abs(p.x) - 4.45, p.y - 0.22, bayPoint.z), vec3(0.56, 0.22, 0.57));
    vec3 arch = bayPoint - vec3(0.0, 3.02, 0.0);
    float archStone = max(torus(arch, 4.44, 0.215), -arch.y);
    float archLight = max(torus(arch - vec3(0.0, 0.0, 0.16), 4.25, 0.033), -arch.y);
    float stone = min(min(pilaster, foot), archStone);
    if (stone < hit.x) hit = vec2(stone, 5.0);
    if (archLight < hit.x) hit = vec2(archLight, 6.0);

    vec3 plinth = p - vec3(0.0, 0.20, 0.0);
    float dais = max(length(plinth.xz) - 2.24, abs(plinth.y) - 0.20);
    float tread = max(length(plinth.xz) - 2.49, abs(p.y - 0.07) - 0.07);
    if (min(dais, tread) < hit.x) hit = vec2(min(dais, tread), 7.0);
    float trim = length(vec2(length(p.xz) - 2.19, p.y - 0.40)) - 0.026;
    if (trim < hit.x) hit = vec2(trim, 6.0);
    return hit;
}

vec3 normalAt(vec3 p) {
    const vec2 e = vec2(0.004, -0.004);
    return normalize(e.xyy * structure(p + e.xyy).x + e.yyx * structure(p + e.yyx).x
                   + e.yxy * structure(p + e.yxy).x + e.xxx * structure(p + e.xxx).x);
}

float sphereHit(vec3 origin, vec3 ray) {
    vec3 o = origin - lens;
    float b = dot(o, ray);
    float c = dot(o, o) - radius * radius;
    float d = b * b - c;
    return d > 0.0 ? max(0.0, -b - sqrt(d)) : 0.0;
}

vec3 lensColour(vec3 normal, vec3 ray) {
    float rim = pow(1.0 - max(dot(normal, -ray), 0.0), 2.4);
    vec3 refracted = refract(ray, normal, 0.72);
    vec3 inside = normal + refracted * 0.61;
    float longitude = atan(inside.z, inside.x) + motion * 0.34;
    float latitude = asin(clamp(inside.y, -1.0, 1.0));
    float currents = sin(8.0 * latitude + 3.0 * sin(longitude * 3.0) + motion * 0.9)
                   + 0.45 * sin(19.0 * latitude - 6.0 * longitude - motion * 1.4);
    float filament = exp(-abs(currents) * 5.0);
    float bands = smoothstep(-0.55, 1.1, currents);
    float glint = pow(max(dot(reflect(ray, normal), KEY), 0.0), 55.0);
    vec2 uv = vec2(0.5) + vec2(inside.x * 0.43, inside.y * 0.48);
    uv += 0.034 * vec2(sin(latitude * 10.0 + motion), sin(longitude * 6.0 - motion));
    vec4 art = artwork(uv);
    vec3 tone = mix(vec3(0.018, 0.12, 0.16), TIDE * 0.47, bands);
    tone += filament * (0.16 + 0.22 * pulse) * mix(TIDE, GOLD, bands * bands);
    tone = mix(tone, tone * 0.42 + art.rgb * 0.75 + TIDE * 0.25, art.a * 0.82);
    return tone + rim * (TIDE * 0.43 + GOLD * 0.22) + glint * vec3(1.35, 1.13, 0.87);
}

vec3 water(vec3 p, vec3 ray) {
    vec2 q = p.xz;
    float ripple = wave(q);
    float distanceFromLens = length(q);
    float ring = sin(distanceFromLens * 11.0 - motion * 3.6);
    float swell = exp(-distanceFromLens * 0.36) * (0.025 + 0.035 * pulse);
    vec3 n = normalize(vec3(-0.045 * (wave(q + vec2(0.08, 0.0)) - ripple) / 0.08,
                            1.0, -0.045 * (wave(q + vec2(0.0, 0.08)) - ripple) / 0.08
                            - ring * swell));
    vec3 reflected = reflect(ray, n);
    float fresnel = pow(1.0 - max(dot(-ray, n), 0.0), 3.0);
    float imageHit = sphereHit(p + vec3(0.0, 0.016, 0.0), reflected);
    vec3 colour = vec3(0.010, 0.035, 0.048);
    colour += TIDE * (0.028 + 0.053 * pulse) * exp(-distanceFromLens * 0.18) * (0.5 + 0.5 * ring);
    if (imageHit > 0.0) {
        vec3 mirrorNormal = normalize(p + reflected * imageHit - lens);
        colour += lensColour(mirrorNormal, reflected) * mix(0.29, 0.68, fresnel);
    }
    float bank = exp(-abs(abs(p.x) - 4.36) * 2.7) * (0.55 + 0.45 * ripple);
    colour += GOLD * 0.20 * bank * fresnel;
    return colour;
}

vec3 surface(vec3 p, vec3 ray, float material) {
    if (material < 1.5) return lensColour(normalize(p - lens), ray);
    vec3 n = normalAt(p);
    float diffuse = max(dot(n, KEY), 0.0);
    float reflected = pow(max(dot(reflect(ray, n), KEY), 0.0), 17.0);
    float rim = pow(1.0 - max(dot(-ray, n), 0.0), 3.0);
    if (material < 4.5) {
        float trim = material < 2.5 ? 0.16 : 0.48;
        float accents = 0.5 + 0.5 * sin(17.0 * atan(p.y - lens.y, p.x) - motion * 2.7);
        return GOLD * (trim + diffuse * 0.32 + reflected * 0.76)
             + TIDE * (rim * 0.17 + pulse * trim * accents);
    }
    if (material > 5.5 && material < 6.5) {
        float sweep = pow(0.5 + 0.5 * sin(p.z * 1.10 + motion * 2.4), 5.0);
        return mix(TIDE, GOLD, 0.28 + 0.22 * sin(p.z * 0.8)) * (0.35 + pulse * (0.22 + 0.73 * sweep));
    }

    float textureGrain = 0.50 + 0.50 * sin(p.y * 44.0 + sin(p.z * 17.0) * 0.8);
    vec3 base = vec3(0.063, 0.096, 0.116) * (0.77 + 0.23 * textureGrain);
    float distanceFromLens = length(p - lens);
    vec3 localLight = TIDE * (0.40 + 0.32 * bass) / (1.0 + 0.38 * distanceFromLens * distanceFromLens);
    vec3 colour = base * (0.43 + diffuse * 0.94) + localLight * (0.4 + 0.6 * rim) + reflected * GOLD * 0.13;
    if (material > 6.5) {
        // The artwork is projected onto the dais through the moving lens.
        vec2 projected = rotate(-0.23 + 0.11 * sin(motion * 0.47)) * p.xz;
        projected += 0.055 * vec2(sin(projected.y * 4.7 + motion), cos(projected.x * 5.0 - motion));
        vec4 art = artwork(projected * 0.225 + 0.5);
        float lattice = pow(0.5 + 0.5 * sin(14.0 * length(projected) + motion * 2.2
                        + 4.0 * sin(atan(projected.y, projected.x) * 6.0)), 5.0);
        colour += TIDE * 0.15 * lattice * (1.0 - art.a);
        colour += art.a * (TIDE * 0.22 + art.rgb * 0.95) * (0.67 + 0.36 * pulse);
    }
    return colour;
}

vec3 chamberHaze(vec3 origin, vec3 ray, float depth) {
    float t = clamp(dot(lens - origin, ray), 0.0, depth);
    float gap = length(origin + ray * t - lens);
    vec3 glow = TIDE * exp(-gap * gap * 0.82) * (0.18 + bass * 0.16);
    float shaftY = 0.0;
    if (abs(ray.y) > 0.001) shaftY = clamp((1.65 - origin.y) / ray.y, 0.0, depth);
    vec3 p = origin + ray * shaftY;
    glow += TIDE * exp(-dot(p.xz, p.xz) * 1.35) * (0.053 + 0.075 * pulse);
    return glow;
}

void main() {
    vec2 screen = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;
    bass = clamp(syn_BassLevel, 0.0, 1.0);
    pulse = clamp(0.72 * syn_OnBeat + 0.28 * syn_Hits, 0.0, 1.0);
    lens = vec3(0.0, 2.45 + 0.12 * sin(motion * 0.75) + bass * 0.22, 0.0);
    radius = 1.24 + 0.17 * bass;
    // Fly a continuous path in front of the chamber ribs. The return pass
    // crosses the aisle at a different height, with the lens still in view.
    float phase = TIME * 0.37;
    float lateral = sin(phase);
    vec3 flyby = vec3(6.0 * lateral,
                     3.15 + 1.05 * cos(phase) + 0.32 * sin(2.0 * phase),
                     8.6 - 2.65 * lateral * lateral);
    vec3 eye = mix(vec3(3.55, 3.31, 8.55), flyby, cameraSweep);
    float lookAhead = 0.19 * cameraSweep * cos(phase);
    vec3 aim = vec3(lookAhead, mix(2.35, 2.26 + 0.14 * cos(phase), cameraSweep), -0.30);
    vec3 forward = normalize(aim - eye);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    vec3 ray = normalize(forward * mix(1.38, 1.30 - 0.10 * abs(lateral), cameraSweep)
                         + screen.x * right + screen.y * up);
    float floorDepth = ray.y < -0.001 ? -eye.y / ray.y : 34.0;
    float limit = min(floorDepth, 34.0);
    float depth = 0.30;
    float material = 0.0;
    for (int i = 0; i < 94; ++i) {
        if (depth >= limit) break;
        vec2 hit = structure(eye + ray * depth);
        if (hit.x < 0.002 + 0.00045 * depth) { material = hit.y; break; }
        depth += max(0.012, 0.80 * hit.x);
    }
    vec3 fog = vec3(0.015, 0.065, 0.10) + TIDE * (0.021 + 0.012 * bass);
    vec3 colour = fog;
    if (material > 0.0) {
        colour = surface(eye + ray * depth, ray, material);
    } else {
        depth = limit;
        if (floorDepth < 34.0) colour = water(eye + ray * depth, ray);
    }
    colour = mix(fog, colour, exp(-depth * 0.036));
    colour += chamberHaze(eye, ray, depth);

    // Three suspended dust layers give highlights a different parallax from the room.
    for (int layer = 0; layer < 3; ++layer) {
        float z = 3.6 - 3.7 * float(layer);
        float t = (z - eye.z) / ray.z;
        if (t < 0.0 || t > depth) continue;
        vec2 point = (eye + ray * t).xy * (2.3 + 0.7 * float(layer));
        point += motion * vec2(0.105, -0.18);
        vec2 cell = floor(point);
        float seed = fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
        vec2 offset = vec2(seed, fract(seed * 29.1)) * 0.74 + 0.13;
        float dust = exp(-length(fract(point) - offset) * (35.0 + 12.0 * float(layer)));
        colour += mix(GOLD, TIDE, seed) * dust * (0.23 + 0.27 * syn_HighLevel) * seed;
    }

    colour *= 1.0 - 0.30 * smoothstep(0.29, 0.92, length(screen));
    colour = 1.0 - exp(-max(colour, 0.0) * 1.62);
    fragColor = vec4(pow(colour, vec3(0.89)), 1.0);
}

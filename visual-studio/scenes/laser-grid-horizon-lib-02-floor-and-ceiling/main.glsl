#version 330 core
uniform vec2  RENDERSIZE;
uniform float TIME;
uniform float motion;
uniform float gridScale;
uniform float laserGain;
uniform float response;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_OnBeat;
uniform float syn_Hits;
uniform float syn_BPM;
uniform float syn_BPMSin;
uniform sampler2D syn_Media;
uniform float syn_MediaType;
out vec4 fragColor;

// Saturated cosine palette, consistent across the family.
vec3 palette(float t) {
    return 0.5 + 0.5 * cos(6.2831853 * (t + vec3(0.0, 0.33, 0.67)));
}

// Soft vignette, 16:9 centred framing.
float vignette(vec2 p) {
    return 1.0 - smoothstep(0.55, 1.15, length(p * vec2(0.82, 1.15)));
}

// Project a screen point below the horizon onto the floor plane.
// Returns (floorU, floorV, perspectiveT); t is 1.0 at the horizon and
// grows toward the camera. Bounded arithmetic, no loops.
vec3 floorProject(vec2 p, float horizonY, float scaleX) {
    float d = max(horizonY - p.y, 1e-4);
    float t = horizonY / d;          // 1 at horizon, large near bottom
    float u = p.x * t * scaleX;
    float v = t;
    return vec3(u, v, t);
}

// Bright grid lines from a repeating cell coordinate.
float gridLine(vec2 gp, float width) {
    vec2 c = abs(fract(gp * gridScale) - 0.5);   // distance from cell centre per axis
    float m = min(c.x, c.y);
    return 1.0 - smoothstep(0.0, width, m);
}

// Sample the nested artwork tiled on the floor. When no artwork is
// supplied, synthesise a readable procedural tile so the scene still
// composes. Alpha is preserved and used as the blend mask.
vec4 floorTile(vec2 gp) {
    vec2 tuv = fract(gp * 0.25);
    if (syn_MediaType > 0.0) {
        return texture(syn_Media, clamp(tuv, 0.0, 1.0));
    }
    vec2 c = abs(fract(gp * 0.5) - 0.5);
    float blob = 1.0 - smoothstep(0.05, 0.42, length(c));
    vec3 pc = palette(gp.x * 0.07 + gp.y * 0.05 + TIME * 0.03);
    float ring = 0.5 + 0.5 * cos(6.2831853 * (gp.y * 0.125 - TIME * 0.15));
    return vec4(pc * (0.35 + 0.65 * ring) * blob, blob);
}

// Beat lasers radiating from the vanishing point on the horizon.
// Bounded fixed loop (3 beams). Returns additive RGB.
vec3 lasers(vec2 p, vec2 vanish, float strength) {
    vec2 dir = p - vanish;
    float ang = atan(dir.y, dir.x);
    float dist = length(dir);
    vec3 acc = vec3(0.0);
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float w = pow(0.5 + 0.5 * cos(ang * 18.0 + fi * 2.094 + TIME * 1.5), 24.0);
        // Fade with distance from the vanishing point; only below horizon.
        float spread = (1.0 - smoothstep(0.0, 1.4, dist)) * smoothstep(0.0, 0.02, vanish.y - p.y);
        acc += palette(ang * 0.6 + fi * 0.33 + (syn_MidLevel * response) * 0.4) * w * spread;
    }
    // Chromatic separation from the highs.
    float split = (syn_HighLevel * response) * 0.02;
    vec3 hi = vec3(
        pow(0.5 + 0.5 * cos((ang + split) * 18.0), 24.0),
        pow(0.5 + 0.5 * cos(ang * 18.0), 24.0),
        pow(0.5 + 0.5 * cos((ang - split) * 18.0), 24.0));
    acc += hi * strength * 0.6 * smoothstep(0.0, 0.02, vanish.y - p.y);
    return acc * strength * laserGain;
}

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 p  = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / RENDERSIZE.y;

        // --- Floor and ceiling, mirrored perspective surfaces ---
        float horizon = 0.0;
        vec2 vanish = vec2(sin(TIME * 0.2) * 0.2, horizon);
        vec3 col = vec3(0.0);
        float zoff = motion * 3.0 + TIME * 0.2 * (0.5 + (syn_BassLevel * response));
        // Floor (lower half) and ceiling (upper half, mirrored).
        bool isFloor = p.y < horizon;
        vec2 q = p;
        if (!isFloor) { q.y = -q.y; }
        if (q.y < 0.02) {
            vec3 pr = floorProject(q, max(horizon, 0.02), 3.0);
            vec2 gp = vec2(pr.x, pr.y - zoff * (isFloor ? 1.0 : -1.0));
            vec4 tile = floorTile(gp);
            float line = gridLine(gp, 0.05 + (syn_HighLevel * response) * 0.03);
            float fog = exp(-pr.z * 0.10);
            vec3 hue = palette(gp.y * 0.06 + (isFloor ? 0.0 : 0.42) + (syn_MidLevel * response) * 0.3);
            vec3 base = tile.rgb * tile.a * (0.5 + 0.5 * (syn_BassLevel * response));
            vec3 glow = hue * line * (0.9 + 2.5 * (syn_BassLevel * response));
            col = (base + glow) * fog;
            col += lasers(p, vanish, (syn_OnBeat * response) * (1.0 + (syn_BassLevel * response))) * fog;
        }

    // Deep background gradient above the horizon.
    vec3 sky = mix(vec3(0.01, 0.02, 0.05), palette(TIME * 0.02 + (syn_MidLevel * response) * 0.25) * 0.28,
                   1.0 - smoothstep(-0.1, 0.55, p.y));
    sky += palette(p.x * 0.5 + TIME * 0.03) * (syn_HighLevel * response) * 0.12;

        col += sky * 0.15;
        // Central seam: bright corridor line where floor meets ceiling.
        float seam = 1.0 - smoothstep(0.0, 0.02 + 0.02 * (syn_OnBeat * response), abs(p.y - horizon));
        col += palette(0.15 + (syn_MidLevel * response) * 0.3) * seam * (0.6 + 2.2 * (syn_OnBeat * response));

    // Light vignette and beat accent.
    col *= vignette(p);
    col *= 0.82 + 0.35 * (syn_OnBeat * response) + 0.10 * (syn_Hits * response);
    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}

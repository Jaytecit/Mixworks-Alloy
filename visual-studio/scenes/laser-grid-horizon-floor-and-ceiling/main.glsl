// Family 06: Laser Grid Horizon — Floor and Ceiling
#version 330 core

uniform vec2 RENDERSIZE;
uniform float TIME;
uniform float syn_Level;
uniform float syn_BassLevel;
uniform float syn_MidLevel;
uniform float syn_HighLevel;
uniform float syn_Hits;
uniform float syn_BassHits;
uniform float syn_OnBeat;
uniform float syn_BPMSin;
uniform sampler2D syn_Media;
uniform float syn_MediaType;
uniform float motion;
uniform float gridScale;
uniform float laserGain;

out vec4 fragColor;

mat2 rot2(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

vec3 cosinePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return clamp(a + b * cos(6.2831853 * (c * t + d)), 0.0, 1.0);
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float starLayer(vec2 uv, float threshold, float twinkleSpeed) {
    vec2 id = floor(uv);
    vec2 gv = fract(uv) - 0.5;
    float rnd = hash21(id);
    if (rnd < threshold) {
        return 0.0;
    }
    vec2 offset = vec2(hash21(id + 13.7), hash21(id + 71.3)) - 0.5;
    float d = length(gv - offset * 0.55);
    float radius = 0.04 + 0.06 * fract(rnd * 37.0);
    float twinkle = 0.65 + 0.35 * sin(TIME * twinkleSpeed + rnd * 62.83);
    return (1.0 - smoothstep(0.0, radius, d)) * twinkle;
}

float laserRay(vec2 p, vec2 origin, vec2 dir) {
    vec2 pa = p - origin;
    float proj = max(dot(pa, dir), 0.0);
    return length(pa - dir * proj);
}

float laserSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float denom = max(dot(ba, ba), 1e-6);
    float h = clamp(dot(pa, ba) / denom, 0.0, 1.0);
    return length(pa - ba * h);
}

vec3 evalLaser(float d, vec3 col, float intensity) {
    float core = exp(-d * 185.0);
    float halo = exp(-d * 22.0);
    return (col * halo * 0.65 + mix(col, vec3(1.0), 0.72) * core * 1.45) * intensity;
}

vec3 sampleGridSurface(
    vec2 tileUV,
    float dist,
    float surfFade,
    vec3 gridCol,
    vec3 accentCol,
    vec2 dx,
    vec2 dy
) {
    vec2 cellID = floor(tileUV);
    vec2 cellUV = fract(tileUV);

    vec2 fw = max(abs(dx) + abs(dy), vec2(0.0008));
    vec2 borderDist = abs(fract(tileUV - 0.5) - 0.5) / fw;
    float gridLine = 1.0 - clamp(min(borderDist.x, borderDist.y) * 0.62, 0.0, 1.0);

    vec2 cellEdge = min(cellUV, 1.0 - cellUV);
    float halo = exp(-min(cellEdge.x, cellEdge.y) * 18.0) * (1.0 - smoothstep(4.0, 48.0, dist));

    float zPulse = exp(-abs(fract(tileUV.y * 0.25 - 0.5 * syn_OnBeat) - 0.5) * 14.0) * (0.20 + 1.10 * syn_OnBeat);
    float intersectGlow = exp(-length(fract(tileUV - 0.5) - 0.5) * 8.5) * (0.18 + 0.90 * syn_OnBeat);

    float checker = mod(cellID.x + cellID.y, 2.0);
    vec2 pLocal = cellUV - 0.5;
    float boxFrame = (1.0 - smoothstep(0.41, 0.43, max(abs(pLocal.x), abs(pLocal.y)))) *
                     smoothstep(0.35, 0.37, max(abs(pLocal.x), abs(pLocal.y)));
    float diamond = (1.0 - smoothstep(0.21, 0.24, abs(pLocal.x) + abs(pLocal.y)));
    float innerGlyph = mix(boxFrame, boxFrame + 0.55 * diamond, checker);

    vec3 baseTile = vec3(0.015, 0.020, 0.045) + 0.032 * accentCol * (0.30 + 0.70 * checker);
    baseTile += accentCol * innerGlyph * (0.22 + 0.28 * syn_Level + 0.32 * syn_OnBeat) * (1.0 - smoothstep(5.0, 55.0, dist));

    if (syn_MediaType > 0.0) {
        vec2 pad = vec2(0.075);
        vec2 mediaUV = (cellUV - pad) / (1.0 - 2.0 * pad);
        vec2 edgeAA = max(fw * 1.25, vec2(0.002));
        float boxMask = smoothstep(0.0, edgeAA.x, mediaUV.x) * (1.0 - smoothstep(1.0 - edgeAA.x, 1.0, mediaUV.x)) *
                        smoothstep(0.0, edgeAA.y, mediaUV.y) * (1.0 - smoothstep(1.0 - edgeAA.y, 1.0, mediaUV.y));
        vec2 clampedUV = clamp(mediaUV, 0.0, 1.0);
        vec2 ca = vec2(0.014 * syn_HighLevel, 0.0);
        vec4 img = textureGrad(syn_Media, clampedUV, dx, dy);
        float rCh = textureGrad(syn_Media, clamp(mediaUV + ca, 0.0, 1.0), dx, dy).r;
        float bCh = textureGrad(syn_Media, clamp(mediaUV - ca, 0.0, 1.0), dx, dy).b;
        vec3 imgCol = mix(img.rgb, vec3(rCh, img.g, bCh), 0.75 * clamp(syn_HighLevel * 1.4, 0.0, 1.0));
        imgCol *= 0.88 + 0.26 * syn_OnBeat + 0.10 * syn_BPMSin;
        float alpha = clamp(img.a * boxMask, 0.0, 1.0);
        baseTile = mix(baseTile, imgCol, alpha);
    }

    vec3 lineGlow = gridCol * (
        gridLine * (0.95 + 0.75 * syn_OnBeat + 0.40 * syn_HighLevel) +
        halo * (0.45 + 0.55 * syn_OnBeat)
    );
    lineGlow += mix(gridCol, vec3(1.0), 0.55) * (intersectGlow + zPulse * gridLine) * 0.80;

    return (baseTile + lineGlow) * surfFade;
}

vec3 renderSynthSky(vec2 pSky, vec3 horizonCol, vec3 zenithCol, vec3 sunTopCol, vec3 sunBotCol) {
    float grad = clamp(pSky.y * 1.35 + 0.08, 0.0, 1.0);
    vec3 sky = mix(horizonCol, zenithCol, pow(grad, 0.65));

    float stars = starLayer(pSky * 18.0 + vec2(0.0, TIME * 0.02), 0.86, 2.2) +
                  starLayer(pSky * 34.0 + vec2(4.2, TIME * 0.04), 0.91, 3.4);
    sky += vec3(0.75, 0.88, 1.0) * stars * smoothstep(0.05, 0.38, pSky.y);

    vec2 sunCenter = vec2(0.0, 0.20 + 0.02 * sin(TIME * 0.6));
    float sunRad = 0.24 + 0.025 * syn_BassHits + 0.015 * syn_BassLevel;
    float dSun = length(pSky - sunCenter);
    float sunGlow = exp(-max(dSun - sunRad, 0.0) * 6.5) * (0.55 + 0.35 * syn_OnBeat);
    sky += mix(sunBotCol, sunTopCol, 0.4) * sunGlow;

    if (dSun < sunRad + 0.02) {
        float sunMask = (1.0 - smoothstep(sunRad - 0.004, sunRad + 0.004, dSun));
        float sunT = clamp((pSky.y - (sunCenter.y - sunRad)) / (2.0 * sunRad), 0.0, 1.0);
        float cutStripe = smoothstep(0.05, 0.22, fract((pSky.y - sunCenter.y) * 18.0 - 0.12 * TIME));
        float lowerHalf = (1.0 - smoothstep(0.30, 0.58, sunT));
        sunMask *= mix(1.0, cutStripe, lowerHalf);
        vec3 sunCol = mix(sunBotCol, sunTopCol, sunT);
        sky = mix(sky, sunCol * (1.15 + 0.25 * syn_BPMSin), sunMask);
    }

    return sky;
}

vec3 renderVariant(
    vec2 p,
    float travel,
    float scale,
    float laserPower,
    vec3 gridCol,
    vec3 accentCol
) {
    float roll = 0.075 * sin(TIME * 0.50) + 0.11 * syn_MidLevel * cos(TIME * 0.35);
    vec2 pr = rot2(roll) * p;

    float gap = 1.32 - 0.18 * syn_BassLevel;
    float focal = 1.15 + 0.18 * syn_BassLevel;
    vec3 rd = normalize(vec3(pr, focal));

    float safeVert = max(abs(rd.y), 0.0025);
    float tPlane = min(gap / safeVert, 95.0);
    vec2 hitXZ = vec2(rd.x * tPlane, rd.z * tPlane);
    vec2 tileUV = vec2(hitXZ.x * scale + 0.5, hitXZ.y * scale + travel);
    vec2 dx = dFdx(tileUV);
    vec2 dy = dFdy(tileUV);

    float isFloor = step(rd.y, 0.0);
    vec3 surfGrid = mix(accentCol, gridCol, isFloor);
    vec3 surfAccent = mix(gridCol, accentCol, isFloor);

    float surfFade = exp(-0.044 * tPlane) * smoothstep(0.003, 0.055, abs(rd.y));
    vec3 planeCol = sampleGridSurface(tileUV, tPlane, surfFade, surfGrid, surfAccent, dx, dy);

    // Central vanishing corridor glow & stylized spectrum horizon core
    float horDist = abs(pr.y);
    float horGlow = exp(-horDist * 22.0);
    float xFreq = abs(pr.x) * 2.2;
    float eqEnv = mix(syn_BassLevel, mix(syn_MidLevel, syn_HighLevel, clamp(xFreq - 0.4, 0.0, 1.0)), clamp(xFreq, 0.0, 1.0));
    float eqWave = 0.012 + 0.042 * eqEnv * (0.55 + 0.45 * cos(pr.x * 46.0 - TIME * 4.0));
    float eqMask = (1.0 - smoothstep(eqWave - 0.002, eqWave + 0.006, horDist));

    vec3 corridorBg = mix(vec3(0.02, 0.015, 0.06), mix(gridCol, accentCol, 0.5), horGlow * 0.85);
    corridorBg += mix(accentCol, vec3(1.0), 0.4) * eqMask * (0.75 + 0.55 * syn_OnBeat);

    vec3 col = mix(corridorBg, planeCol, smoothstep(0.003, 0.032, abs(rd.y))) + corridorBg * horGlow * 0.55;

    // Criss-crossing corridor laser beams spanning floor and ceiling on the beat
    float sweep = 0.22 * sin(TIME * 2.2 + syn_MidLevel * 2.0);
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float side = mod(fi, 2.0) * 2.0 - 1.0;
        float idx = floor(fi * 0.5);
        float xA = side * (0.18 + 0.26 * idx + sweep);
        float xB = -side * (0.34 + 0.22 * idx - sweep * 0.7);
        vec2 ptA = vec2(xA, -0.46);
        vec2 ptB = vec2(xB, 0.46);
        float dSeg = laserSegment(pr, ptA, ptB);
        vec3 beamCol = mix(gridCol, accentCol, idx * 0.45);
        col += evalLaser(dSeg, beamCol, laserPower * 0.58);
    }

    // Horizon split laser beam
    col += evalLaser(abs(pr.y), mix(gridCol, vec3(1.0), 0.4), laserPower * 0.68);

    return col;
}

void main() {
    vec2 res = max(RENDERSIZE, vec2(1.0));
    vec2 p = (gl_FragCoord.xy - 0.5 * res) / res.y;

    float palShift = 0.15 * syn_MidLevel + 0.018 * TIME;
    vec3 gridCol = cosinePalette(
        0.04 + palShift,
        vec3(0.45, 0.40, 0.55),
        vec3(0.50, 0.50, 0.45),
        vec3(1.0, 1.0, 1.0),
        vec3(0.55, 0.05, 0.85)
    );
    vec3 accentCol = cosinePalette(
        0.38 + palShift,
        vec3(0.48, 0.40, 0.52),
        vec3(0.50, 0.48, 0.46),
        vec3(1.0, 1.0, 1.0),
        vec3(0.55, 0.05, 0.85)
    );

    float travel = motion * 3.2;
    float scale = 0.54 * max(gridScale, 0.25);
    float laserPower = max(laserGain, 0.0) * (0.16 + 0.95 * syn_OnBeat + 0.40 * max(syn_Hits, syn_BassHits));

    vec3 col = renderVariant(p, travel, scale, laserPower, gridCol, accentCol);

    // Subtle high-frequency edge shimmer and 16:9 vignette
    float vig = clamp(1.0 - 0.26 * dot(p, p), 0.25, 1.0);
    col *= vig;
    col = col / (vec3(1.0) + 0.22 * col);
    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}

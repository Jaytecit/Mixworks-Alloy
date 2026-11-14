// Family 06: Laser Grid Horizon — Curved Planet Horizon
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
    float roll = 0.055 * sin(TIME * 0.40) * (0.4 + syn_MidLevel);
    vec2 pr = rot2(roll) * p;

    float pitch = -0.25;
    vec3 rd = normalize(vec3(pr.x, pr.y * cos(pitch) + 1.18 * sin(pitch), -pr.y * sin(pitch) + 1.18 * cos(pitch)));

    float R = 11.5;
    float alt = 1.12 + 0.24 * syn_BassLevel;
    vec3 center = vec3(0.0, -(R + alt), 0.0);
    vec3 oc = -center; // ro = (0,0,0)

    float b = dot(oc, rd);
    float c = dot(oc, oc) - R * R;
    float disc = b * b - c;

    // Space sky & ringed cyber-moon behind curved horizon
    vec3 spaceBg = mix(vec3(0.012, 0.015, 0.055), accentCol * 0.28, exp(-max(pr.y + 0.08, 0.0) * 5.0));
    float stars = starLayer(pr * 19.0 + vec2(0.0, TIME * 0.015), 0.85, 2.1) +
                  starLayer(pr * 36.0 + vec2(3.1, TIME * 0.03), 0.90, 3.2);
    spaceBg += vec3(0.80, 0.90, 1.0) * stars;

    vec2 moonPos = vec2(0.32 * sin(TIME * 0.12 + syn_MidLevel * 0.4), 0.22);
    float moonRad = 0.14 + 0.018 * syn_BassHits;
    float dMoon = length(pr - moonPos);
    if (dMoon < moonRad + 0.01) {
        float mMask = (1.0 - smoothstep(moonRad - 0.004, moonRad + 0.004, dMoon));
        float mBand = 0.6 + 0.4 * sin((pr.y - moonPos.y) * 65.0);
        vec3 mCol = mix(accentCol, vec3(1.0, 0.82, 0.38), clamp((pr.y - moonPos.y) / moonRad * 0.5 + 0.5, 0.0, 1.0));
        spaceBg = mix(spaceBg, mCol * (0.75 + 0.45 * mBand), mMask);
    }
    // Tilted planetary ring around the moon
    vec2 ringP = rot2(-0.32) * (pr - moonPos);
    ringP.y *= 3.8;
    float ringD = abs(length(ringP) - moonRad * 1.75);
    float ringMask = (1.0 - smoothstep(0.004, 0.035, ringD)) * step(0.0, pr.y - moonPos.y + 0.06);
    spaceBg += mix(gridCol, vec3(1.0), 0.35) * ringMask * (0.45 + 0.35 * syn_OnBeat);

    // Closest approach of ray to sphere for atmospheric limb halo
    float rayProj = max(-b, 0.0);
    float closestDist = length(oc + rd * rayProj);
    float atmoDist = max(closestDist - R, 0.0);
    spaceBg += mix(gridCol, accentCol, 0.45) * exp(-atmoDist * 3.8) * (0.75 + 0.55 * syn_OnBeat);

    // Compute curved spherical surface coordinates unconditionally for smooth derivatives
    float safeDisc = max(disc, 0.0);
    float tHit = clamp(-b - sqrt(safeDisc), 0.5, 42.0);
    vec3 hitPos = rd * tHit;
    vec3 q = hitPos - center;
    vec3 surfN = normalize(q);

    float lon = atan(q.x, max(q.y, 0.001));
    float lat = atan(q.z, max(length(q.xy), 0.001));
    vec2 tileUV = vec2(lon * R * 0.42 * scale + 0.5, lat * R * 0.42 * scale + travel);
    vec2 dx = dFdx(tileUV);
    vec2 dy = dFdy(tileUV);

    float fresnel = pow(1.0 - clamp(dot(surfN, -rd), 0.0, 1.0), 2.8);
    float surfFade = exp(-0.048 * tHit) * smoothstep(0.0, 0.18, disc);
    vec3 planetCol = sampleGridSurface(tileUV, tHit, surfFade, gridCol, accentCol, dx, dy);
    planetCol += mix(gridCol, vec3(0.9, 0.95, 1.0), 0.35) * fresnel * (0.70 + 0.60 * syn_OnBeat);

    float hitMask = smoothstep(0.0, 0.14, disc) * step(0.0, -b);
    vec3 col = mix(spaceBg, planetCol, hitMask);

    // Orbital laser beams erupting upward from behind the curved planetary limb
    float limbOffset = -0.04;
    for (int i = 0; i < 6; ++i) {
        float fi = float(i);
        float fracPos = (fi - 2.5) / 2.5;
        float xOrigin = fracPos * 0.58;
        float yOrigin = limbOffset - 0.16 * xOrigin * xOrigin;
        vec2 origin = vec2(xOrigin, yOrigin);
        float ang = 1.5707963 - fracPos * (0.42 + 0.15 * sin(TIME * 1.7 + syn_MidLevel * 2.0));
        vec2 dir = vec2(cos(ang), sin(ang));
        float dBeam = laserRay(pr, origin, dir);
        // Attenuate laser slightly below the curved foreground limb
        float aboveLimb = mix(0.25, 1.0, 1.0 - hitMask * 0.75);
        vec3 beamCol = mix(gridCol, accentCol, abs(fracPos));
        col += evalLaser(dBeam, beamCol, laserPower * 0.65 * aboveLimb);
    }

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

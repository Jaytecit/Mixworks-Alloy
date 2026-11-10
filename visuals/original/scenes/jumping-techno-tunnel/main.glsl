#version 330
uniform sampler2D userImage;
// Original cyan/pink tunnel. Jumping time-times-bass motion deliberately retained.
uniform vec2 resolution;
uniform float sceneTime, bass;
out vec4 colour;
// User-supplied Shadertoy-style scene core.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord - 0.5 * resolution) / resolution.y;
    float low = bass;
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    // Use the artwork as a relief chart wrapped around the tunnel.
    vec2 wall=vec2(a/6.2831853+.5,fract(.15/max(r,.04)));
    r+=.060*structureHeight(wall);

    float depth = 1.0 / max(r, 0.001) + sceneTime * (0.6 + 1.5 * low);
    float rings = step(0.88, fract(depth * 1.5));
    float spokes = step(0.90, fract(a * 8.0 / 6.2831853 + sceneTime * 0.05));
    float line = max(rings, spokes);
    vec3 col = mix(vec3(0.0, 0.02, 0.05), vec3(0.1, 0.9, 1.0), line);
    col *= smoothstep(0.0, 0.4, r);
    col += low * low * vec3(1.0, 0.15, 0.4) * (1.0 - r);
    fragColor = vec4(col, 1.0);
}

void main(){ mainImage(colour, gl_FragCoord.xy); }

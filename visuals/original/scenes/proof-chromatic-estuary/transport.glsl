#version 330
// Material coordinates advect through the current velocity, retaining their
// trajectories. Independent from image-driven force and optical refraction.
uniform vec2 resolution;
uniform sampler2D input0, input1;
uniform float simulationReset, stepDelta;
out vec4 fragColor;
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    if(simulationReset>.5) {fragColor=vec4(uv,0.,1.);return;}
    vec2 velocity=texture(input0,uv).xy;
    vec2 source=texture(input1,uv-velocity*stepDelta/resolution).xy;
    // Slow replenishment prevents irreversible collapse of the material chart.
    source=mix(source,uv,1.-exp(-stepDelta*.055));
    fragColor=vec4(source,0.,1.);
}

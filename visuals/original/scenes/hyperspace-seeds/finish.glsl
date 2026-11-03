#version 330
uniform sampler2D input0, input1;
uniform vec2 resolution;
out vec4 colour;
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 light=texture(input0,uv).rgb+texture(input1,uv).rgb*.22;
    light*=1.35;
    vec3 mapped=light/(1.+light);
    colour=vec4(pow(mapped,vec3(1./2.2)),1);
}

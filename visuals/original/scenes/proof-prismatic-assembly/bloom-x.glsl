#version 330
// Original separable highlight filter, horizontal extraction.
uniform sampler2D input0;
uniform vec2 resolution;
out vec4 colour;
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 sum=vec3(0); float weight=0.;
    for(int i=-4;i<=4;i++) {
        float w=exp(-float(i*i)/8.);
        vec3 c=texture(input0,uv+vec2(float(i)*2./resolution.x,0)).rgb;
        sum+=max(c-vec3(.65),vec3(0))*w; weight+=w;
    }
    colour=vec4(sum/weight,1);
}

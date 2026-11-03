#version 330
uniform sampler2D input0;
uniform vec2 resolution;
out vec4 colour;
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 sum=vec3(0); float weight=0.;
    for(int i=-4;i<=4;i++) {
        float w=exp(-float(i*i)/8.);
        sum+=texture(input0,uv+vec2(0,float(i)*2./resolution.y)).rgb*w; weight+=w;
    }
    colour=vec4(sum/weight,1);
}

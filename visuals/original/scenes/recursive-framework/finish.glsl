#version 330
// Linear-light bloom followed by a hue-preserving highlight shoulder and sRGB.
// The shoulder is identity below .72: photographs are not gamma-lifted twice.
uniform sampler2D input0,input1;
uniform vec2 resolution;
out vec4 colour;
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 c=max(texture(input0,uv).rgb+texture(input1,uv).rgb*.13,vec3(0));
    float peak=max(c.r,max(c.g,c.b));
    float mapped=peak<=.72?peak:.72+.28*(1.-exp(-(peak-.72)/.28));
    c*=mapped/max(peak,.00001);
    c=mix(12.92*c,1.055*pow(c,vec3(1./2.4))-.055,step(vec3(.0031308),c));
    colour=vec4(clamp(c,0.,1.),1);
}

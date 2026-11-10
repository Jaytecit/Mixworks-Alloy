#version 330
// Pre-filter the source once, rather than resampling artwork inside every ray step.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
out vec4 colour;
vec3 field(vec2 uv){
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 cut=reliefCutout(materialDomain(uv));
    vec3 weights=vec3(.2126,.7152,.0722);
    float sprite=dot(cut.rgb,weights);
    float pigment=dot(background.rgb,weights)*background.a*(1.-cut.a)+sprite;
    float coverage=cut.a+background.a*(1.-cut.a);
    return vec3(pigment,coverage,sprite);
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 c=(field(uv)*2.+field(uv+vec2(.012,0))+field(uv-vec2(.012,0))
        +field(uv+vec2(0,.012))+field(uv-vec2(0,.012)))/6.;
    c.r=max(c.r,structureHeight(uv));
    c.b=max(c.b,structureHeight(uv));
    colour=vec4(c,1.);
}

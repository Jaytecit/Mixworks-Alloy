#version 330
// Pre-filter the source once, rather than resampling artwork inside every ray step.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
out vec4 colour;
vec3 art(vec2 uv){
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0),cut=sampleCutout(materialDomain(uv));
    return mix(background.rgb,cut.rgb,cut.a);
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 c=(art(uv)*2.+art(uv+vec2(.012,0))+art(uv-vec2(.012,0))
        +art(uv+vec2(0,.012))+art(uv-vec2(0,.012)))/6.;
    colour=vec4(max(dot(c,vec3(.2126,.7152,.0722)),structureHeight(uv)),0,0,1);
}

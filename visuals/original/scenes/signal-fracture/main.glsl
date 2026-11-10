#version 330
// Original block/raster signal instrument; source picture remains the subject.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bassHit,midHit,trebleHit,warp,detail,imageMix,pulse;
out vec4 colour;
vec3 imageAt(vec2 uv){
    uv=.5+(uv-.5)/vec2(1.+.7*bassHit*pulse,1.+2.5*bassHit*pulse);uv.y+=sin(floor(sceneTime*2.)*2.4)*.22*bassHit*pulse;
    if(hasImage!=0||hasCutout!=0){
        vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
        vec4 cut=sampleCutout(materialDomain(uv));return mix(background.rgb,cut.rgb,cut.a);
    }
    return vec3(step(.5,fract(uv.x*8.)),step(.5,fract(uv.y*6.)),.65);
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 cell=floor(uv*vec2(12,7));vec4 region=texelFetch(input0,ivec2(clamp(cell,vec2(0),vec2(11,6))),0);
    float line=floor(uv.y*(180.+detail*360.));
    float tear=sin(line*.71+floor(sceneTime*8.)*.9)*trebleHit*pulse*.018;
    vec2 q=uv+vec2(region.x+tear,region.z*.035*sin(sceneTime*1.2+cell.x));
    vec3 c=imageAt(q);
    float shift=region.y*(.4+warp)+midHit*pulse*.008;
    c.r=imageAt(q+vec2(shift,0)).r;c.b=imageAt(q-vec2(shift,0)).b;
    float quantise=5.+floor((1.-region.z)*18.);
    c=mix(c,floor(c*quantise)/quantise,region.z*.8);
    vec2 f=fract(uv*vec2(12,7));
    float seam=(1.-smoothstep(.006,.013,min(f.x,f.y)))*abs(region.x)*3.;
    c+=seam*vec3(.02,.7,1.);
    c*=.92+.08*cos(uv.y*resolution.y*3.14159);
    colour=vec4(clamp(c,0.,1.),1.);
}

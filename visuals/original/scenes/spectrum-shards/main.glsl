#version 330
#define COMPOSITION 5
// Original block/raster signal instrument; source picture remains the subject.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bassHit,midHit,trebleHit,warp,detail,imageMix,pulse;
out vec4 colour;
vec3 imageAt(vec2 uv){
    uv=.5+(uv-.5)/(1.+2.1*bassHit*pulse);uv+=vec2(.25*sin(travelTime*.8),.24*cos(travelTime*1.1))*pulse;
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
    vec3 past=texture(input1,clamp(q,0.,1.)).rgb;
#if COMPOSITION == 1
    q.y=fract(uv.y*1.4+region.x*.8+sceneTime*.025);q.x+=sin(floor(uv.y*35.)+sceneTime)*.04;
#elif COMPOSITION == 2
    q.x+=mod(floor(uv.x*60.),2.)<1.?region.x:-region.x;
#elif COMPOSITION == 3
    q=uv+vec2(1,-1)*region.x*step(.5,fract((uv.x+uv.y)*7.));
#elif COMPOSITION == 4
    vec2 blocks=uv*vec2(8,5),local=fract(blocks)-.5;
    q=(floor(blocks)+local*vec2(1.,mod(cell.x+cell.y,2.)<1.?-1.:1.)+.5)/vec2(8,5);
#elif COMPOSITION == 5
    vec2 p=(uv-.5)*vec2(1.78,1.);float a=atan(p.y,p.x),r=length(p);
    q=.5+vec2(cos(a+floor(a*4.)*.12),sin(a+floor(a*4.)*.12))*r*.7;
#elif COMPOSITION == 6
    q.y=fract(q.y+sceneTime*.025);q.x+=sin(floor(uv.y*90.)*.5)*region.x;
#elif COMPOSITION == 7
    vec2 grid=uv*vec2(12,7),patch=fract(grid)-.5;q=(floor(grid)+patch+vec2(sin(sceneTime+cell.x),cos(sceneTime*.8+cell.y))*region.z*.3+.5)/vec2(12,7);
#endif
    vec3 c=imageAt(q);
#if COMPOSITION == 2
    c=mix(c,past,mod(floor(uv.x*60.),2.)*.8);
#elif COMPOSITION == 3
    c=mix(c,past,step(.5,fract((uv.x+uv.y)*7.))*.7);
#elif COMPOSITION == 4
    c=mix(c,past,.65+region.z*.3);
#elif COMPOSITION == 6
    c=mix(c,past,step(.5,fract(uv.y*12.))*.65);
#endif
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

#version 330
#define COMPOSITION 4
// Original triangulated image-height terrain, lattice edge material and camera.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform int hasImage;
uniform float scale,warp,detail,imageMix,pulse,bass,bassHit,mid,midImpulse,trebleHit;
out vec4 colour;
vec3 sourceArt(vec2 uv){
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 cut=spriteMaterial(uv);
    return mix(background.rgb,cut.rgb,cut.a);
}
float vertexHeight(vec2 p){
    vec2 chart=fract(p*.052+vec2(.5,.25));
    float lum=(hasImage!=0||hasCutout!=0)?texture(input0,chart).r:.4+.3*sin(p.x*.7)*cos(p.y*.6);
    float ground=lum*(.7+imageMix*1.2)-.8;
#if COMPOSITION == 1
    ground+=sin(p.x*.5+p.y*.16+midImpulse*.17)*(.35+lum*.3);
#elif COMPOSITION == 2
    ground+=min(1.8,abs(p.x)*.55)-.4;
#elif COMPOSITION == 3
    ground=floor((lum+.2*sin(p.y*.3))*6.)*.19-.9;
#elif COMPOSITION == 4
    ground=floor(lum*4.)*.35+smoothstep(.5,.8,lum)*.2-.8;
#elif COMPOSITION == 5
    ground+=cos(length(p)*.8)*.3+sin(atan(p.y,p.x)*5.)*.16;
#elif COMPOSITION == 6
    ground+=abs(sin(p.x*.48+midImpulse*.1))*abs(cos(p.y*.6))*.9;
#else
    ground+=clamp(p.x*.32,-.5,1.7)+.2*sin(p.y*.4);
#endif
    return .55*spriteRelief(chart)+ground+bassHit*pulse*.25*exp(-abs(p.x)*.3);
}
float heightAt(vec2 p){
    float size=.46+.36*(1.-detail);vec2 grid=p/size,cell=floor(grid),f=fract(grid);
    float a=vertexHeight(cell*size),b=vertexHeight((cell+vec2(1,0))*size);
    float c=vertexHeight((cell+vec2(0,1))*size),d=vertexHeight((cell+1.)*size);
    return f.x+f.y<1.?a+(b-a)*f.x+(c-a)*f.y:d+(c-d)*(1.-f.x)+(b-d)*(1.-f.y);
}
void main(){
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float travel=travelTime*.72;
    vec3 eye=vec3(sin(travel*.07)*1.1,2.4+bass*.18,-4.+travel);
#if COMPOSITION == 5
    eye=vec3(sin(travel*.08)*5.,3.2,cos(travel*.08)*5.);
#elif COMPOSITION == 2
    eye.x=.2;eye.y=1.4;
#elif COMPOSITION == 7
    eye.x=-2.;eye.y=2.6;
#endif
    vec3 forward=normalize(vec3(.2*sin(travel*.06),-.24,1.));
#if COMPOSITION == 4
    eye.y=3.8;forward=normalize(vec3(.12,-.65,1.));
#elif COMPOSITION == 6
    eye.y=1.;forward=normalize(vec3(.08,.3,1.));
#elif COMPOSITION == 7
    forward=normalize(vec3(.65,-.16,.8));
#endif
#if COMPOSITION == 5
    forward=normalize(vec3(0,-.3,0)-eye);
#endif
    vec3 right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*1.45+right*uv.x+up*uv.y);
    vec3 sky=mix(vec3(.005,.008,.024),vec3(.025,.006,.075),clamp(uv.y*.5+.5,0.,1.));
    float dist=.2;vec3 p=eye;
    bool hit=false;
    for(int i=0;i<80;i++){
        p=eye+ray*dist;float gap=p.y-heightAt(p.xz);
#if COMPOSITION == 6
        gap=3.-p.y-heightAt(p.xz);
#endif
        if(gap<.012){hit=true;break;}
        dist+=max(.025,gap*.42);if(dist>34.)break;
    }
    vec3 c=sky;
    if(hit){
        float e=.022;
        vec3 n=normalize(vec3(heightAt(p.xz-vec2(e,0))-heightAt(p.xz+vec2(e,0)),2.*e,
            heightAt(p.xz-vec2(0,e))-heightAt(p.xz+vec2(0,e))));
        float size=.46+.36*(1.-detail);vec2 f=fract(p.xz/size);
        float edge=min(min(min(f.x,1.-f.x),min(f.y,1.-f.y)),abs(f.x+f.y-1.)*.707);
        float rods=1.-smoothstep(.018,.055+dist*.0008,edge);
#if COMPOSITION == 1
        rods*=.25+.75*step(abs(f.x-.5),.43);
#elif COMPOSITION == 3
        rods*=smoothstep(-.6,-.25,p.y);
#elif COMPOSITION == 6
        n=-n;
#endif
        vec3 art=(hasImage!=0||hasCutout!=0)?sourceArt(fract(p.xz*.052+vec2(.5,.25))):vec3(.2,.32,.7);
        float light=.23+.77*max(0.,dot(n,normalize(vec3(-.5,.9,-.4))));
        c=art*(.4+.6*imageMix)*light;
#if COMPOSITION == 3
        c=mix(vec3(.015,.12,.25),c,smoothstep(-.6,-.25,p.y));
#endif
        c+=rods*vec3(.02,.75,1.)*(.48+bassHit*.6);
        c=mix(c,sky,1.-exp(-dist*.052));
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

#version 330
#define COMPOSITION 0
// Owned continuous cellular population: Gaussian inner/outer neighbourhoods.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,motion,feedback,imageMix,pulse;
uniform float bassHit,midHit,trebleHit,bass,mid,treble,midImpulse;
out vec4 colour;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec4 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));return vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));}
float alive(vec2 uv){return texture(input0,fract(uv)).r;}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution,q=uv;
    float t=simulationTime,seed=hash(floor(uv*resolution));
#if COMPOSITION == 1
    q=fract(uv*vec2(3,2));
#elif COMPOSITION == 2
    vec2 p=(uv-.5)*vec2(1.78,1.);q=vec2(atan(p.y,p.x)/6.283+.5,length(p));
#elif COMPOSITION == 4
    q=.5+(uv-.5)*(.8+.35*cos(length(uv-.5)*9.));
#elif COMPOSITION == 6
    q=fract(uv*vec2(4,3)+vec2(mod(floor(uv.y*3.),2.)*.5,0));
#elif COMPOSITION == 7
    q=fract(uv*2.);
#endif
    vec4 image=art(q);float food=dot(image.rgb,vec3(.2126,.7152,.0722))*image.a;
#if COMPOSITION == 4
    float radius=length((uv-.5)*vec2(1.78,1.));
    food*=smoothstep(.2,.3,radius)*(1.-smoothstep(.55,.72,radius));
#endif
    if(hasImage==0&&hasCutout==0)food=.5+.3*cos(uv.x*19.)*sin(uv.y*13.);
    if(simulationReset>.5){colour=vec4(smoothstep(.08,.65,food)*(.65+.3*seed),0,food,1);return;}
    vec2 drift=vec2(0);
#if COMPOSITION == 3
    drift=vec2(sin(uv.y*8.+t*.25),.6)*px*.23;
#elif COMPOSITION == 5
    drift=vec2(cos(uv.y*12.+midImpulse),sin(uv.x*4.))*px*.14;
#elif COMPOSITION == 7
    drift=vec2(mod(floor(uv.y*6.),2.)*2.-1.,0)*px*.18*(.2+mid);
#endif
    vec2 chart=fract(uv-drift);float inner=0.,outer=0.,wi=0.,wo=0.;
    for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
        vec2 offset=vec2(x,y);
#if COMPOSITION == 5
        offset*=vec2(1.7,.65);
#elif COMPOSITION == 4
        offset*=1.5;
#elif COMPOSITION == 2
        offset*=vec2(.8,1.3);
#endif
        float r2=float(x*x+y*y),a=exp(-r2*1.1),b=exp(-r2*.20)-a*.7;
        float v=alive(chart+offset*px*1.6);inner+=v*a;wi+=a;outer+=v*b;wo+=b;
    }
    inner/=wi;outer/=wo;
    float birthLow=.27+float(COMPOSITION)*.005,birthHigh=.36+float(COMPOSITION%3)*.009;
    float surviveLow=.24+.025*bass,surviveHigh=.46-.02*mid;
    float low=mix(birthLow,surviveLow,smoothstep(.28,.66,inner));
    float high=mix(birthHigh,surviveHigh,smoothstep(.28,.66,inner));
    float growth=smoothstep(low-.025,low+.025,outer)*(1.-smoothstep(high-.025,high+.025,outer));
    float pop=alive(chart),rate=stepDelta*(1.1+motion*2.7);
    float next=mix(pop,growth,clamp(rate,0.,1.));
    next+=food*imageMix*stepDelta*(.02+.25*trebleHit*pulse);
    next-=bassHit*pulse*stepDelta*.16*(.2+.8*seed);
    colour=vec4(clamp(next,0.,1.),growth-pop,food,1.);
}

#version 330
#define PIGMENT_KIND 2
// Premultiplied retained pigment; three different source-entry and release rules.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta;
out vec4 colour;
// Authoring fragment, expanded into only Wake / Press / Fault.
uniform float bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
uniform float pulse,motion,warp,detail,feedback,imageMix;
vec4 pigmentArt(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 b=sampleCutout(materialDomain(uv));
    vec4 c=vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));
    if(hasImage==0&&hasCutout==0)c=vec4(.3+.3*sin(uv.x*12.),.35+.25*cos(uv.y*9.),.7,1.);
    return vec4(c.rgb*c.a,c.a);
}
vec2 jetOrigin(){return vec2(.12,.5+.12*sin(travelTime*.68+midImpulse*.14*pulse));}
float pressCentre(vec2 uv,float side){
    float squeeze=.04*sin(travelTime*.72)+bassHit*pulse*.035;
    return .5+side*(.25-squeeze)+.024*sin(uv.y*(6.+detail*6.)+midImpulse*.25*pulse);
}
float pressWidth(){return .075+.020*warp+.018*bassHit*pulse;}
float pressMask(vec2 uv,float side){
    float dx=(uv.x-pressCentre(uv,side))/pressWidth();
    float vertical=smoothstep(.04,.16,uv.y)*smoothstep(.04,.16,1.-uv.y);
    float square=dx*dx;
    return exp(-square*square)*vertical;
}
vec2 pressSource(vec2 uv,float side){
    float local=(uv.x-pressCentre(uv,side))/pressWidth();
    return vec2((side<0.?.25:.75)+local*.24,uv.y);
}
float faultBoundary(vec2 uv){
    float tooth=2.*abs(2.*fract(uv.x*(4.+floor(detail*6.)))-1.)-1.;
    return .5+.10*(uv.x-.5)+.024*tooth+.012*sin(travelTime*.4);
}
float faultGap(){return .018+.044*bassHit*pulse;}
vec2 faultSource(vec2 uv){
    float side=sign(uv.y-faultBoundary(uv));
    float slip=.075*sin(travelTime*.65)+.12*sin(midImpulse*.48)*pulse;
    return uv-vec2(side*slip,side*faultGap()*.65);
}

void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec4 fresh=vec4(0);
    float mask=1.,release=.25,feed=1.;
#if PIGMENT_KIND == 0
    vec2 origin=jetOrigin(),d=uv-origin;
    vec2 size=vec2(.035,.032+.020*bassHit*pulse);
    mask=exp(-dot(d/size,d/size));
    vec2 source=.5+d/vec2(.070,.110)+vec2(.18*sin(travelTime*.5),0);
    fresh=pigmentArt(source)*mask*(.5+.5*bassHit*pulse);
    release=.32+(1.-feedback)*.7;
    feed=32.;
#elif PIGMENT_KIND == 1
    float left=pressMask(uv,-1.),right=pressMask(uv,1.);
    fresh=pigmentArt(pressSource(uv,-1.))*left+pigmentArt(pressSource(uv,1.))*right;
    mask=max(left,right);release=.8+(1.-feedback)*.7;feed=15.;
#else
    float distance=abs(uv.y-faultBoundary(uv));
    mask=smoothstep(faultGap(),faultGap()+.008,distance);
    fresh=pigmentArt(uv)*mask;release=.15+(1.-feedback)*.35;
    feed=.7+trebleHit*pulse*.9;
#endif
    if(simulationReset>.5){colour=fresh;return;}
    vec2 flow=texture(input1,uv).rg;
    vec2 chart=uv-flow*stepDelta*(.6+motion);
    // Pigment exits the frame rather than accumulating on a clamped border.
    vec4 old=vec4(0);
    if(all(greaterThanEqual(chart,vec2(0)))&&all(lessThanEqual(chart,vec2(1))))old=texture(input0,chart);
    old*=exp(-stepDelta*release);
    float acquisition=(1.-exp(-stepDelta*feed))*mask*imageMix;
    vec4 state=mix(old,fresh,acquisition);
#if PIGMENT_KIND == 1
    // The compression corridor stays empty; converging retained pigment must
    // not accumulate into the old broad central smear.
    state*=smoothstep(.06,.095,abs(uv.x-.5));
#endif
#if PIGMENT_KIND == 2
    state*=mask;
#endif
    colour=vec4(clamp(state.rgb,0.,1.),clamp(state.a,0.,1.));
}

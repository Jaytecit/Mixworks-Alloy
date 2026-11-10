#version 330
#define PIGMENT_KIND 0
// Retained bounded velocity/curl state; local feed is in the pigment pass.
uniform vec2 resolution;
uniform sampler2D input0;
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
vec2 jetOrigin(){return vec2(.10,.5+.36*sin(travelTime*1.15+midImpulse*.30*pulse));}
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
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
    vec2 target=vec2(0);
#if PIGMENT_KIND == 0
    target=vec2(.27+.10*bassHit*pulse,
        .055*sin(uv.x*5.-travelTime*.8+midImpulse*.17*pulse)
        +detail*.012*sin(uv.x*55.-travelTime*4.+trebleImpulse*.3*pulse));
#elif PIGMENT_KIND == 1
    float approach=sign(.5-uv.x);
    target=vec2(approach*(.065+.10*bassHit*pulse),
        (uv.y-.5)*.025+.026*sin(uv.y*10.+midImpulse*.22*pulse));
#else
    float side=sign(uv.y-faultBoundary(uv));
    target=vec2(side*(.075+.13*midHit*pulse),side*bassHit*pulse*.018);
#endif
    target*=.8+.4*warp;
    target+=structureCurrent(uv)*.22;

    if(simulationReset>.5){colour=vec4(target,0.,pigmentArt(uv).a);return;}
    vec2 old=texture(input0,uv).rg;
    vec2 previous=texture(input0,clamp(uv-old*stepDelta,0.,1.)).rg;
    vec2 velocity=mix(previous,target,1.-exp(-stepDelta*(2.+motion*3.)));
    velocity=clamp(velocity,vec2(-.4),vec2(.4));
    float curl=(texture(input0,uv+vec2(px.x,0)).g-texture(input0,uv-vec2(px.x,0)).g
        -texture(input0,uv+vec2(0,px.y)).r+texture(input0,uv-vec2(0,px.y)).r)*.5;
    colour=vec4(velocity,curl,pigmentArt(uv).a);
}

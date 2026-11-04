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

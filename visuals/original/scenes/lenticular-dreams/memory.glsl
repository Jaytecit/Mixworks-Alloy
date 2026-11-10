#version 330
#define COMPOSITION 6
// Owned recurrent optical colour, separate from vector/chemical/particle state.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,motion,feedback,imageMix;
uniform float bassHit,midHit,trebleHit,pulse,midImpulse,trebleImpulse;
out vec4 colour;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec4 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));vec4 c=vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));return vec4(c.rgb*c.a,c.a);}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,p=uv-.5,q=p;float t=simulationTime;
    vec4 fresh=hasCutout!=0 ? structurePigment(uv) : art(uv);
    if(hasImage==0&&hasCutout==0)fresh=vec4(.5+.3*sin(uv.x*13.),.2,.5+.4*cos(uv.y*11.),1);
    if(simulationReset>.5){colour=fresh;return;}
#if COMPOSITION == 0
    q=rot(stepDelta*.08)*p*(1.+stepDelta*.18);
#elif COMPOSITION == 1
    float a=atan(p.y,p.x),r=length(p);q=vec2(cos(a+stepDelta*(.2+r)),sin(a+stepDelta*(.2+r)))*r*(1.-stepDelta*.14);
#elif COMPOSITION == 2
    q=p*(1.+stepDelta*.5/(.2+dot(p,p)*3.));q+=p.yx*stepDelta*.08;
#elif COMPOSITION == 3
    q=rot(stepDelta*.16)*p*vec2(1.+stepDelta*.27,1.-stepDelta*.08)+vec2(stepDelta*.02,0);
#elif COMPOSITION == 4
    q=rot(stepDelta*(p.x<0.?-.23:.23))*p;
#elif COMPOSITION == 5
    q=vec2(p.x*(1.+stepDelta*.3),p.y*(1.+stepDelta*.3))+vec2(stepDelta*.035*sin(p.y*9.+t*.2),0);
#elif COMPOSITION == 6
    q=p+vec2(stepDelta*.03*sin(floor(uv.y*60.)*.7+t*.3),stepDelta*.007);
#else
    vec2 px=2./resolution;float a=dot(texture(input0,uv+vec2(px.x,0)).rgb,vec3(.3,.6,.1)),b=dot(texture(input0,uv+vec2(0,px.y)).rgb,vec3(.3,.6,.1));
    q=p+vec2(b-a,a-b)*stepDelta*.18;
#endif
    q+=vec2(midHit,trebleHit)*pulse*stepDelta*vec2(.025,-.018);
    q+=structureCurrent(uv)*stepDelta*.22;

    vec4 old=texture(input0,clamp(q+.5,0.,1.));
    vec3 shifted=old.gbr;float cycle=stepDelta*(.04+.12*midHit);
    old.rgb=mix(old.rgb,shifted,cycle)*exp(-stepDelta*.04);
    float replenish=(.28+(1.-feedback)*.65+bassHit*pulse*.5)*stepDelta*imageMix;
#if COMPOSITION == 2
    replenish=max(replenish,stepDelta*.9*imageMix);
#endif
    colour=mix(old,fresh,clamp(replenish,0.,1.));
}

#version 330
#define PIGMENT_KIND 0
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float intensity;
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

float density(vec2 uv){return texture(input0,clamp(uv,0.,1.)).a;}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    vec4 pigment=texture(input0,uv);
    vec3 c=vec3(0);
#if PIGMENT_KIND == 0
    // The retained inlet makes the moving jet and its decaying trail.
    vec2 g=vec2(density(uv+vec2(px.x,0))-density(uv-vec2(px.x,0)),
        density(uv+vec2(0,px.y))-density(uv-vec2(0,px.y)));
    c=pigment.rgb*3.2;
    float edge=length(g)*(.12+.75*trebleHit*pulse);
    c+=edge*vec3(.1,.6,1.);
#elif PIGMENT_KIND == 1
    // Two independent fronts carry complementary source halves, never two logos.
    float left=pressMask(uv,-1.),right=pressMask(uv,1.),mask=max(left,right);
    vec4 direct=pigmentArt(pressSource(uv,-1.))*left+pigmentArt(pressSource(uv,1.))*right;
    c=mix(pigment.rgb,direct.rgb,.30)*mask;
    float ridge=pow(mask,.35)*(1.-mask);
    c+=ridge*(.10+1.1*trebleHit*pulse)*vec3(.15,.65,1.)*max(pigment.a,direct.a);
    c*=smoothstep(.06,.095,abs(uv.x-.5));
#else
    // Two broad pigment banks slip across one open jagged fault.
    float distance=abs(uv.y-faultBoundary(uv));
    float mask=smoothstep(faultGap(),faultGap()+.009,distance);
    vec2 q=faultSource(uv);vec4 direct=pigmentArt(q),past=texture(input0,clamp(q,0.,1.));
    c=mix(direct.rgb,past.rgb,.28)*mask;
    float lip=exp(-pow((distance-faultGap()-.008)*150.,2.));
    c+=lip*(.08+.65*trebleHit*pulse)*vec3(.3,.65,1.)*max(direct.a,past.a);
#endif
    colour=vec4(clamp(pow(max(c,0.),vec3(.85))*(.7+.6*intensity),0.,1.),1.);
}

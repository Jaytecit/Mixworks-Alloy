#version 330
// Persistent pigment is transported by vector state; never interpreted as velocity.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,motion,feedback,imageMix,pulse,bassHit,trebleHit;
out vec4 colour;
vec4 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));vec4 c=vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));return vec4(c.rgb*c.a,c.a);}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;vec4 fresh=art(uv);
    if(hasImage==0&&hasCutout==0)fresh=vec4(.4+.3*sin(uv.x*12.),.2,.6+.3*cos(uv.y*9.),1);
    if(simulationReset>.5){colour=fresh;return;}
    vec2 flow=texture(input1,uv).rg;
    vec2 chart=clamp(uv-flow*stepDelta*(.6+motion),0.,1.);
    vec4 old=texture(input0,chart);
#if COMPOSITION == 2
    vec2 px=1./resolution;vec4 a=texture(input0,chart+vec2(px.x,px.y)),b=texture(input0,chart+vec2(-px.x,px.y));
    vec4 chosen=dot(a.rgb,vec3(.3,.6,.1))<dot(b.rgb,vec3(.3,.6,.1))?a:b;
    old=mix(old,chosen,.12+.3*bassHit*pulse);
#elif COMPOSITION == 4
    old=mix(old,texture(input0,clamp(chart+flow.yx*stepDelta*.3,0.,1.)),.25);
#endif
    float feed=(.14+.18*(1.-feedback)+trebleHit*pulse*.5)*imageMix*stepDelta;
    vec3 transported=old.rgb*exp(-stepDelta*.035);
    transported+=vec3(-flow.y,flow.x,flow.x-flow.y)*bassHit*pulse*.004;
    colour=mix(vec4(clamp(transported,0.,1.5),old.a),fresh,clamp(feed,0.,1.));
}

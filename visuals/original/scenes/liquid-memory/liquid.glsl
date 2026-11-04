#version 330
// Original advected damped height/velocity field. B records image forcing.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,motion,warp,imageMix;
uniform float bassHit,midHit,trebleHit,bass,mid,pulse,midImpulse;
out vec4 colour;
float height(vec2 uv){return texture(input0,clamp(uv,0.,1.)).r;}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 cut=sampleCutout(materialDomain(uv));
    vec3 ink=(hasImage!=0||hasCutout!=0)?mix(background.rgb,cut.rgb,cut.a):vec3(.5+.3*sin(uv.x*17.)*cos(uv.y*13.));
    float source=dot(ink,vec3(.2126,.7152,.0722))-.35;
    if(simulationReset>.5){colour=vec4(source*.45,0.,source,1.);return;}
    vec2 grad=vec2(height(uv+vec2(px.x,0))-height(uv-vec2(px.x,0)),height(uv+vec2(0,px.y))-height(uv-vec2(0,px.y)));
    vec2 spin=vec2(-grad.y,grad.x);
    vec2 drift=vec2(sin(uv.y*7.+simulationTime*.23),cos(uv.x*6.-simulationTime*.19));
    vec2 chart=clamp(uv-(drift*.13+spin*(12.+mid*10.))*px*(.4+warp),0.,1.);
    vec2 s=texture(input0,chart).rg;
    float lap=height(chart+vec2(px.x,0))+height(chart-vec2(px.x,0))+height(chart+vec2(0,px.y))+height(chart-vec2(0,px.y))-4.*s.x;
    float ring=exp(-pow((length((uv-.5)*vec2(1.78,1.))-.26)*19.,2.));
    float force=source*imageMix*(.0015+.012*trebleHit*pulse)+ring*bassHit*pulse*.008;
    float rate=stepDelta*60.*(.65+motion*.6);
    float velocity=(s.y+(lap*.16-s.x*.003+force)*rate)*exp(-rate*.065);
    colour=vec4(clamp(s.x+velocity*rate+lap*.045,-1.,1.),clamp(velocity,-.2,.2),source,1.);
}

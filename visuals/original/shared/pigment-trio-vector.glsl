#version 330
// Retained bounded velocity/curl state; local feed is in the pigment pass.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta;
out vec4 colour;
// PIGMENT_TRIO_COMMON
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
    if(simulationReset>.5){colour=vec4(target,0.,pigmentArt(uv).a);return;}
    vec2 old=texture(input0,uv).rg;
    vec2 previous=texture(input0,clamp(uv-old*stepDelta,0.,1.)).rg;
    vec2 velocity=mix(previous,target,1.-exp(-stepDelta*(2.+motion*3.)));
    velocity=clamp(velocity,vec2(-.4),vec2(.4));
    float curl=(texture(input0,uv+vec2(px.x,0)).g-texture(input0,uv-vec2(px.x,0)).g
        -texture(input0,uv+vec2(0,px.y)).r+texture(input0,uv-vec2(0,px.y)).r)*.5;
    colour=vec4(velocity,curl,pigmentArt(uv).a);
}

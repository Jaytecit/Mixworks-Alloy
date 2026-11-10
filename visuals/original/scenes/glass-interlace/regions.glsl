#version 330
uniform sampler2D userImage;
// Original persistent region control state: displacement, channel offset, gate, age.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float simulationReset,stepDelta,simulationTime,bassHit,midHit,trebleHit,pulse,motion,warp;
out vec4 colour;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
    vec2 cell=floor(gl_FragCoord.xy);vec4 s=texelFetch(input0,ivec2(cell),0);
    if(simulationReset>.5){colour=vec4(0,0,0,hash(cell));return;}
    float age=s.w+stepDelta*(.45+motion);
    float event=floor(age)-floor(s.w),seed=hash(cell+floor(age)*vec2(7.3,3.1));
    float trigger=max(event*step(.56,seed),step(.22,bassHit*pulse)*step(.8,seed));
    vec2 stamp=(cell+.5)/resolution;
    float coverage=structureCoverage(stamp);
    vec2 current=structureCurrent(stamp);
    trigger=max(trigger,coverage*max(event,.55*spriteStructurePush));
    seed=mix(seed,fract(seed+coverage*.55+current.x*.4),coverage);

    vec3 wanted=vec3((seed-.5)*(.05+.32*warp),(.5-hash(cell+age))* .07,step(.68,seed));
    wanted.xy+=current*.26;
    wanted.z=max(wanted.z,coverage*spriteStructurePush);

    vec3 state=mix(s.xyz*exp(-stepDelta*vec3(2.1,3.2,2.8)),wanted,trigger);
    state.y+=(trebleHit+midHit)*pulse*(seed-.5)*.012;
    colour=vec4(state,age);
}

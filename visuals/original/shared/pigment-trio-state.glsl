#version 330
// Premultiplied retained pigment; three different source-entry and release rules.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta;
out vec4 colour;
// PIGMENT_TRIO_COMMON
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

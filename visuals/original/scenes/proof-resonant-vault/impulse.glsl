#version 330
// Bounded 1-D travelling pressure field. Bass attacks enter at the far opening
// (x=1), travel toward the viewer and leave at x=0 without reflection.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float simulationReset,stepDelta,bassHit;
out vec4 colour;
void main(){
    float x=gl_FragCoord.x/resolution.x;
    if(simulationReset>.5){colour=vec4(0);return;}
    float back=x+stepDelta*.32;
    float pressure=back>1.?bassHit:texture(input0,vec2(back,.5)).r*exp(-stepDelta*.65);
    float previous=texture(input0,vec2(x,.5)).g;
    float relaxed=mix(previous,pressure,1.-exp(-stepDelta*13.));
    colour=vec4(pressure,relaxed,0,1);
}

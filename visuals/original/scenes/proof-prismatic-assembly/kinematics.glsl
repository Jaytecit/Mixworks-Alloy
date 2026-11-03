#version 330
// Persistent travel/turn phases: release slows the movement, never reverses it.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float simulationReset,stepDelta,bass,mid,treble,bassHit,midHit,trebleHit,motion,pulse;
out vec4 colour;
void main(){
    float id=floor(gl_FragCoord.x);
    if(simulationReset>.5){colour=vec4(id/resolution.x,id*2.399,id*.73,id*.43);return;}
    vec4 s=texelFetch(input0,ivec2(int(id),0),0);
    float speed=(.55+motion)*( .86+.22*sin(id*7.));
    s+=stepDelta*vec4((.0025+bass*.004)*speed,
        (.035+mid*.25+(bassHit*5.5+midHit*2.4)*pulse)*speed,
        (.06+treble*.35+trebleHit*7.*pulse)*speed,
        (.04+mid*.15+(midHit*3.+trebleHit*4.)*pulse)*speed);
    colour=s;
}

#version 330
#define FORM 1
// Eight persistent parent transforms, velocities and musical travel phases.
uniform sampler2D input0;
uniform float simulationReset,stepDelta,energy,bassHit,mid,treble,motion;
out vec4 state;
vec4 destination(float g,float t,float e){
 float a=g*2.39996;
 vec2 p=vec2(cos(a)*(.38+.065*g),sin(a)*(.31+.025*g));
 p+=vec2(.09*sin(t*.21+g),.085*cos(t*.26+g*1.7));
 float turn=.38*sin(t*.19+g*1.3)+.22*cos(a);
 float size=(g<3.?1.06:.65)+.045*sin(t*.27+g);
 if(FORM==1){
  p=vec2(cos(a)*(.40+.09*g),sin(a)*(.35+.033*g));
  p+=vec2(.07*sin(t*.29+g),.07*cos(t*.23+g));
  turn=.16*sin(t*.30+g);size=g<3.?.74:.40;
 }
 if(FORM==2){
  p=vec2((mod(g,3.)-1.)*.91,(floor(g/3.)-1.)*.52);
  p+=vec2(.07*sin(t*.22+g),.04*cos(t*.31+g));
  turn=.14*sin(t*.32+g);size=g<3.?.80:.64;
 }
 return vec4(p,turn,size);
}
void main(){
 int g=int(gl_FragCoord.x),row=int(gl_FragCoord.y);
 vec4 p=texelFetch(input0,ivec2(g,0),0),v=texelFetch(input0,ivec2(g,1),0),q=texelFetch(input0,ivec2(g,2),0);
 if(simulationReset>.5){state=row==0?destination(float(g),0.,0.):vec4(0);return;}
 vec4 target=destination(float(g),q.x,q.y);
 // Damped parent movement continues through release; never a beat-direction flip.
 vec4 nextV=(v+(target-p)*stepDelta*2.2)*exp(-stepDelta*2.4);
 if(row==0)state=p+nextV*stepDelta;
 else if(row==1)state=nextV;
 else state=vec4(q.x+stepDelta*(.28+motion*.65+q.y*.7),mix(q.y,energy,1.-exp(-stepDelta*2.)),
                 max(q.z*exp(-stepDelta*2.5),bassHit),q.w+stepDelta*(.13+mid*.7+treble*.12));
}

#version 330
uniform float midImpulse,trebleImpulse,bassHit,trebleHit;
#define FAMILY 4
uniform sampler2D input0;
uniform float scale,detail,mode,generationBlend,handoffFamily,handoffMode,handoffScale;
uniform vec2 resolution;
uniform float projectBeat;
out vec2 sourceUV,localUV;
flat out float speed;
vec3 shape(int family,int kind,vec4 state,vec2 home,vec2 size,float amount){
 float speed=length(state.zw),angle=0.;vec2 halfSize=(.32+.3*amount)/size;
if(family==0){
 angle=(state.x-home.x)*7.+(state.y-home.y)*4.;
}else if(family==1){
 angle=atan(state.w,state.z)*smoothstep(.01,.12,speed);halfSize*=vec2(1.25,.72);
}else if(family==2){
 angle=(state.x-home.x)*10.;
}else if(family==3){
 halfSize*=kind==3?vec2(.75,1.25):vec2(1.18,.8);
}else if(family==4){
 angle=(state.y-home.y)*4.;
}else if(family==5||family==7){
 angle=atan(state.y-.5,state.x-.5)+1.5708;halfSize*=vec2(1.3,.7);
}else if(family==6){
 angle=atan(state.w,state.z)*smoothstep(.01,.06,speed);halfSize*=vec2(1.5,.6);
}else{
 angle=atan(state.w,state.z)*smoothstep(.01,.1,speed);halfSize*=vec2(1.+min(.8,speed*2.),.75);
}
 return vec3(halfSize,angle);
}
void main(){
 ivec2 size=textureSize(input0,0),cell=ivec2(gl_InstanceID%size.x,gl_InstanceID/size.x);
 vec4 state=texelFetch(input0,cell,0);
 vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
 vec2 corner=corners[gl_VertexID],home=(vec2(cell)+.5)/vec2(size);
 localUV=corner;sourceUV=home+corner*mix(handoffFamily<0.?.5:.49,.49,generationBlend)/vec2(size);speed=length(state.zw);
 vec2 halfSize=(.32+.3*scale)/vec2(size);
 float angle=0.;int kind=int(floor(mode*3.+.5));
vec3 current=shape(FAMILY,kind,state,home,vec2(size),scale);
 vec3 prior=handoffFamily<0.?vec3(.5/vec2(size),0.):shape(int(handoffFamily+.5),int(floor(handoffMode*3.+.5)),state,home,vec2(size),handoffScale);
 halfSize=mix(prior.xy,current.xy,generationBlend);
 float difference=mod(current.z-prior.z+3.14159265,6.2831853)-3.14159265;
 angle=prior.z+difference*generationBlend;
 angle+=generationBlend*(midImpulse*.65+trebleImpulse*.9)*(mod(float(cell.x+cell.y),3.)-1.);
 halfSize*=1.+generationBlend*(bassHit*.32+trebleHit*.15);
 float passage=sin(mod(projectBeat-12.,32.)*.1963495408);
 float recall=exp(-passage*passage*28.);
 // Cross home with nonzero displacement slope; alignment has no flat hold.
 state.xy=home+(state.xy-home)*passage;halfSize=mix(halfSize,.5/vec2(size),recall);
 sourceUV=mix(sourceUV,home+corner*.5/vec2(size),recall);angle*=1.-recall;
 // Rotate in pixel space so non-square outputs preserve fragment proportions.
 vec2 offset=corner*halfSize*resolution;
 offset=mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*offset/resolution;
 gl_Position=vec4((state.xy+offset)*2.-1.,0.,1.);
}

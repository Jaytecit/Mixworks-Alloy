#version 330
#define FORM 1
uniform sampler2D input0,input1;
uniform vec2 resolution;
uniform float scale,generationBlend,handoffHierarchy;
out vec2 sourceUV,localUV;
out float sheen;
flat out float group,tiny;
vec2 curve(float u,float g,vec4 q){
 return vec2((u-.5)*1.72,.20*sin((u-.5)*5.7+g*.75+q.x*.5)+.09*sin(u*9.-q.x*.8+g));
}
vec3 shape(int form,int c,float g,vec2 corner,vec4 q,vec4 s){
 float small=float(c>=32);
 float u=(float(c%32)+.5)/32.;
 if(c>=32)u=(float(c-32)+.5)/16.;
 if(form==0){
  float width=mix(.015625,.009,small),v=corner.y*mix(.092,.022,small)*(0.75+scale*.5);
  float along=u+corner.x*width+s.x;
  vec2 centre=curve(along,g,q);
  vec2 tangent=normalize(curve(along+.002,g,q)-curve(along-.002,g,q));
  vec2 normal=vec2(-tangent.y,tangent.x);
  float orbit=c>=32?(.17+.055*sin(q.w*1.6+float(c)))*mix(-1.,1.,float(c%2)):0.;
  return vec3(centre+normal*(v+s.y+orbit),.5+.5*cos(along*5.7+g*.75+q.x*.5));
 }
 if(form==1){
  // Eight broad petals, each carrying four material tiles; 16 small satellites.
  float petal=floor(float(c)/4.),section=mod(float(c),4.);
  float a=-petal*.785398+q.w*.30+g*.51;
  float radial=.20+(section+.5+corner.x*.5)*.087+s.x;
  float side=corner.y*.105*sin((section+.5)*.65+.2)+s.y;
  if(c>=32){a=u*6.283185-q.w*.65+g;radial=.72+s.y+corner.y*.024;side=-corner.x*.026;}
  radial*=1.+.16*sin(q.x*.8+g)+q.z*.10;
  vec2 direction=vec2(cos(a),sin(a));
  return vec3(direction*radial+vec2(-direction.y,direction.x)*side,.6+.4*sin(a+radial*2.));
 }
 if(form==3){
  float a=-(u+corner.x*mix(.015625,.009,small))*6.283185+q.w*.30+g*.51;
  float r=mix(.43,.72,small)+corner.y*mix(.092,.022,small)+s.y;
  return vec3(vec2(cos(a),sin(a))*r,.6+.4*sin(a+r*2.));
 }
 // Four hinged panels carrying 2x4 child image tiles; satellites settle at corners.
 float panel=floor(float(c)/8.),tile=mod(float(c),8.);
 vec2 tileUV=vec2(mod(tile,2.),floor(tile/2.));
 vec2 unfolded=(tileUV+vec2(.5)+corner*.47)*vec2(.21,.135);
 float a=panel*1.570796+.35*sin(q.w*.6+g);
 float hinge=.75*sin(q.x*.65+g+panel*.9)+q.z*.35;
 float depth=sin(hinge)*unfolded.x;
 vec2 local=vec2(unfolded.x*cos(hinge)+.06,unfolded.y-.27)+s.xy;
 if(c>=32){a=float(c-32)*1.570796;local=vec2(.65,.26)+corner*.018+s.xy;depth=0.;}
 local=mat2(cos(a),sin(a),-sin(a),cos(a))*local/(1.+depth*.7);
 return vec3(local,.4+.6*abs(cos(hinge)));
}
vec3 morph(int from,int to,int c,float g,vec2 corner,vec4 q,vec4 s,float b){
 if(b<=0.)return shape(from,c,g,corner,q,s);
 if(b>=1.)return shape(to,c,g,corner,q,s);
 vec3 a=shape(from,c,g,vec2(0),q,s),z=shape(to,c,g,vec2(0),q,s);
 vec2 ax=(shape(from,c,g,vec2(1,0),q,s).xy-shape(from,c,g,vec2(-1,0),q,s).xy)*.5;
 vec2 zx=(shape(to,c,g,vec2(1,0),q,s).xy-shape(to,c,g,vec2(-1,0),q,s).xy)*.5;
 vec2 ay=(shape(from,c,g,vec2(0,1),q,s).xy-shape(from,c,g,vec2(0,-1),q,s).xy)*.5;
 vec2 zy=(shape(to,c,g,vec2(0,1),q,s).xy-shape(to,c,g,vec2(0,-1),q,s).xy)*.5;
 float angle=atan(ax.y,ax.x),difference=mod(atan(zx.y,zx.x)-angle+3.141593,6.283185)-3.141593;
 vec2 direction=vec2(cos(angle+difference*b),sin(angle+difference*b));
 // Interpolate orientation and length separately so panels cannot flatten at half-turns.
 float signA=sign(ax.x*ay.y-ax.y*ay.x),signZ=sign(zx.x*zy.y-zx.y*zy.x);
 float height=mix(length(ay)*signA,length(zy)*signZ,b);
 vec2 offset=direction*(corner.x*mix(length(ax),length(zx),b))+vec2(-direction.y,direction.x)*corner.y*height;
 return vec3(mix(a.xy,z.xy,b)+offset,mix(a.z,z.z,b));
}
void main(){
 int g=gl_InstanceID/48,c=gl_InstanceID%48;
 vec4 p=texelFetch(input0,ivec2(g,0),0),q=texelFetch(input0,ivec2(g,2),0),s=texelFetch(input1,ivec2(c,g),0);
 vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
 vec2 corner=corners[gl_VertexID];tiny=float(c>=32);group=float(g);localUV=corner;
 float u=(float(c%32)+.5)/32.;
 if(c>=32)u=(float(c-32)+.5)/16.;
 vec3 current=shape(FORM,c,float(g),corner,q,s);
 int previous=handoffHierarchy<0.?FORM:int(handoffHierarchy+.5);
 float blend=generationBlend;
 vec3 result=morph(previous,FORM,c,float(g),corner,q,s,blend);
 // Coil first closes one continuous band, then its eight sectors open into petals.
 if(FORM==1&&previous==0){
  result=blend<.58?morph(0,3,c,float(g),corner,q,s,smoothstep(0.,.58,blend)):
                   morph(3,1,c,float(g),corner,q,s,smoothstep(.58,1.,blend));
 }
 vec2 local=result.xy;
 // True parent composition, in aspect-correct space, after child-local motion.
 mat2 rotation=mat2(cos(p.z),sin(p.z),-sin(p.z),cos(p.z));
 vec2 world=p.xy+rotation*local*p.w;
 gl_Position=vec4(world/vec2(resolution.x/resolution.y,1.),0.,1.);
 sourceUV=vec2(.12+.76*(u+corner.x*mix(.015625,.009,tiny)),.16+float(g)*.088+corner.y*.042);
 sheen=result.z;
}

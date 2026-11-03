#version 330
// Stable IDs: x is child 0..47, y is parent 0..7. XY local displacement, ZW velocity.
uniform sampler2D input0,input1;
uniform float simulationReset,stepDelta,bassHit,midHit,trebleHit,pulse,warp;
out vec4 state;
void main(){
 ivec2 id=ivec2(gl_FragCoord.xy);
 vec4 s=texelFetch(input0,id,0),p=texelFetch(input1,ivec2(id.y,2),0);
 if(simulationReset>.5){state=vec4(0);return;}
 float u=(float(id.x%32)+.5)/32.,g=float(id.y);
 // A travelling disturbance, with each child's own damped local momentum.
 float wave=sin(u*10.-p.x*2.1+g*.9);
 vec2 target=vec2(.015*sin(p.w+u*8.+g),(.022+warp*.035)*wave);
 target.y+=p.z*.065*sin(u*13.-p.x*3.+g);
 if(id.x>=32)target*=1.7;
 vec2 v=(s.zw+(target-s.xy)*stepDelta*18.)*exp(-stepDelta*5.);
 state=vec4(clamp(s.xy+v*stepDelta,vec2(-.22),vec2(.22)),clamp(v,vec2(-.7),vec2(.7)));
}

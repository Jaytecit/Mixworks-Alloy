#version 330
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float simulationReset,stepDelta,bassHit,beatPulse,pulse,feedback,focusX,focusY;
out vec4 fragColor;
void main(){vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
 vec4 c=texture(input0,uv)*.96;
 c+=.01*(texture(input0,uv+vec2(px.x,0))+texture(input0,uv-vec2(px.x,0))+texture(input0,uv+vec2(0,px.y))+texture(input0,uv-vec2(0,px.y)));
 if(simulationReset>.5){fragColor=c;return;}
 float push=pulse*clamp(1.25*bassHit+1.8*beatPulse,0.,1.);
 vec2 p=uv-mix(vec2(.5),vec2(focusX,focusY),.3);
 vec2 back=uv-p/max(.08,length(p))*push*45.*stepDelta/resolution;
 vec4 memory=texture(input1,clamp(back,0.,1.));
 memory*=exp(-stepDelta*(.18+(1.-feedback)*.3+push*1.4));
 fragColor=mix(memory,c,1.-exp(-stepDelta*(2.+push*14.)));}

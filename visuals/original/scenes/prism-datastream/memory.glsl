#version 330
#define COMPOSITION 1
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,stepDelta,simulationTime,feedback,bassHit,trebleHit,pulse;
out vec4 colour;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));vec4 fresh=vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));
if(simulationReset>.5){colour=fresh;return;}
vec2 q=uv+vec2(sin(uv.y*9.+simulationTime*.3),cos(uv.x*8.))*stepDelta*(.012+.012*float(COMPOSITION));
vec4 old=texture(input0,clamp(q,0.,1.));old.rgb=mix(old.rgb,old.brg,stepDelta*.07);
colour=mix(old,fresh,stepDelta*(.12+(1.-feedback)*.4+trebleHit*pulse*.4));}

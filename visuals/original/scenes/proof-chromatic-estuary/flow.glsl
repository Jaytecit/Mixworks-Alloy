#version 330
// Broad estuarine currents with inertial, locally varying audio forces.
// XY pixels/second, Z pigment, W relief: shared semantic, distinct dynamics.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,bass,mid,trebleHit,bassHit,motion,pulse,feedback,scale;
out vec4 colour;
float ink(vec2 p){
    vec4 c=hasImage!=0?sampleFittedMedia(p):vec4(0),s=sampleCutout(p);
    return dot(mix(c.rgb,s.rgb,s.a),vec3(.21,.72,.07));
}
vec2 current(vec2 p){
    float t=simulationTime*.16,k=3.8+scale*2.;
    vec2 v=vec2(sin(p.y*k+t)+.6*cos(p.x*2.-p.y*3.),-cos(p.x*k-t)*.8);
    return v;
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution,p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
    float seed=ink(uv);
    vec2 g=vec2(ink(uv+vec2(px.x,0))-ink(uv-vec2(px.x,0)),ink(uv+vec2(0,px.y))-ink(uv-vec2(0,px.y)));
    if(simulationReset>.5){colour=vec4(current(p)*9.,seed,0.);return;}
    vec4 old=texture(input0,uv),c=texture(input0,clamp(uv-old.xy*stepDelta*px,0.,1.));
    vec2 drive=current(p)*(12.+35.*bass+38.*bassHit*pulse)*(.5+motion);
    drive+=vec2(-g.y,g.x)*(35.+60.*mid)*feedback;
    drive+=vec2(sin(p.y*23.),cos(p.x*19.))*trebleHit*8.*pulse;
    vec2 v=mix(c.xy,drive,1.-exp(-stepDelta*(1.3+mid)));
    v*=smoothstep(vec2(0),px*4.,uv)*smoothstep(vec2(0),px*4.,1.-uv);
    colour=vec4(clamp(v,-80.,80.),mix(c.z,seed,stepDelta*.1),0.);
}

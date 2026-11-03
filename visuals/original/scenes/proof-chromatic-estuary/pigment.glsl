#version 330
// RGB pigment is advected by the shared signed velocity field. Artwork sets
// pigment boundaries and colour selection; it is not pasted over the display.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,imageMix,feedback,mid,trebleHit;
out vec4 colour;
vec2 reflectUV(vec2 p){return 1.-abs(mod(p,2.)-1.);}
vec4 art(vec2 uv){
    vec4 c=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 s=sampleCutout(uv);return vec4(mix(c.rgb,s.rgb,s.a),max(c.a,s.a));
}
vec3 palette(float x){
    // Defined pigment stops, not a global hue oscillator.
    vec3 a=vec3(.015,.08,.35),b=vec3(.015,.85,.78),c=vec3(1.,.18,.24),d=vec3(1.,.77,.07);
    x=fract(x);if(x<.28)return mix(a,b,smoothstep(0.,.28,x));
    if(x<.57)return mix(b,c,smoothstep(.28,.57,x));
    if(x<.80)return mix(c,d,smoothstep(.57,.80,x));
    return mix(d,a,smoothstep(.80,1.,x));
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
    vec4 image=art(uv);
    float value=dot(image.rgb,vec3(.21,.72,.07))*image.a;
    float band=.42+.23*sin(p.x*3.7+p.y*2.3)+.18*cos(p.y*6.-p.x*1.8);
    vec3 fresh=palette(band+value*.32*imageMix);
    fresh=mix(fresh,fresh*.30,smoothstep(.38,.55,value)*.6);
    if(simulationReset>.5){colour=vec4(fresh,1);return;}
    vec2 velocity=texture(input0,uv).xy/vec2(textureSize(input0,0));
    vec2 back=reflectUV(uv-velocity*stepDelta);
    vec3 carried=texture(input1,back).rgb;
    float replenish=1.-exp(-stepDelta*(.12+.45*trebleHit)*(.5+feedback));
    colour=vec4(clamp(mix(carried,fresh,replenish),0.,1.),1);
}

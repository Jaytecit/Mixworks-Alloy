#version 330
uniform vec2 resolution;
uniform float travelTime,bassImpulse,bassHit,kickImpact,pulse,warp;
out vec4 colour;

float grain(vec2 p) {
    vec3 q=fract(vec3(p.xyx)*vec3(.1031,.1030,.0973));
    q+=dot(q,q.yzx+33.33);
    return fract((q.x+q.y)*q.z);
}

float noise(vec2 p) {
    vec2 i=floor(p),f=fract(p);
    f=f*f*(3.-2.*f);
    return mix(mix(grain(i),grain(i+vec2(1,0)),f.x),
               mix(grain(i+vec2(0,1)),grain(i+1.),f.x),f.y);
}

vec2 eddies(vec2 p) {
    return vec2(noise(p),noise(p+vec2(11.7,27.4)))-.5;
}

void main() {
    vec2 p=(gl_FragCoord.xy-.5*resolution)/resolution.y;
    float swell=pulse*max(.62*bassHit,kickImpact);
    // The integral advances the field after an attack. The swell releases
    // independently, so successive kicks never rewind the marbling.
    float t=.34*travelTime+1.45*pulse*bassImpulse;
    vec2 q=3.1*p+vec2(.35,-.28)*t;
    vec2 broad=eddies(q);
    vec2 fold=eddies(1.6*q+(2.6+swell)*broad);
    vec2 veins=q+2.35*fold+1.1*broad;
    float pigment=.63*noise(veins)+.26*noise(3.4*veins)+.11*noise(10.*veins);
    float filament=.5+.5*cos(47.*pigment+2.8*noise(2.2*veins));
    vec2 displacement=(broad+.50*fold)*(.32+.68*warp)*(1.+1.9*swell);
    colour=vec4(displacement,pigment,filament);
}

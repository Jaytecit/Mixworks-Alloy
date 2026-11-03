#version 330
// Original evolving pigment atlas. Previous pigment is transported, while fresh
// artwork replenishes it. Geometry carries this independently moving material.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,mid,midTime,trebleHit,imageMix,feedback;
out vec4 colour;
vec2 reflected(vec2 p){return 1.-abs(mod(p,2.)-1.);}
vec3 source(vec2 uv){
    vec4 c=sampleMedia(reflected(uv));
    vec3 base=vec3(.055,.085,.105);
    if(hasImage!=0)base=mix(base,c.rgb*c.a,imageMix);
    if(hasCutout!=0){
        vec4 cut=sampleCutout(reflected(uv));
        base=mix(base,cut.rgb,cut.a*imageMix);
    }
    return base;
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 fresh=source(uv);
    if(simulationReset>.5){colour=vec4(fresh,1);return;}
    vec2 p=uv-.5;
    vec2 flow=vec2(sin(p.y*9.+midTime*.8),cos(p.x*8.-midTime*.7));
    vec2 carried=uv-stepDelta*(flow*(.006+.012*mid)+vec2(-p.y,p.x)*.022);
    vec3 previous=texture(input0,reflected(carried)).rgb;
    // Secondary scale inside the atlas creates nested contour echoes, never tiles.
    vec3 inner=texture(input0,reflected((carried-.5)*1.008+.5)).rgb;
    float refresh=1.-exp(-stepDelta*(8.+4.*trebleHit)*(1.2-.65*feedback));
    colour=vec4(clamp(mix(mix(previous,inner,.08),fresh,refresh),0.,1.),1);
}

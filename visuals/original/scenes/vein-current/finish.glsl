#version 330
uniform sampler2D input0,input1;
uniform vec2 resolution;
out vec4 colour;
float h(vec2 uv){return dot(texture(input0,uv).rgb,vec3(.21,.72,.07));}
void main(){vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;float height=h(uv);vec3 n=normalize(vec3(h(uv-vec2(px.x,0))-h(uv+vec2(px.x,0)),h(uv-vec2(0,px.y))-h(uv+vec2(0,px.y)),.012));
    float light=.25+.75*max(0.,dot(n,normalize(vec3(-.5,.7,1.))));float shine=pow(max(0.,dot(reflect(normalize(vec3(.4,-.7,-1.)),n),vec3(0,0,1))),30.);
    vec3 c=texture(input0,uv+n.xy*.012).rgb*light+vec3(.3,.55,.65)*shine*.6+texture(input1,uv).rgb*.12;
    c=max(c,vec3(0));float peak=max(c.r,max(c.g,c.b));float mapped=peak<=.72?peak:.72+.28*(1.-exp(-(peak-.72)/.28));c*=mapped/max(peak,.00001);
    c=mix(12.92*c,1.055*pow(c,vec3(1./2.4))-.055,step(vec3(.0031308),c));colour=vec4(clamp(c,0.,1.),1);}

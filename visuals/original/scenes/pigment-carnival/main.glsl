#version 330
#define COMPOSITION 5
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float detail,intensity,warp,bassHit;
out vec4 colour;
float h(vec2 uv){return dot(texture(input0,clamp(uv,0.,1.)).rgb,vec3(.2126,.7152,.0722));}
void main(){
    vec2 uv=materialDomain(gl_FragCoord.xy/resolution),px=2./vec2(textureSize(input0,0));
    vec2 g=vec2(h(uv+vec2(px.x,0))-h(uv-vec2(px.x,0)),h(uv+vec2(0,px.y))-h(uv-vec2(0,px.y)));
    vec3 n=normalize(vec3(-g*(2.+detail*6.),1.));
    vec3 c=texture(input0,clamp(uv+g*.025*warp,0.,1.)).rgb;
#if COMPOSITION == 1 || COMPOSITION == 4
    c*=.3+.7*max(0.,dot(n,normalize(vec3(-.5,.7,1.))));
    c+=vec3(.02,.7,1.)*pow(max(0.,dot(n,normalize(vec3(-.4,.5,1.)))),34.)*length(g);
#elif COMPOSITION == 2 || COMPOSITION == 6
    c=pow(max(c,0.),vec3(.8));
#else
    c+=vec3(.03,.12,.3)*abs(texture(input1,uv).b)*4.;
#endif
    colour=vec4(clamp(pow(max(c,0.),vec3(.72))*1.15,0.,1.),1.);
}

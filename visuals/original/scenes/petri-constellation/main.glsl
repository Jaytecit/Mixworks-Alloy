#version 330
#define COMPOSITION 1
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float intensity,detail,hue,imageMix,bassHit,trebleHit;
out vec4 colour;
float height(vec2 uv){return texture(input0,clamp(uv,0.,1.)).r;}
void main(){
    vec2 uv=materialDomain(gl_FragCoord.xy/resolution),px=1./vec2(textureSize(input0,0));
    vec4 state=texture(input0,uv);float p=state.r;
    vec2 g=vec2(height(uv+vec2(px.x,0))-height(uv-vec2(px.x,0)),height(uv+vec2(0,px.y))-height(uv-vec2(0,px.y)));
    vec3 n=normalize(vec3(-g*(9.+detail*14.),1.));
    vec3 a=vec3(.015,.85,.34),b=vec3(.42,.02,1.);
    float bands=smoothstep(.05,.85,p);
#if COMPOSITION == 1 || COMPOSITION == 6
    bands=pow(bands,.65);
#elif COMPOSITION == 5
    bands=(1.-abs(bands*2.-1.))*.75+p*.2;
#endif
    vec3 c=mix(a,b,clamp(p+state.g*.4+hue*.2,0.,1.))*bands;
    c*=.22+.78*max(0.,dot(n,normalize(vec3(-.5,.7,1.))));
    c+=vec3(.04,.65,1.)*pow(max(0.,dot(n,normalize(vec3(-.4,.45,1.)))),32.)*length(g)*2.;
    colour=vec4(clamp(c*1.4+vec3(.002,.003,.009),0.,1.),1.);
}

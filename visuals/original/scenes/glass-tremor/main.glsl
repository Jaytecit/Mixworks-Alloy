#version 330
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float warp,detail,imageMix,intensity,bassHit,trebleHit;
out vec4 colour;
float height(vec2 p){return texture(input0,clamp(p,0.,1.)).r+.45*spriteRelief(p);}
vec3 sourceArt(vec2 uv){
#if COMPOSITION == 1
    vec2 p=(uv-.5)*vec2(1.78,1.);float r=length(p),a=atan(p.y,p.x)+r*1.5;
    uv=.5+vec2(cos(a),sin(a))*r/vec2(1.78,1.);
#elif COMPOSITION == 2
    uv=fract(uv*2.);
#endif
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 cut=spriteMaterial(uv);
    return mix(background.rgb,cut.rgb,cut.a);
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    float h=height(uv);
    vec2 g=vec2(height(uv+vec2(px.x*2.,0))-height(uv-vec2(px.x*2.,0)),height(uv+vec2(0,px.y*2.))-height(uv-vec2(0,px.y*2.)))*.5;
    // Broad and fine derivatives produce a polished surface, not contour lines.
    g+=vec2(height(uv+vec2(px.x*4.,0))-height(uv-vec2(px.x*4.,0)),height(uv+vec2(0,px.y*4.))-height(uv-vec2(0,px.y*4.)))*.22;
    vec3 n=normalize(vec3(-g*(7.+detail*8.),1.));
    vec3 art=(hasImage!=0||hasCutout!=0)?sourceArt(clamp(uv+g*(.6+warp),0.,1.)):vec3(.05,.3,.7);
    vec3 reflection=(hasImage!=0||hasCutout!=0)?sourceArt(clamp(.5+n.xy*.35,0.,1.)):vec3(.4,.1,.8);
    float diffuse=.18+.82*max(0.,dot(n,normalize(vec3(-.6,.8,1.))));
    float spec=pow(max(0.,dot(n,normalize(vec3(-.5,.6,1.)))),54.);
    float fresnel=pow(1.-n.z,2.);
    vec3 body=mix(art*.68,reflection*.8,fresnel*.7)*diffuse;
#if COMPOSITION == 4
    vec3 film=.5+.5*cos(6.283185*(vec3(.0,.33,.67)+h*.8+n.z*.5));
    body=mix(art*.2,film*(.22+.65*diffuse),.65);
#endif
    body+=vec3(.05,.8,1.)*spec*(.25+.4*intensity)+vec3(.36,.01,.8)*max(0.,-h)*.45;
    colour=vec4(clamp(body,0.,1.),1.);
}

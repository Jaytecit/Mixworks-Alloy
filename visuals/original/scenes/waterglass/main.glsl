// Mixworks Waterglass: authored liquid optics, 2026-11-02.
// Image coordinates stay in screen space; waves bend light, not image proportions.
#version 330
uniform vec2 resolution;
uniform float sceneTime,projectBeat,energy,bass,mid,treble,bassHit,midHit,trebleHit,bassTime,midTime,trebleTime;
uniform float intensity,motion,imageMix,scale,warp,detail,hue,orbit,focusX,focusY,pulse;
uniform float colourR,colourG,colourB;
uniform sampler2D userImage;
uniform int hasImage;
out vec4 colour;

// Alpha-correct bilinear filtering: invisible texel RGB cannot stain the rim.
vec4 alphaTexel(sampler2D tex,ivec2 p,ivec2 size){
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);
    vec4 c=texelFetch(tex,p,0);return vec4(c.rgb*c.a,c.a);
}
vec4 alphaImage(sampler2D tex,vec2 uv,vec2 aspect){
    uv=(uv-.5)*aspect+.5;
    ivec2 size=textureSize(tex,0);vec2 p=uv*vec2(size)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);
    return mix(mix(alphaTexel(tex,i,size),alphaTexel(tex,i+ivec2(1,0),size),f.x),
               mix(alphaTexel(tex,i+ivec2(0,1),size),alphaTexel(tex,i+ivec2(1),size),f.x),f.y);
}
vec4 artwork(vec2 uv){
    if(hasCutout==0)return vec4(0);
    vec4 c=mix(alphaImage(cutoutImage,uv,cutoutAspect),alphaImage(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);
    return vec4(c.rgb/max(c.a,.00001),c.a);
}

mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec2 reflectEdges(vec2 uv){return 1.-abs(mod(uv,2.)-1.);}
vec3 imageLight(vec2 uv){
    // Aspect-correct cover. Reflect outside the image instead of border streaks.
    vec2 p=uv-.5; p.x*=resolution.x/resolution.y;
    p=turn((imageRotation-.5)*.20)*p; p.x/=resolution.x/resolution.y;
    p/=1.+imageZoom*.14;
    vec3 a=linearRGB(textureLod(userImage,reflectEdges(p*mediaAspect+.5),0.).rgb);
    vec3 b=linearRGB(textureLod(nextImage,reflectEdges(p*nextMediaAspect+.5),0.).rgb);
    return mix(a,b,imageBlend);
}
float field(vec2 p,float t){
    // Band clocks accumulate travel: a kick moves a wave onward rather than
    // swelling and returning the whole frame to the same position.
    vec2 centre=.55*vec2(sin(midTime*.63),cos(midTime*.47));
    vec2 d=p-centre;
    float curl=(.12+.95*mid)*sin(length(d)*1.2-midTime*2.8)+.4*mid*sin(projectBeat*1.570796);
    vec2 q=centre+turn(curl)*d;
    q+=.42*bass*sin(projectBeat*3.141593)*vec2(sin(p.y*1.7),cos(p.x*1.3));
    q+=vec2(sin(q.y*1.4-bassTime*2.7),cos(q.x*1.1-bassTime*2.1))*(.12+.42*bass);
    q+=.78*vec2(sin(p.y*1.35+t*.27)+cos(p.x*.92-t*.17),cos(p.x*1.1+t*.21));
    float v=sin(q.x*2.2+q.y*.73+t*.34);
    v+=.52*sin(q.y*3.5-q.x*1.1-t*.42);
    v+=.22*sin(q.x*6.4+q.y*4.2+t*.6);
    v+=.09*sin(q.y*12.-q.x*7.1-t*.8);
    v+=(.035+.08*treble)*sin(q.x*23.+q.y*17.+sin(q.y*7.)*2.+t*.9-trebleTime*19.);
    v+=(.08+.32*bass)*sin(length(p-centre)*4.-bassTime*10.-1.1*bass*sin(projectBeat*3.141593));
    return v;
}
vec3 pigment(float v){
    // Deep ink -> violet -> tropical cyan -> molten gold, in linear light.
    float a=.5+.5*sin(v*1.4+hue*2.2);
    float b=.5+.5*sin(v*1.1+2.5);
    vec3 c=mix(vec3(.009,.006,.045),vec3(.14,.012,.38),smoothstep(.1,.7,a));
    c=mix(c,vec3(.006,.48,.47),smoothstep(.42,.93,b));
    c=mix(c,vec3(.95,.27,.028),pow(.5+.5*sin(v*1.7+4.),8.));
    return c;
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float t=travelTime*.38;
    vec2 q=p*(1.15+scale*.65)-vec2(focusX-.5,focusY-.5)*.45;
    q=turn(.10*sin(t*.13)*(orbit+.2))*q;
    float h=field(q,t),eps=.003;
    vec2 slope=vec2(field(q+vec2(eps,0),t)-field(q-vec2(eps,0),t),field(q+vec2(0,eps),t)-field(q-vec2(0,eps),t))/(2.*eps);
#ifdef GEOMETRY_PROBE
    // Test-only height and slope output, independent of lights and pigments.
    colour=vec4(clamp(vec3(.5+h*.18,.5+slope.x*.12,.5+slope.y*.12),0.,1.),1);return;
#endif
    // The submerged object's alpha and visible luminance create optical relief.
    // This affects surface normals/illumination as well as its visible pigment.
    vec2 objectPosition=vec2(.28*sin(midTime*1.1)+.15*mid*sin(projectBeat*1.570796),-.05+.22*sin(bassTime*.85)+.09*bass*sin(projectBeat*3.141593));
    mat2 objectTurn=turn(.28*sin(midTime*.8));
    vec2 objectUV=objectTurn*(p-objectPosition)/1.35+.5;
    vec4 object=artwork(objectUV),ox=artwork(objectUV+vec2(.003,0)),oy=artwork(objectUV+vec2(0,.003));
    float height=object.a*(.3+dot(linearRGB(object.rgb),vec3(.21,.72,.07))*.2);
    vec2 relief=vec2(ox.a*(.3+dot(linearRGB(ox.rgb),vec3(.21,.72,.07))*.2)-height,
                    oy.a*(.3+dot(linearRGB(oy.rgb),vec3(.21,.72,.07))*.2)-height);
    slope+=clamp(relief/.003,vec2(-4),vec2(4))*imageMix*.22;
    vec3 n=normalize(vec3(-slope*(.22+warp*.16),1.));
    vec2 bend=n.xy*(.005+.018*warp+.006*bassHit*pulse);
    bend.x*=resolution.y/resolution.x;
    vec3 base=pigment(h*.8+q.x*.3+q.y*.25);
    // A subsurface artwork layer and the same optics/light field on both modes.
    if(hasImage!=0)base=mix(base,imageLight(uv+bend),imageMix);
    vec2 logo=objectUV+bend*.45;
    vec4 cut=artwork(logo);
    base=mix(base,linearRGB(cut.rgb),cut.a*imageMix*.88);
    vec3 L=normalize(vec3(-.5,.7,1.25));
    float diff=.54+.46*max(0.,dot(n,L));
    float trough=.18+.82*smoothstep(-1.1,.6,h);
    vec3 c=base*diff*(.94+.06*bass)*mix(trough,1.,hasImage!=0?imageMix:0.);
    // Broad reflected softboxes follow the normal; thin caustics ride the folds.
    vec3 r=reflect(vec3(0,0,-1),n);
    float soft=pow(max(0.,dot(r,normalize(vec3(.3,.8,1.)))),18.);
    float ribbon=pow(.5+.5*sin(h*5.2+q.y*.8-t*.22),48.);
    float micro=pow(.5+.5*sin(h*(16.+detail*12.)+t*.3),32.);
    c+=vec3(.18,.65,.78)*soft*(.09+.23*mid);
    c+=mix(vec3(.03,.35,.38),vec3(.65,.32,.08),.5+.5*n.x)*ribbon*(.4+.16*treble);
    c+=vec3(.28,.65,.78)*micro*(.065+.3*trebleHit)*detail;
    c+=vec3(.28,.43,.55)*pow(max(0.,dot(r,L)),100.)*(.16+.15*trebleHit);
    // Alpha supplies a subtle submerged rim, never hidden RGB outside the cutout.
    float edge=abs(artwork(logo+vec2(.003,0)).a-cut.a)+abs(artwork(logo+vec2(0,.003)).a-cut.a);
    c+=vec3(.06,.55,.65)*edge*imageMix*(.15+midHit*.5);
    c*=mix(vec3(.85),vec3(colourR,colourG,colourB)*.4+.72,.35);
    c*=.86+.36*intensity;
    colour=vec4(max(c,vec3(0)),1);
}

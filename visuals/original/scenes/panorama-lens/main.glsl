// Original Mixworks Panorama Lens; shared image-optics algorithm, composition 0.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
#define FAMILY 16
#define VARIANT 0
uniform vec2 resolution;
uniform float sceneTime,projectBeat,beatPulse,energy,bass,mid,treble,onset,audioMotion,intensity,motion,imageMix;
uniform sampler2D userImage;
uniform int hasImage;
out vec4 colour;
// Owned Mixworks primitives and lighting. No third-party shader includes.
uniform float scale,warp,detail,hue,symmetry,orbit,focusX,focusY;
uniform float colourR,colourG,colourB,pulse,mode,mirror,feedback;
uniform sampler2D historyImage;
uniform int historyValid;
uniform float bassHit,midHit,trebleHit,bassTime,midTime,trebleTime;
// Project beats give 120â€“160 BPM articulation; measured bands supply its strength.
// Independent accumulated band phases carry movement between attacks. No global zoom.
float lowSwing(){return .85*sin(bassTime*1.1+bassImpulse*4.2+float(VARIANT)*.73);}
float midSwing(){return .8*sin(midTime*.9+midImpulse*3.8+float(VARIANT)*1.17);}
float highSwing(){return .65*sin(trebleTime*1.2+trebleImpulse*5.+float(VARIANT)*.41);}
float drive(){return bassHit*pulse;}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.13,311.71)))*43757.37);}
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);float a=hash(i.xy+i.z*37.);float b=hash(i.xy+vec2(1,0)+i.z*37.);float c=hash(i.xy+vec2(0,1)+i.z*37.);float d=hash(i.xy+vec2(1)+i.z*37.);float e=hash(i.xy+(i.z+1.)*37.);float g=hash(i.xy+vec2(1,0)+(i.z+1.)*37.);float h=hash(i.xy+vec2(0,1)+(i.z+1.)*37.);float j=hash(i.xy+vec2(1)+(i.z+1.)*37.);return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,j,f.x),f.y),f.z);}
float fbm(vec3 p){float v=0.,a=.5;for(int i=0;i<4;i++){v+=noise(p)*a;p=p*2.03+vec3(2,7,3);a*=.5;}return v;}
vec3 palette(float v){vec3 c=.48+.48*cos(6.283*(v+vec3(.04,.32,.58)+hue+float(VARIANT)*.071));return pow(c,vec3(1.65))*mix(vec3(.5),vec3(colourR,colourG,colourB)*1.5,.6);}
vec2 domain(){vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;p-=vec2(focusX-.5,focusY-.5)*.8;p*=.65+scale*1.4;if(mirror>.5)p.x=abs(p.x);float n=floor(symmetry*6.99);if(n>0.){float a=atan(p.y,p.x),r=length(p);a=abs(mod(a+3.14159/(n+2.),6.28318/(n+2.))-3.14159/(n+2.));p=vec2(cos(a),sin(a))*r;}return rot((orbit-.5)*.4*sin(travelTime*.13))*p;}
float clockTime(){return travelTime+bassImpulse*1.8+midImpulse*.7;}
// World-plane UVs use equal units on both axes. Convert once to screen aspect.
// Reflect at photograph boundaries rather than stretching border pixels.
// Original Alloy material composition. Child charts inherit their parent chart,
// then turn, advect and refract independently; the grandchild inherits the child.
mat2 nestedTurn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 nestedLinear(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 nestedInk(vec2 uv){
    uv=1.-abs(mod(uv,2.)-1.);
    vec3 bg=mix(nestedLinear(textureLod(userImage,uv,0.).rgb),nestedLinear(textureLod(nextImage,uv,0.).rgb),imageBlend);
    vec4 cut=sampleCutout(uv);
    return mix(bg,nestedLinear(cut.rgb),cut.a*.85);
}
vec3 nestedMaterial(vec2 uv,vec3 parent,float style){
    vec2 chart=(uv-.5)*(2.4+mod(style,3.)*.55);
    vec2 cell=floor(chart+.5),p=fract(chart+.5)-.5;
    float id=dot(cell,vec2(1.71,2.93))+style;
    float turn=midImpulse*(.8+mod(style,3.)*.28)+midTime*.25;
    vec2 child=nestedTurn(turn+id*.37)*p;
    // Liquid inlays, angular frames and ribbon windows have distinct domains.
    if(mod(style,3.)<1.)child+=.07*vec2(sin(child.y*9.-bassImpulse*3.),cos(child.x*8.+trebleImpulse*4.));
    else if(mod(style,3.)<2.)child.x+=.12*sin(child.y*6.+bassImpulse*2.);
    else child=nestedTurn(.3*sin(length(child)*8.-bassImpulse*3.))*child;
    float shape=mod(style,3.)<1.?length(child):max(abs(child.x),abs(child.y));
    float mask=1.-smoothstep(.31,.36,shape);
    vec3 ink=nestedInk(child/(.65+.13*bassHit)+.5);
    vec2 grand=nestedTurn(-trebleImpulse*1.9-trebleTime*.4)*(child-vec2(.12*sin(id),.1*cos(id)))*2.8;
    float inner=1.-smoothstep(.22,.29,max(abs(grand.x),abs(grand.y)));
    ink=mix(ink,nestedInk(grand+.5),inner*.85);
    vec3 rim=.5+.5*cos(vec3(0,2.1,4.2)+id*.4+trebleImpulse*.35);
    ink+=rim*exp(-abs(shape-.32)*100.)*(.15+.8*trebleHit);
    return mix(parent,ink,mask*.78);
}

vec3 imagePlane(vec2 uv){
    vec2 p=rot((imageRotation-.5)*6.283185)*(uv-.5)/(1.+imageZoom*.25);
    p.x/=resolution.x/resolution.y;
    vec2 a=1.-abs(mod(p*mediaAspect+.5,2.)-1.);
    vec2 b=1.-abs(mod(p*nextMediaAspect+.5,2.)-1.);
    return nestedMaterial(uv,mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend),16.0);
}
vec4 alphaPixel(sampler2D tex,ivec2 p,ivec2 size){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);vec4 c=texelFetch(tex,p,0);return vec4(linearRGB(c.rgb)*c.a,c.a);}
vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){ivec2 s=textureSize(tex,0);vec2 p=((uv-.5)*aspect+.5)*vec2(s)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);return mix(mix(alphaPixel(tex,i,s),alphaPixel(tex,i+ivec2(1,0),s),f.x),mix(alphaPixel(tex,i+ivec2(0,1),s),alphaPixel(tex,i+ivec2(1),s),f.x),f.y);}
vec4 cutoutArt(vec2 uv){if(hasCutout==0)return vec4(0);vec4 c=mix(alphaArt(cutoutImage,uv,cutoutAspect),alphaArt(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);return vec4(c.rgb/max(c.a,.00001),c.a);}
vec3 media(vec2 uv,vec3 fallback){
    if(hasImage==0)return fallback;
    vec3 src=imagePlane(uv);float m=floor(mode*3.99);
    if(m==1.)src=fallback*smoothstep(.15,.65,dot(src,vec3(.21,.72,.07)))*1.7;
    else if(m==2.)src=abs(imagePlane(uv+vec2(.004,0))-imagePlane(uv-vec2(.004,0)))*8.+fallback*.2;
    else if(m==3.)src=imagePlane(uv+(src.rg-.5)*warp*.025);
    return mix(fallback,src,imageMix);
}
vec3 material(vec3 p,vec3 n,float id){float diff=max(0.,dot(n,normalize(vec3(-.6,.8,1.))));float rim=pow(1.-abs(n.z),3.);float spec=pow(max(0.,dot(reflect(normalize(vec3(.6,-.8,-1.)),n),vec3(0,0,1))),24.+detail*40.);vec3 albedo=palette(id*.11+fbm(p*2.)*.3);albedo=media(p.xy*.18+.5,albedo);
#if FAMILY == 4 || FAMILY == 5
vec4 decal=cutoutArt(p.xy*.35+.5);albedo=mix(albedo,decal.rgb,decal.a*imageMix);
#endif
return albedo*(.13+diff*.85)+palette(id*.1+.3)*rim*.45+vec3(spec)*.5;}
void finish(vec3 c,vec2 p){c*=.65+intensity*.6;c*=.85+.15*exp(-dot(p,p)*.4);colour=vec4(max(c,vec3(0)),1);}

vec3 opticalInk(vec2 uv){vec2 p=uv-.5;float ribs=sin(length(p)*18.-bassTime*5.+sin(atan(p.y,p.x)*3.+midTime*2.));return palette(ribs*.2+p.x*.2)*(.3+.7*smoothstep(-.65,.3,ribs));}
void main(){vec2 p=domain();float t=clockTime();float cells=2.+float(VARIANT%4)+floor(detail*2.);
    vec2 q=rot(float(VARIANT%3)*.4)*p*cells;
    q.x+=.55*lowSwing()*sin(floor(q.y)*1.7);q.y+=.4*midSwing()*cos(floor(q.x)*1.3);
    vec2 id=floor(q),f=fract(q)-.5;float seed=hash(id);
    f=rot(midImpulse*(.8+seed)+trebleImpulse*.65)*f;
    float r=length(f),beat=.2+.8*bassHit*(.5+.5*sin(seed*6.283+bassImpulse));float line=0.;
#if FAMILY == 14
    // Graphic assemblage with independent cell shapes and cutouts.
    float shape=mod(floor(seed*9.)+floor(mode*3.99)+float(VARIANT),4.);
    if(shape<1.)line=1.-smoothstep(.2,.22,r);else if(shape<2.)line=1.-smoothstep(.19,.21,max(abs(f.x),abs(f.y)));else if(shape<3.)line=1.-smoothstep(.04,.06,abs(r-(.18+.1*beat)));else line=1.-smoothstep(.03,.05,min(abs(f.x),abs(f.y)));
    if(VARIANT==0){vec3 img=media(p*.5+.5,palette(seed)*.15);vec4 sprite=cutoutArt(f/(.65+.2*beat)+.5);if(hasCutout!=0)img=mix(img,sprite.rgb,sprite.a*imageMix);float mask=1.-smoothstep(.4,.48,max(abs(f.x),abs(f.y)));finish(img*mask,p);return;}
    if(VARIANT==1)line=1.-smoothstep(.02,.035,abs(abs(f.x)+abs(f.y)-(.3+.1*beat)));
    if(VARIANT==2)line=1.-smoothstep(.05,.07,min(abs(f.x),abs(f.y)));
    if(VARIANT==3)line=step(.0,sin(f.x*20.+t))*step(.0,sin(f.y*20.-t));
    if(VARIANT==4)line=step(f.y,.3*sin(f.x*8.+t+seed*4.));
    if(VARIANT==5)line=1.-smoothstep(.02,.04,abs(length(f+vec2(.5,.5))-.6));
    if(VARIANT==6)line=smoothstep(.2,.24,abs(f.x))*smoothstep(.2,.24,abs(f.y));
#elif FAMILY == 15
    // Scanline discontinuities, channel separation, block masking.
    float row=floor(p.y*(18.+detail*40.));vec2 uv=p*.5+.5;
    uv.x+=sin(row*13.+floor(projectBeat*2.))*(.015+.13*treble)*warp;
    uv+=vec2(.12*lowSwing()*sin(row),.10*midSwing());
    if(VARIANT==1)uv=abs(fract(uv*3.+vec2(t*.02,0))*2.-1.);
    if(VARIANT==2)uv.x=floor(uv.x*16.)/16.;
    if(VARIANT==3)uv.y+=hash(vec2(floor(uv.x*24.),1))*.3*sin(t);
    vec3 src=media(uv,palette(uv.y+sin(uv.x*8.+t)*.2));
    if(VARIANT==4)src=floor(src*4.)/4.;
    if(VARIANT==5)src=abs(src-media(uv+vec2(.012,0),palette(uv.x)))*2.;
    float scan=.5+.5*sin(p.y*resolution.y+trebleTime*12.);src*=.8+.2*scan;finish(src,p);return;
#elif FAMILY == 16
    // Image lens / mirrored tiles / rippled photograph, independent media mode.
    vec2 optical=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    optical=rot(midImpulse*.08)*domain();
    p=optical;
    vec2 uv=optical*.5+.5;float m=floor(mode*3.99);
    if(VARIANT==0){uv=vec2(atan(p.y,p.x)/6.283+.5,length(p)*.3);uv.y+=(.06*treble+.025*trebleHit)*sin(atan(p.y,p.x)*8.-trebleTime*7.);}
    else if(VARIANT==1)uv=abs(fract(uv*3.)*2.-1.);
    else if(VARIANT==2)uv=uv;
    else if(VARIANT==3)uv+=warp*(.01+.025*bass)*vec2(sin(p.y*9.-bassTime*7.),cos(p.x*8.-trebleTime*9.));
    else if(VARIANT==4)uv=.5+(uv-.5)*(1.+warp*length(p)*length(p));
    else uv+=vec2(.04*lowSwing(),.025*midSwing());
    uv+=vec2(.025*lowSwing(),.012*highSwing());
    vec3 base=media(uv,opticalInk(uv));
    if(VARIANT==2){vec4 sprite=cutoutArt((rot(.2*midSwing())*p-vec2(.28*lowSwing(),.15*highSwing()))/1.6+.5);base=mix(base,sprite.rgb,sprite.a*imageMix);}
    if(VARIANT==5){base=vec3(0);vec2 direction=vec2(cos(midTime*2.+midSwing()),sin(midTime*2.+midSwing()));for(int j=-4;j<=4;j++){vec2 tap=uv+direction*float(j)*(.003+.04*treble+.012*trebleHit);base+=media(tap,opticalInk(tap))/9.;}}
    finish(base,p);return;
#else
    line=exp(-abs(r-.2-.06*sin(t+seed*8.))*70.);
#endif
    vec3 c=mix(palette(seed)*.025,media((id+f)/cells*.3+.5,palette(seed+beat*.08)),line);finish(c,p);}

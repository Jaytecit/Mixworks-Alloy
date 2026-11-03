// Original Mixworks Chrome Labyrinth; shared isosurface algorithm, composition 0.
#version 330
#define FAMILY 19
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
float lowSwing(){return bass*sin(projectBeat*3.141593+float(VARIANT)*.73);}
float midSwing(){return mid*sin(projectBeat*1.570796+float(VARIANT)*1.17);}
float highSwing(){return treble*sin(projectBeat*6.283185+float(VARIANT)*.41);}
float drive(){return bassHit*pulse;}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.13,311.71)))*43757.37);}
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);float a=hash(i.xy+i.z*37.);float b=hash(i.xy+vec2(1,0)+i.z*37.);float c=hash(i.xy+vec2(0,1)+i.z*37.);float d=hash(i.xy+vec2(1)+i.z*37.);float e=hash(i.xy+(i.z+1.)*37.);float g=hash(i.xy+vec2(1,0)+(i.z+1.)*37.);float h=hash(i.xy+vec2(0,1)+(i.z+1.)*37.);float j=hash(i.xy+vec2(1)+(i.z+1.)*37.);return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,j,f.x),f.y),f.z);}
float fbm(vec3 p){float v=0.,a=.5;for(int i=0;i<4;i++){v+=noise(p)*a;p=p*2.03+vec3(2,7,3);a*=.5;}return v;}
vec3 palette(float v){vec3 c=.48+.48*cos(6.283*(v+vec3(.04,.32,.58)+hue+float(VARIANT)*.071));return pow(c,vec3(1.65))*mix(vec3(.5),vec3(colourR,colourG,colourB)*1.5,.6);}
vec2 domain(){vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;p-=vec2(focusX-.5,focusY-.5)*.8;p*=.65+scale*1.4;if(mirror>.5)p.x=abs(p.x);float n=floor(symmetry*6.99);if(n>0.){float a=atan(p.y,p.x),r=length(p);a=abs(mod(a+3.14159/(n+2.),6.28318/(n+2.))-3.14159/(n+2.));p=vec2(cos(a),sin(a))*r;}return rot((orbit-.5)*.4*sin(travelTime*.13))*p;}
float clockTime(){return travelTime;}
// World-plane UVs use equal units on both axes. Convert once to screen aspect.
// Reflect at photograph boundaries rather than stretching border pixels.
vec3 imagePlane(vec2 uv){
    vec2 p=rot((imageRotation-.5)*6.283185)*(uv-.5)/(1.+imageZoom*.25);
    p.x/=resolution.x/resolution.y;
    vec2 a=1.-abs(mod(p*mediaAspect+.5,2.)-1.);
    vec2 b=1.-abs(mod(p*nextMediaAspect+.5,2.)-1.);
    return mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend);
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

// Signed-distance sculptural families: occlusion, normals, ambient shadow and fog.
float box(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
float sdf(vec3 p){
    float t=clockTime();
#if FAMILY == 0
    p.xy-=vec2(.30*midSwing()*sin(p.z*.65),(.24*highSwing()+.12*trebleHit)*cos(p.z));
    p.xy=rot(.22*lowSwing()*sin(p.z*.8-bassTime*2.))*p.xy;
#elif FAMILY == 2
    p.xy=rot(.34*midSwing()+.18*sin(midTime*2.+p.z*.7)*mid)*p.xy;
    p+=.12*treble* sin(p.yzx*2.5-trebleTime*6.);
#elif FAMILY == 3
    p.x+=.45*midSwing()*sin(p.z*.9-midTime*2.);
#elif FAMILY == 4
    p.xy=rot(.6*lowSwing())*p.xy;
    p.yz=rot(.65*midSwing())*p.yz;
    p.xz=rot(.45*highSwing()+.16*trebleHit)*p.xz;
    p.z+=.10*treble*sin(p.x*5.-trebleTime*7.);
#elif FAMILY == 5
    p.xz=rot(.85*lowSwing())*p.xz;
    p.yz=rot(.75*midSwing())*p.yz;
    p.xy=rot(.28*highSwing())*p.xy;
#elif FAMILY == 19
    p.yz=rot(.65*lowSwing())*p.yz;
    p.xz=rot(.5*midSwing())*p.xz;
    p+=.09*treble*sin(p.zxy*4.-trebleTime*5.);
#endif
    p.xy=rot(t*.08*(orbit+.15))*p.xy;
#if FAMILY == 0
    // Undulating mineral cave, holes and hanging columns.
    float wall=2.1-length(p.xy)+.35*sin(p.z*1.7+t)*sin(p.x*2.3)+warp*(fbm(p*2.)-.5);
    vec3 q=p;q.xy-=vec2(1.3*sin(floor(p.z/2.)*3.),.8);q.z=mod(q.z,2.)-1.;
    if(VARIANT==1){vec3 c=p;c.xz=mod(c.xz+1.5,3.)-1.5;return min(min(length(c.xz)-.22,p.y+1.5),1.6-p.y);}
    if(VARIANT==2)return 1.6-length(p.xy)+.18*sin(p.z*4.+t)*sin(atan(p.y,p.x)*7.)+noise(p*3.)*.25;
    if(VARIANT==3){float roof=2.-length(vec2(p.x,p.y*.7));vec3 c=p;c.z=mod(c.z+1.,2.)-1.;return min(roof,length(vec2(abs(c.x)-1.5,c.z))-.18);}
    return min(wall,length(q.xz)-(.12+.13*bass));
#elif FAMILY == 1
    // Repeated structural portal, changes cross-section and twists along flight.
    p.xy=rot(p.z*(.09+warp*.2)+t*.12)*p.xy;float z=mod(p.z,1.7)-.85;
    float ring=(VARIANT%3==0?length(p.xy):max(abs(p.x),abs(p.y)));
    return max(abs(ring-(1.25+.15*bass))-.055,abs(z)-(.08+.10*detail));
#elif FAMILY == 2
    // Infinite cubic scaffolding / crossed rods.
    vec3 q=mod(p+1.,2.)-1.;float r=.05+detail*.10+bass*.05;
    float rods=min(length(q.xy),min(length(q.yz),length(q.zx)))-r;
    if(VARIANT==1)return min(rods,length(q)-(.24+warp*.15));
    if(VARIANT==2){vec3 edge=abs(q)-.72;return min(length(edge.xy),min(length(edge.yz),length(edge.xz)))-.065;}
    return rods;
#elif FAMILY == 3
    // Displaced landscape; stepped plateaux and wire ridges vary the topology.
    float h=fbm(vec3(p.xz*(.5+warp),t*.05))*1.3;
    h+=.27*bass*sin(p.z*2.4-bassTime*5.)+.13*mid*sin(p.x*3.+midTime*3.)+(.10*treble+.025*trebleHit)*sin(p.x*11.+p.z*7.-trebleTime*9.);
    if(VARIANT%3==1)h=floor(h*6.)/6.;
    return (p.y+1.1-h-bass*.25)*.65;
#elif FAMILY == 4
    // Interlocking polished tori with a hollow axial core.
    p.xz=rot(t*.13)*p.xz;p.yz=rot(t*.11)*p.yz;
    float a=length(vec2(length(p.xy)-1.,p.z))-(.17+.11*bass);
    float b=length(vec2(length(p.yz)-.78,p.x))-(.12+warp*.12);
    if(VARIANT==0)b=length(p)-(.58+.12*sin(t));
    if(VARIANT==1){p.xy=rot(p.z*1.6+t*.12)*p.xy;return length(vec2(length(p.xy)-.75,p.z))-.17;}
    if(VARIANT==2)return min(a,min(b,length(vec2(length(p.xz)-.95,p.y))-.10));
    if(VARIANT==3)return min(a,min(length(p)-.35,length(vec2(length(p.xz)-1.4,p.y))-.055));
    return min(a,b);
#elif FAMILY == 5
    // Faceted convex solids, satellites, displaced crystalline core.
    p.xz=rot(t*.14)*p.xz;p.yz=rot(t*.19)*p.yz;
    float d=(abs(p.x)+abs(p.y)+abs(p.z)-1.45-bass*.2)*.577;
    if(VARIANT==1)d=length(p)-1.05+(fbm(p*4.)-.5)*.14;
    if(VARIANT==2)d=max(box(p,vec3(.75)),-box(p,vec3(.84,.38,.38)));
    if(VARIANT==3)d=max(box(p,vec3(.8)),-box(p,vec3(.66,.66,.95)));
    vec3 q=p; q.xy-=vec2(1.35*cos(t*.4+.7*lowSwing()),1.35*sin(t*.4+.7*lowSwing()));
    return min(d, length(q)-.17-warp*.1);
#elif FAMILY == 18
    // Repeated city blocks of different heights, narrow glowing street grid.
    vec2 cell=floor(p.xz);vec3 q=vec3(fract(p.x)-.5,p.y,fract(p.z)-.5);
    float h=.25+hash(cell)*1.8+(.25+.22*sin(cell.x*2.+cell.y-bassTime*4.))*bass;
    q.xz=rot(.22*midSwing()*sin(cell.x+cell.y))*q.xz;
    q.x+=(.12*highSwing()+.10*trebleHit)*sin(q.y*4.);
    return min(box(q-vec3(0,h*.5-1.,0),vec3(.31,h*.5,.31)),p.y+1.);
#else
    // Isosurface shell with folded chambers.
    float g=dot(sin(p*(2.5+float(VARIANT)*.8)+t*.1),cos(p.zxy*(2.5+float(VARIANT)*.8)));
    if(VARIANT==1){vec3 q=p;float d=box(q,vec3(1.25));float s=1.;for(int i=0;i<3;i++){q=mod(p*s+1.,2.)-1.;s*=3.;vec3 r=abs(1.-3.*abs(q));float hole=(min(max(r.x,r.y),min(max(r.y,r.z),max(r.z,r.x)))-1.)/s;d=max(d,hole);}return d;}
    if(VARIANT==2)return max(abs(g)*.20-.035,box(p,vec3(1.15)));
    if(VARIANT==3)return max(abs(length(p)-1.35)-.055,abs(sin(p.y*7.+t*.2))*.12-.04);
    return max(abs(g)*.20-.035,length(p)-1.6);
#endif
}
void main(){vec2 p=domain();float t=clockTime();vec3 ro=vec3(0,0,3.4);vec3 rd=normalize(vec3(p*.65,-1.8));
#if FAMILY <= 2 || FAMILY == 3 || FAMILY == 18
    // Two-beat surge/recoil, lateral syncopation and a longer curved route.
    float route=t*.35+bassTime*2.4+1.0*lowSwing();
    ro=vec3(.18*sin(midTime)+.28*midSwing(),.12*highSwing(),-route*(.65+float(VARIANT%3)*.25));
    rd=normalize(vec3(rot(.12*midSwing())*p,1.7));
#if FAMILY == 2
    ro.xy=vec2(.48,.46)+vec2(.14*midSwing(),.10*highSwing());rd=normalize(vec3(rot(.2*lowSwing())*p*.65,1.7));
#elif FAMILY == 3
    ro.y=1.3+.15*drive();rd=normalize(vec3(p.x+.2*midSwing(),p.y-.38,1.7));
#elif FAMILY == 18
    ro.x=.01+.08*midSwing();ro.y=1.8+.18*lowSwing();rd=normalize(vec3(p.x+.25*midSwing(),p.y-.4,1.7));
#endif
#endif
    float dist=0.;bool hit=false;vec3 q;
    for(int i=0;i<64;i++){q=ro+rd*dist;float d=sdf(q);if(d<.0025*(1.+dist*.1)){hit=true;break;}dist+=max(.004,d*.7);if(dist>22.)break;}
    vec3 c=vec3(.004,.007,.018)+palette(.4)*.025*exp(-length(p));
    if(hit){vec2 e=vec2(.004,0);vec3 n=normalize(vec3(sdf(q+e.xyy)-sdf(q-e.xyy),sdf(q+e.yxy)-sdf(q-e.yxy),sdf(q+e.yyx)-sdf(q-e.yyx)));
    float ao=clamp(sdf(q+n*.14)/.14,.18,1.);c=material(q,n,float(VARIANT)+dist*.08)*ao;
    vec3 reflection=reflect(rd,n);float env=pow(fbm(reflection*3.+vec3(t*.04)),3.);
#if FAMILY == 4 || FAMILY == 5
    c=c*.6+palette(env*.4)*env*.75+vec3(pow(max(0.,dot(reflection,normalize(vec3(-.4,.7,-.6)))),50.))*1.2;
#endif
#if FAMILY == 18
    float windows=step(.65,fract(q.y*7.))*step(.5,fract((q.x+q.z)*8.));c=c*.18+palette(q.y*.2)*windows*(.5+treble)+vec3(.01,.02,.03);
#endif
    float seams=pow(.5+.5*sin(q.x*12.+sin(q.z*9.-trebleTime*5.)+q.y*9.),24.);c+=palette(.8)*seams*.26;
    c=mix(c,vec3(.005,.01,.025),1.-exp(-dist*.075));}
    finish(c,p);}

// Original Mixworks Cloud Atlas; shared cosmos algorithm, composition 0.
#version 330
#define FAMILY 9
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

// Musical positions are shared by lights, strands and substantial focal artworks.
vec2 node(float id,float t){
    float a=id*2.399+t*.13+midTime*.7;vec2 centre;
#if FAMILY == 10
    centre=vec2(sin(id*1.73+t*.23),cos(id*.93-t*.17))*(.4+hash(vec2(id,8))*2.);
    if(VARIANT==1)centre=vec2(cos(a),sin(a))*(.2+id*.025);
    if(VARIANT==2)centre=vec2(sin(id*.618+t*.3)*2.,sin(id+t)*.4);
    if(VARIANT==3)centre=vec2(sin(id*1.7+t*.2),cos(id*.9+t*.3))*(.6+.2*sin(midTime));
    if(VARIANT==4)centre=vec2(id/12.-2.,sin(id*.4+t*.6)*.6);
    centre+=vec2(.38*lowSwing()*cos(id*.8),.44*midSwing()*sin(id*.7));
    centre+=.16*treble*vec2(cos(id+trebleTime*5.),sin(id-trebleTime*7.));
#elif FAMILY == 11
    centre=vec2(cos(a+.5*midSwing()),sin(a+.5*midSwing()))*(.3+mod(id,8.)*.18+.25*lowSwing());
    if(VARIANT==0)centre=vec2(id/12.-2.,sin(id*.4+midTime*2.)*(.1+bass*.7)+.22*lowSwing()*cos(id));
    centre+=(.4*highSwing()+.18*trebleHit)*vec2(sin(id),cos(id));
#else
    centre=(vec2(hash(vec2(id,1)),hash(vec2(id,2)))-.5)*5.;
    centre=rot(.25*midSwing()+t*.03)*centre;
    centre+=vec2(.35*lowSwing(),.22*highSwing())*sin(id);
#endif
    float dep=.5+.5*sin(id*1.71+t*.06+bassTime*.8);
    return centre/(.7+dep*2.4);
}
void main(){vec2 p=domain();float t=clockTime();vec3 c=vec3(.002,.004,.012);
    if(hasImage!=0)c=mix(c,imagePlane(p*.5+.5)*.32,imageMix);
#if FAMILY == 9
    for(int z=0;z<7;z++){
        float depth=1.+float(z)*.7;vec2 q=rot(t*.02+float(z)*.13+.2*midSwing())*p*depth;
        q+=vec2(.4*lowSwing(),.25*highSwing())*depth;
        float cloud=pow(fbm(vec3(q*1.5,float(z)*2.+t*.07+midTime)),4.);
        c+=palette(float(z)*.11+length(q)*.06)*cloud*.5/depth;
    }
#endif
    for(int i=0;i<48;i++){
        float id=float(i);vec2 centre=node(id,t);float r=length(p-centre);
        float size=.009+detail*.018;float bright=.35+hash(vec2(id,6))*.6;
        c+=palette(id*.031)*bright*(exp(-r*r/(size*size))+.008/(.005+r*r*90.));
#if FAMILY == 10
        vec2 next=node(mod(id+1.,48.),t),v=next-centre;
        float d=length(p-centre-v*clamp(dot(p-centre,v)/max(dot(v,v),.001),0.,1.));
        c+=palette(id*.03)*exp(-d*200.)*.09;
#endif
    }
#if FAMILY == 10 || FAMILY == 11
    // Three large orbital artworks instead of 48 repeated tiny stickers.
    for(int j=0;j<3;j++){
        float id=float(j);vec2 centre=node(id*13.+3.,t)*.55;
        vec2 local=rot(.25*midSwing()+id*2.094)*(p-centre)/(.62+detail*.38);
        vec4 art=cutoutArt(local+.5);
        c=mix(c,art.rgb*(.72+.28*cos(local.y*1.4)),art.a*imageMix);
    }
#endif
    finish(c,p);
}

// Original Mixworks Veiled Cells; shared reaction-field algorithm, composition 4.
#version 330
#define FAMILY 17
#define VARIANT 4
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

// Bounded temporal field. This is a new simulation, not reference simulation code.
uniform float deltaTime;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec2 p=domain();float t=clockTime();vec2 px=1./resolution;float style=float(VARIANT%5);vec2 flow=vec2(sin(p.y*(2.+style*.7)+t*.2),cos(p.x*(2.5+style*.4)-t*.17));
    if(VARIANT%3==1)flow=vec2(-p.y,p.x)/(length(p)+.4);if(VARIANT%3==2)flow=rot(t*.1)*vec2(sin(p.y*5.),.5);
    // Alternating jets, snare shear and high-frequency eddies move the history.
    flow=rot(1.2*midSwing())*flow*(.4+1.8*bass);
    flow+=vec2(1.8*lowSwing()*cos(p.y*2.),1.4*midSwing()*sin(p.x*2.));
    flow+=.7*treble*vec2(sin(p.y*9.-trebleTime*8.),cos(p.x*8.+trebleTime*7.));
    float stepSize=clamp(deltaTime*60.,0.,3.);
    vec2 at=uv+flow*(.001+warp*.003)*stepSize;vec3 prev=historyValid!=0?texture(historyImage,clamp(at,0.,1.)).rgb:vec3(0);
    vec3 c;
#if FAMILY == 17
    // Reaction-like diffusion: local Laplacian and bounded nonlinear growth.
    vec3 lap=texture(historyImage,clamp(at+vec2(px.x,0),0.,1.)).rgb+texture(historyImage,clamp(at-vec2(px.x,0),0.,1.)).rgb+texture(historyImage,clamp(at+vec2(0,px.y),0.,1.)).rgb+texture(historyImage,clamp(at-vec2(0,px.y),0.,1.)).rgb-4.*prev;
    vec2 source=p+vec2(.28*lowSwing(),.24*midSwing());
    float n=fbm(vec3(source*2.,t*.13+trebleTime));
    float field;
    if(VARIANT==0)field=.5+.5*sin(source.x*9.+n*9.+source.y*2.);
    else if(VARIANT==1)field=pow(.5+.5*cos(source.x*7.+n*4.)*cos(source.y*7.-n*4.),2.);
    else if(VARIANT==2)field=smoothstep(.32,.67,fbm(vec3(source*3.,midTime*.7)));
    else if(VARIANT==3)field=.5+.5*sin(source.y*12.+sin(source.x*3.+midTime)*3.+n*3.);
    else if(VARIANT==4)field=pow(abs(sin(source.x*7.+n*6.)*sin(source.y*6.-n*6.)),.45);
    else if(VARIANT==5){vec2 tile=fract(source*4.+n*.3)-.5;field=exp(-dot(tile,tile)*22.);}
    else if(VARIANT==6)field=.5+.5*sin(length(source)*13.+atan(source.y,source.x)*3.-bassTime*4.+n*4.);
    else field=pow(.5+.5*sin(source.x*14.+sin(source.y*7.+n*3.)*3.),5.);
    vec3 seed=palette(field*.45+float(VARIANT)*.13)*(.025+.65*field);
    c=clamp(prev+lap*min(.20,.08*stepSize)+(seed-prev)*min(.16,.07*stepSize),0.,1.6);
    if(historyValid==0)c=seed;
#else
    p=rot(.55*midSwing())*p+vec2(.25*lowSwing(),.18*highSwing());
    float ribbon=exp(-abs(p.y-.5*sin(p.x*(2.+style)+t*.7+bassTime*3.)-.15*sin(p.x*11.-trebleTime*7.))*(18.+detail*24.));
    if(VARIANT%3==1)ribbon=exp(-abs(length(p)-.7-.18*sin(atan(p.y,p.x)*(3.+style)+t))*30.);
    if(VARIANT%3==2)ribbon=pow(fbm(vec3(p*(3.+style),t*.2)),8.)*5.;
    vec3 injection=palette(p.x*.15+t*.03)*ribbon*(.1+mid*.4+beatPulse*.2*pulse);
    c=prev*pow(.85+feedback*.12,stepSize)+injection*stepSize;
    if(historyValid==0)c=injection;
#endif
    c=mix(c,media(uv+flow*.03,c),hasImage!=0?.025:0.);colour=vec4(c,1);
}

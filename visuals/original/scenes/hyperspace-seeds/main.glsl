// Original Mixworks Hyperspace Seeds; shared folded-mandala algorithm, composition 0.
#version 330
#define FAMILY 13
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
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.13,311.71)))*43757.37);}
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);float a=hash(i.xy+i.z*37.);float b=hash(i.xy+vec2(1,0)+i.z*37.);float c=hash(i.xy+vec2(0,1)+i.z*37.);float d=hash(i.xy+vec2(1)+i.z*37.);float e=hash(i.xy+(i.z+1.)*37.);float g=hash(i.xy+vec2(1,0)+(i.z+1.)*37.);float h=hash(i.xy+vec2(0,1)+(i.z+1.)*37.);float j=hash(i.xy+vec2(1)+(i.z+1.)*37.);return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,j,f.x),f.y),f.z);}
float fbm(vec3 p){float v=0.,a=.5;for(int i=0;i<4;i++){v+=noise(p)*a;p=p*2.03+vec3(2,7,3);a*=.5;}return v;}
vec3 palette(float v){vec3 c=.48+.48*cos(6.283*(v+vec3(.04,.32,.58)+hue+float(VARIANT)*.071));return c*mix(vec3(.5),vec3(colourR,colourG,colourB)*1.5,.6);}
vec2 domain(){vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;p-=vec2(focusX-.5,focusY-.5)*.8;p*=(.65+scale*1.4)/(1.+bass*.1+beatPulse*pulse*.06);if(mirror>.5)p.x=abs(p.x);float n=floor(symmetry*6.99);if(n>0.){float a=atan(p.y,p.x),r=length(p);a=abs(mod(a+3.14159/(n+2.),6.28318/(n+2.))-3.14159/(n+2.));p=vec2(cos(a),sin(a))*r;}return rot((orbit-.5)*.4*sin(sceneTime*.13))*p;}
float clockTime(){return travelTime+audioMotion*.3;}
vec3 media(vec2 uv,vec3 fallback){
    if(hasImage==0)return fallback;
    vec3 src=sampleMedia(uv).rgb;float m=floor(mode*3.99);
    if(m==1.)src=fallback*smoothstep(.15,.65,dot(src,vec3(.21,.72,.07)))*1.7;
    else if(m==2.)src=abs(sampleMedia(uv+vec2(.004,0)).rgb-sampleMedia(uv-vec2(.004,0)).rgb)*8.+fallback*.2;
    else if(m==3.)src=sampleMedia(uv+(src.rg-.5)*warp*.12).rgb;
    return mix(fallback,src,imageMix*sampleMedia(uv).a);
}
vec3 material(vec3 p,vec3 n,float id){float diff=max(0.,dot(n,normalize(vec3(-.6,.8,1.))));float rim=pow(1.-abs(n.z),3.);float spec=pow(max(0.,dot(reflect(normalize(vec3(.6,-.8,-1.)),n),vec3(0,0,1))),24.+detail*40.);vec3 albedo=palette(id*.11+fbm(p*2.)*.3);albedo=media(p.xy*.18+.5,albedo);
#if FAMILY == 4 || FAMILY == 5
vec4 decal=sampleCutout(p.xy*.35+.5);albedo=mix(albedo,decal.rgb,decal.a*imageMix);
#endif
return albedo*(.13+diff*.85)+palette(id*.1+.3)*rim*.45+vec3(spec)*(.5+treble*2.);}
void finish(vec3 c,vec2 p){c*=(.65+intensity*.6)*(.85+energy*.3);c*=.85+.15*exp(-dot(p,p)*.4);colour=vec4(max(c,vec3(0)),1);}

void main(){vec2 p=domain();float t=clockTime();vec3 c=vec3(0);
#if FAMILY == 12
    // Escape-time Julia / Mandelbrot fields with orbit-trap relief.
    vec2 z=p*(.75+warp*.5),k=vec2(-.72+.12*sin(t*.11),.18+.1*cos(t*.13));if(VARIANT%2==0){k=z+vec2(-.4,0);z=vec2(0);}
    float count=0.,trap=9.;for(int i=0;i<64;i++){z=vec2(z.x*z.x-z.y*z.y,2.*z.x*z.y)+k;trap=min(trap,abs(length(z)-.7));if(dot(z,z)>40.)break;count+=1.;}
    c=palette(count*.023+hue*.2)*(.25+exp(-trap*8.));c*=.25+.75*smoothstep(64.,40.,count);
#else
    // Repeated folding, orbit-trap threads and angular mandala.
    vec2 q=p;float glow=0.;
    if(VARIANT==1)q=vec2(atan(p.y,p.x),log(length(p)+.2));
    if(VARIANT==2)q=rot(t*.1)*p/(dot(p,p)+.5);
    if(VARIANT==3)q=vec2(length(p),atan(p.y,p.x)*.7);
    if(VARIANT==4)q=sin(p*2.);
    if(VARIANT==5)q=abs(fract(p*.5+t*.02)*2.-1.);
    for(int i=0;i<7;i++){q=abs(q)-vec2(.55+.07*sin(t*.17),.34);q=rot(.55+float(VARIANT%5)*.25+warp*.2)*q;q*=1.35;float d=abs(length(q)-.3-bass*.05);if(VARIANT%3==1)d=abs(max(abs(q.x),abs(q.y))-.35);if(VARIANT%3==2)d=abs(q.x*q.y)*.5;glow+=exp(-d*(30.+detail*60.))/float(i+1);}
    c=palette(length(q)*.04+length(p)*.2)*glow*(.55+mid*.6);c+=palette(.7)*pow(max(0.,cos(atan(p.y,p.x)*(6.+float(VARIANT%5)*2.)+t*.2)),32.)*exp(-abs(length(p)-.7)*12.)*.4;
#endif
    c=media(p*.3+.5,c);finish(c,p);}

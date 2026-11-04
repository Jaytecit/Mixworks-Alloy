#version 330
#define COMPOSITION 5
// Owned implicit sculptures. Image luminance changes the surface itself.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bass,mid,treble,bassHit,midHit,trebleHit,midImpulse,trebleImpulse;
uniform float motion,warp,scale,detail,imageMix,pulse,intensity;
out vec4 colour;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));return mix(a.rgb,b.rgb,b.a);}
vec2 sphereUV(vec3 p){vec3 n=normalize(p+vec3(.0001));return vec2(atan(n.z,n.x)/6.283185+.5,asin(clamp(n.y,-1.,1.))/3.141593+.5);}
float soft(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}
float capsule(vec3 p,vec3 a,vec3 b,float r){vec3 q=p-a,ab=b-a;return length(q-ab*clamp(dot(q,ab)/dot(ab,ab),0.,1.))-r;}
float shape(vec3 p){
    float t=travelTime*.45+midImpulse*.13*pulse;vec3 q=p;q.xz=rot(t*.25)*q.xz;
    float feed=dot(art(sphereUV(q)),vec3(.2126,.7152,.0722));
    float bump=(feed-.3)*imageMix*(.03+.07*warp),d=0.;
#if COMPOSITION == 0
    d=length(q)-.78;
    for(int i=0;i<3;i++){float a=float(i)*2.094+t;vec3 centre=vec3(cos(a),sin(a*1.2)*.5,sin(a))*.62;
        d=soft(d,length(q-centre)-(.46+bassHit*pulse*.12),.42);}
#elif COMPOSITION == 1
    float g=abs(dot(sin(q*3.+t*.2),cos(q.yzx*3.-t*.1)))*.23-.09;
    d=max(length(q)-1.22,g);
#elif COMPOSITION == 2
    d=length(q)-.58;
    for(int i=0;i<7;i++){float a=float(i)*.8976+t*.55;vec3 centre=vec3(cos(a),sin(a*2.+t)*.55,sin(a))*1.05;
        d=soft(d,length(q-centre)-(.2+.13*feed+.06*bassHit),.18);}
#elif COMPOSITION == 3
    vec3 cell=floor(q*1.7),f=fract(q*1.7)-.5;
    f+=sin(cell.yzx*2.1+t)*.1;d=max(length(q)-1.25,(length(f)-(.27+.08*feed+bassHit*.035))/1.7);
#elif COMPOSITION == 4
    q.x+=sin(q.y*2.+t)*.25;q.z+=cos(q.y*1.7-t*.7)*.18;
    d=capsule(q,vec3(0,-.9,0),vec3(0,.9,0),.43+.13*cos(q.y*4.-t));
#elif COMPOSITION == 5
    q.xy=rot(.6)*q.xy;vec2 ring=vec2(length(q.xz)-.82,q.y);
    d=length(ring)-(.26+.06*cos(atan(q.z,q.x)*7.+t)+bassHit*.06);
#elif COMPOSITION == 6
    float poly=(dot(abs(q),vec3(1)) -1.85)*.57735;
    d=max(max(length(q)-1.14,poly),-(length(q)-.98));
    d=max(d,-length(q.xy-vec2(.3,.15))+.38);
#else
    d=10.;
    for(int i=0;i<6;i++){float a=float(i)*1.0472+t*.3;
        vec3 a0=vec3(cos(a),-.8,sin(a))*.8,b0=vec3(cos(a+1.8),.8,sin(a+1.8))*.8;
        d=soft(d,capsule(q,a0,b0,.16+.04*bassHit),.12);}
#endif
    return d-bump;
}
void main(){
    vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float orbit=travelTime*.10+float(COMPOSITION)*.34;
    vec3 eye=vec3(sin(orbit)*2.85,.35+sin(orbit*.7)*.4,cos(orbit)*2.85);
    vec3 f=normalize(-eye),r=normalize(cross(f,vec3(0,1,0))),u=cross(r,f);
    vec3 ray=normalize(f*1.9+r*p.x+u*p.y);float distance=0.;bool hit=false;vec3 q=eye;
    for(int i=0;i<72;i++){q=eye+ray*distance;float d=shape(q);if(d<.004){hit=true;break;}distance+=max(.003,d*.68);if(distance>7.)break;}
    vec3 c=vec3(.003,.006,.018);
    if(hit){float e=.004;vec3 n=normalize(vec3(shape(q+vec3(e,0,0))-shape(q-vec3(e,0,0)),shape(q+vec3(0,e,0))-shape(q-vec3(0,e,0)),shape(q+vec3(0,0,e))-shape(q-vec3(0,0,e))));
        vec3 ink=(hasImage!=0||hasCutout!=0)?art(sphereUV(q)):vec3(.05,.35,.9);
        float light=.35+.65*max(0.,dot(n,normalize(vec3(-.6,.8,1.))));
        float spec=pow(max(0.,dot(reflect(ray,n),normalize(vec3(-.6,.8,1.)))),40.);
        float rim=pow(1.-max(0.,dot(n,-ray)),3.);
        vec3 material=ink*.82+vec3(.035,.10,.17);
        c=material*light+art(sphereUV(reflect(ray,n)))*.12+vec3(.02,.65,1.)*(spec*.6+rim*.38);
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

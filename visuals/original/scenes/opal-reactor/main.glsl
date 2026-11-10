#version 330
// Owned implicit sculptures. Image luminance changes the surface itself.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bass,mid,treble,bassHit,midHit,trebleHit,midImpulse,trebleImpulse;
uniform float projectBeat,kickImpact;
uniform float motion,warp,scale,detail,imageMix,pulse,intensity;
uniform int sourceComposition, targetComposition;
uniform float sceneMorph;
out vec4 colour;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 art(vec2 uv,vec3 nativeInk){
    vec4 background=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 cut=surfaceSpriteMaterial(uv);
    return mix(mix(nativeInk,background.rgb,background.a),cut.rgb,cut.a);
}
vec2 sphereUV(vec3 p){vec3 n=normalize(p+vec3(.0001));return vec2(atan(n.z,n.x)/6.283185+.5,asin(clamp(n.y,-1.,1.))/3.141593+.5);}
float soft(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}
float capsule(vec3 p,vec3 a,vec3 b,float r){vec3 q=p-a,ab=b-a;return length(q-ab*clamp(dot(q,ab)/dot(ab,ab),0.,1.))-r;}
bool refinedSurface(int composition){
    return composition==0 || composition==3 || composition==5;
}
// Explicit centres preserve whole spheres at the boundary of the cluster.
const vec4 NUCLEI[22]=vec4[22](
    vec4(0.302,0.919,0.000,0.185),
    vec4(-0.357,0.781,0.327,0.213),
    vec4(0.053,0.682,-0.599,0.156),
    vec4(0.450,0.632,0.586,0.189),
    vec4(-0.731,0.489,-0.129,0.211),
    vec4(0.715,0.427,-0.455,0.155),
    vec4(-0.229,0.330,0.854,0.192),
    vec4(-0.398,0.223,-0.766,0.209),
    vec4(0.904,0.146,0.330,0.154),
    vec4(-0.835,0.045,0.345,0.195),
    vec4(0.391,-0.046,-0.836,0.206),
    vec4(0.285,-0.145,0.910,0.153),
    vec4(-0.741,-0.221,-0.430,0.199),
    vec4(0.880,-0.337,-0.193,0.204),
    vec4(-0.477,-0.418,0.678,0.153),
    vec4(-0.097,-0.496,-0.746,0.202),
    vec4(0.566,-0.634,0.477,0.201),
    vec4(-0.590,-0.670,0.024,0.153),
    vec4(0.351,-0.799,-0.349,0.204),
    vec4(-0.014,-0.905,0.297,0.198),
    vec4(-0.320,0.210,0.180,0.160),
    vec4(0.270,-0.310,-0.270,0.182));
float nucleiField(vec3 p){
    float d=10.;
    for(int i=0;i<22;i++){
        vec4 nucleus=NUCLEI[i];
        d=min(d,length(p-nucleus.xyz)-nucleus.w);
    }
    return d-kickImpact*pulse*.026;
}
float sculptureRelief(vec3 p){
    if(reliefDepth<=0. || (hasImage==0 && hasCutout==0))return 0.;
    vec2 uv=sphereUV(p);
    vec2 edge=min(uv,1.-uv);
    // Flatten across the longitude seam and near the UV singularities.
    float coverage=smoothstep(0.,.09,edge.x)*smoothstep(.01,.14,edge.y);
    return reliefDepth*coverage*dot(heightArt(uv,4.5,true),vec3(.2126,.7152,.0722));
}
float shapeField(vec3 p,int composition){
    bool refined=refinedSurface(composition);
    float midTurn=refined ? .085*sin(midImpulse*.22) : midImpulse*.20;
    float t=travelTime*.45+midTurn*.65*pulse;
    vec3 q=p;
    q.xz=rot(t*.25)*q.xz;
    q.xy=rot(midTurn*pulse)*q.xy;
    float flex=refined ? .10*kickImpact+.012*bass : .10*bassHit+.04*bass;
    q*=1.-flex*pulse;
    vec2 uv=vec2(0.);
    vec3 luminance=vec3(.2126,.7152,.0722);
    float feed=0.;
    if(!refined){
        uv=sphereUV(q);
        feed=dot(heightArt(uv,3.5,true),luminance);
    }
    float d=0.;
    if(composition==0){
    d=length(q)-.78;
    for(int i=0;i<3;i++){float a=float(i)*2.094+t;vec3 centre=vec3(cos(a),sin(a*1.2)*.5,sin(a))*.62;
        d=soft(d,length(q-centre)-(.46+kickImpact*pulse*.12),.42);}
    }else if(composition==1){
    float g=abs(dot(sin(q*3.+t*.2),cos(q.yzx*3.-t*.1)))*.23-.09;
    d=max(length(q)-1.22,g);
    }else if(composition==2){
    d=length(q)-.58;
    for(int i=0;i<7;i++){float a=float(i)*.8976+t*.55;vec3 centre=vec3(cos(a),sin(a*2.+t)*.55,sin(a))*1.05;
        d=soft(d,length(q-centre)-(.2+.13*feed+.06*bassHit),.18);}
    }else if(composition==3){
    d=nucleiField(q);
    }else if(composition==4){
    q.x+=sin(q.y*2.+t)*.25;q.z+=cos(q.y*1.7-t*.7)*.18;
    d=capsule(q,vec3(0,-.9,0),vec3(0,.9,0),.43+.13*cos(q.y*4.-t));
    }else if(composition==5){
    vec3 ringPoint=q;
    ringPoint.xy=rot(.15)*ringPoint.xy;
    vec2 ring=vec2(length(ringPoint.xz)-.82,ringPoint.y);
    d=length(ring)-(.26+.06*cos(atan(ringPoint.z,ringPoint.x)*7.+t)+kickImpact*pulse*.05);
    }else if(composition==6){
    float poly=(dot(abs(q),vec3(1)) -1.85)*.57735;
    d=max(max(length(q)-1.14,poly),-(length(q)-.98));
    d=max(d,-length(q.xy-vec2(.3,.15))+.38);
    }else{
    d=10.;
    for(int i=0;i<6;i++){float a=float(i)*1.0472+t*.3;
        vec3 a0=vec3(cos(a),-.8,sin(a))*.8,b0=vec3(cos(a+1.8),.8,sin(a+1.8))*.8;
        d=soft(d,capsule(q,a0,b0,.16+.04*bassHit),.12);}
    }
    if(refined){
        float strength=composition==0 ? .085 : .055;
        return d-strength*sculptureRelief(q);
    }
    float bump=(feed-.3)*imageMix*(.03+.07*warp);
    float relief=0.;
    if(reliefDepth>0.)relief=reliefDepth*dot(reliefCutout(materialDomain(uv)).rgb,luminance);
    return d-bump-.12*relief;
}
float shape(vec3 p){
    p.y-=.38*structureHeight(p.xz*.34+.5);

    bool active=sourceComposition>=0 && sceneMorph<1.;
    int count=active && sceneMorph>0. ? 2 : 1;
    float field=0.;
    for(int i=0;i<count;i++){
        bool source=active && i==0;
        float weight=count==1 ? 1. : (source ? 1.-sceneMorph : sceneMorph);
        field+=weight*shapeField(p,source ? sourceComposition : targetComposition);
    }
    return field;
}
// Each composition keeps its camera response. During a join, interpolate the
// camera parameters and distance fields before tracing a single surface.
const vec4 CAMERA[8]=vec4[8](
    vec4(.49,.22,.98,.20), vec4(.83,.40,.98,.40),
    vec4(.91,.28,.79,.45), vec4(.62,.44,.62,.32),
    vec4(.68,.50,.89,.32), vec4(.62,.22,.98,0.),
    vec4(.55,.32,.94,.29), vec4(.76,.38,1.,.38));
const float LENS[8]=float[8](2.60,2.46,2.35,2.50,1.87,2.06,2.55,2.42);
const vec3 NATIVE_BLUE=vec3(.05,.35,.9);
const vec3 INK[8]=vec3[8](vec3(.18,.56,.46), NATIVE_BLUE, NATIVE_BLUE, vec3(.74,.40,.14),
    NATIVE_BLUE, vec3(.15,.29,.61), NATIVE_BLUE, NATIVE_BLUE);
const vec3 BLUE_LIGHT=vec3(.02,.65,1.);
const vec3 ACCENT[8]=vec3[8](vec3(.64,.84,.78), BLUE_LIGHT, BLUE_LIGHT, vec3(.92,.68,.34),
    BLUE_LIGHT, vec3(.58,.72,.96), BLUE_LIGHT, BLUE_LIGHT);

void main(){
    vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    int source=sourceComposition<0 ? targetComposition : sourceComposition;
    vec4 camera=mix(CAMERA[source],CAMERA[targetComposition],sceneMorph);
    float torus=mix(float(source==5),float(targetComposition==5),sceneMorph);
    float refined=mix(float(refinedSurface(source)),float(refinedSurface(targetComposition)),sceneMorph);
    float legacyAttack=clamp(max(bassHit*1.5,pow(1.-fract(projectBeat),3.)*bass*1.6),0.,1.);
    float impact=mix(legacyAttack,kickImpact,refined)*pulse;
    float drift=mix(midImpulse*1.7,.12*sin(midImpulse*.22),refined)*pulse;
    float orbit=travelTime*mix(.18,.22,torus)+mix(float(source),float(targetComposition),sceneMorph)*.34
        +camera.x*drift+.10*impact*refined;
    float roll=mix(sin(floor(projectBeat)*1.570796),.82*cos(travelTime*.31),refined);
    p=rot(camera.y*impact*roll)*p;
    float radius=3.05-.67*camera.z*impact;
    float eyeHeight=.35+sin(orbit*.7)*.4+camera.w*impact*cos(orbit*1.3);
    float torusHeight=radius*(.72+.08*impact)+.14*sin(orbit*.7);
    vec3 eye=vec3(sin(orbit)*radius,mix(eyeHeight,torusHeight,torus),cos(orbit)*radius);
    vec3 aim=vec3(camera.w*sin(travelTime*1.1)*impact,mix(.14*cos(travelTime*.8),-.10,torus)*impact,0.);
    float panorama=max(0.,resolution.x/resolution.y-1.777778);
    float lateral=panorama*1.15*sin(travelTime*.95+mix(float(source),float(targetComposition),sceneMorph)*.7)
        *clamp(impact*1.8,0.,1.)*(1.-torus);
    aim+=normalize(cross(-eye,vec3(0,1,0)))*lateral;
    vec3 f=normalize(aim-eye),r=normalize(cross(f,vec3(0,1,0))),u=cross(r,f);
    float nuclei=mix(float(source==3),float(targetComposition==3),sceneMorph);
    float lens=mix(LENS[source],LENS[targetComposition],sceneMorph)+mix(-.15,.10,torus)*impact
        -nuclei*(.20+.50*impact);
    vec3 ray=normalize(f*lens+r*p.x+u*p.y);
    float distance=0.,stepSize=0.,previousGap=0.;
    bool hit=false;
    vec3 q=eye;
    for(int i=0;i<72;i++){
        q=eye+ray*distance;
        float d=shape(q);
        if(d<.004){
            hit=true;
            // Estimate the zero crossing from the last two march samples.
            if(stepSize>0. && refined>0.){
                float slope=max(.14,(previousGap-d)/stepSize);
                q+=ray*refined*clamp(d/slope,-stepSize,.024);
            }
            break;
        }
        previousGap=d;
        stepSize=max(.003,d*.68);
        distance+=stepSize;
        if(distance>7.)break;
    }
    vec3 c=vec3(.003,.006,.018);
    if(hit){
        // Four signed samples estimate the gradient without expanding six
        // independent copies of the field in the compiler.
        vec3 gradient=vec3(0.);
        float normalRadius=mix(.004,.008,refined);
        for(int i=0;i<4;i++){
            vec3 direction=vec3((i&1)==0 ? 1. : -1., (i&2)==0 ? 1. : -1., i==0 || i==3 ? 1. : -1.);
            gradient+=direction*shape(q+normalRadius*direction);
        }
        vec3 n=normalize(gradient);
        vec3 nativeInk=mix(INK[source],INK[targetComposition],sceneMorph);
        vec3 accent=mix(ACCENT[source],ACCENT[targetComposition],sceneMorph);
        float pigment=mix(1.,imageMix,refined);
        vec3 ink=mix(nativeInk,art(sphereUV(q),nativeInk),pigment);
        float light=.35+.65*max(0.,dot(n,normalize(vec3(-.6,.8,1.))));
        float spec=pow(max(0.,dot(reflect(ray,n),normalize(vec3(-.6,.8,1.)))),40.);
        float rim=pow(1.-max(0.,dot(n,-ray)),3.);
        vec3 material=ink*.82+vec3(.035)+accent*.10;
        float shimmer=.5+.5*cos(24.*sphereUV(q).x+trebleImpulse*2.);
        c=material*light+ink*rim*shimmer*trebleHit*.35+art(sphereUV(reflect(ray,n)),vec3(0.))*.12*pigment+accent*(spec*.6+rim*.38);
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

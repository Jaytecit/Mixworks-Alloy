#version 330
#define CITY_KIND 1
// Owned city instruments. Expanded with CITY_KIND into the two scene folders.
// 0: lateral waterfront skyline; 1: forward street canyon. No shared camera pulse.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bass,mid,treble,bassHit,midHit,trebleHit;
uniform float bassTime,midTime,trebleTime,bassImpulse,midImpulse,trebleImpulse;
uniform float scale,warp,detail,hue,intensity,pulse,orbit,focusX,focusY;
uniform float colourR,colourG,colourB,imageMix;
out vec4 colour;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.13,311.71)))*43757.37);}
float box(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
vec3 ink(float phase){return (.12+.88*pow(.5+.5*cos(6.283185*(phase+hue+vec3(.02,.32,.61))),vec3(1.35)))*mix(vec3(.65),vec3(colourR,colourG,colourB)*1.6,.55);}
vec3 imageInk(vec2 uv,vec3 fallback){
    vec4 a=hasImage!=0?sampleMedia(uv):vec4(0),b=sampleCutout(uv);
    return mix(fallback,mix(a.rgb,b.rgb,b.a),max(a.a,b.a)*imageMix*.65);
}
// Return the lot identity and the local facade chart without moving its walls.
vec3 lot(vec3 p,out vec2 id){
#if CITY_KIND == 0
    id=floor(vec2(p.x/2.8,(p.z-4.)/2.6));
    return vec3((fract(p.x/2.8)-.5)*2.8,p.y,(fract((p.z-4.)/2.6)-.5)*2.6);
#else
    float side=p.x<0.?-1.:1.;
    float row=floor(max(0.,abs(p.x)-1.5)/2.25);
    id=vec2(side*(row+1.),floor(p.z/2.6));
    return vec3(p.x-side*(2.4+row*2.25),p.y,(fract(p.z/2.6)-.5)*2.6);
#endif
}
float city(vec3 p){
    p.y-=.70*structureHeight(fract(vec2(p.x*.13,p.z*.078)+.5));

    vec2 id;vec3 q=lot(p,id);float seed=hash(id);
#if CITY_KIND == 0
    float height=1.1+seed*5.4+step(.87,seed)*2.3;
    float width=.48+hash(id+9.)*.37;
    float building=box(q-vec3(0,height*.5,0),vec3(width,height*.5,.72));
    // A clear waterfront foreground and a bounded, readable skyline silhouette.
    building=max(building,max(4.-p.z,p.z-24.));
    return min(building,p.y);
#else
    float height=3.2+seed*6.2;
    float width=.65+hash(id+8.)*.2;
    float building=box(q-vec3(0,height*.5,0),vec3(width,height*.5,.92));
    float crown=box(q-vec3(0,height+.16,0),vec3(width*.66,.16,.70));
    return min(min(building,crown),p.y);
#endif
}
vec3 facade(vec3 q,vec3 n,float distance){
    vec2 id;vec3 local=lot(q,id);float seed=hash(id);
    float face=abs(n.x)>.5?local.z:local.x;
    float rows=step(.30,fract(q.y*3.8))*step(.32,fract(face*5.));
    float low=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse;
#if CITY_KIND == 0
    float district=.5+.5*sin(id.x*.9+id.y*1.7-bassImpulse*2.8);
    float occupied=step(.32,hash(floor(vec2(face*5.,q.y*3.8))+id*11.));
    vec3 light=ink(seed*.6+.18)*rows*occupied*(.24+low*district*1.25);
    float sweep=exp(-pow(fract(q.y*.12-midImpulse*.10)-.5,2.)*160.);
    light+=ink(.68)*sweep*mi*.22;
    float roof=pow(max(n.y,0.),8.)*(.015+.22*hi);
    vec3 body=vec3(.018,.026,.06)*(.35+.65*max(0.,dot(n,normalize(vec3(-.5,.8,-.4)))));
    return body+light+ink(.42)*roof;
#else
    vec3 body=vec3(.025,.018,.065)*(.3+.7*max(0.,dot(n,normalize(vec3(.6,.9,-.5)))));
    vec3 light=ink(seed*.7)*rows*(.18+.16*mid+.28*mi);
    float signPanel=step(abs(q.y-(1.9+seed*1.8)),.24)*step(abs(face),.62);
    vec2 chart=vec2(face*.7+.5,(q.y-1.9-seed*1.8)*1.5+.5);
    light+=imageInk(chart,ink(.6+seed*.2))*signPanel*(.65+.55*mi);
    light+=ink(.1)*exp(-abs(abs(local.x)-(.65+hash(id+8.)*.2))*90.)*hi*.24;
    return body+light;
#endif
}
void main(){
    vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    p-=vec2(focusX-.5,focusY-.5)*.65;p*=.78+scale*.7;
    vec3 eye,forward;
#if CITY_KIND == 0
    // Slow lateral dolly. Bass changes districts, never the horizon or roofs.
    eye=vec3(travelTime*.18,2.65,-5.5);
    forward=normalize(vec3(.10+.08*sin(travelTime*.025)*(orbit+.15),.035,1.));
#else
    // Camera clearance: |eye.x| <= .46; the nearest facade starts beyond 1.55.
    float steer=.23*sin(midImpulse*.7)+.12*sin(travelTime*.14);
    eye=vec3(steer,.85,travelTime*2.2+bassImpulse*pulse*.24);
    forward=normalize(vec3(.17*sin(midImpulse*.7)+.06*sin(travelTime*.14),.045,1.));
#endif
    vec3 right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*1.55+right*p.x+up*p.y);
    float distance=.01;vec3 q=eye;bool hit=false;
    for(int i=0;i<86;i++){
        q=eye+ray*distance;float d=city(q);
        if(d<.0035*(1.+distance*.04)){hit=true;break;}
        distance+=max(.004,d*.75);if(distance>55.)break;
    }
    vec3 sky=mix(vec3(.055,.018,.11),vec3(.003,.008,.025),smoothstep(-.12,.65,ray.y));
    sky+=ink(.68)*exp(-abs(ray.y+.025)*20.)*.055;
    vec3 c=sky;
    if(hit){
        float e=.006;vec3 n=normalize(vec3(city(q+vec3(e,0,0))-city(q-vec3(e,0,0)),city(q+vec3(0,e,0))-city(q-vec3(0,e,0)),city(q+vec3(0,0,e))-city(q-vec3(0,0,e))));
        if(q.y<.025){
#if CITY_KIND == 0
            float reflections=pow(.5+.5*sin(q.x*2.7+sin(q.z*5.+midTime*.4)*(.15+warp*.25)),7.);
            c=vec3(.006,.012,.035)+ink(q.x*.035)*reflections*(.06+.16*bassHit*pulse)*exp(-abs(q.z-3.)*.13);
            c+=ink(.45)*pow(.5+.5*sin(q.z*21.+trebleTime*.3),20.)*.025;
#else
            float lane=exp(-pow((abs(q.x)-.55)*42.,2.));
            float dash=step(.65,fract(q.z*.5));
            float traffic=pow(.5+.5*cos(q.z*2.1-trebleTime*10.-trebleImpulse*2.),20.);
            c=vec3(.012,.016,.028)+ink(.65)*lane*(.08+dash*.10+traffic*(.5+1.2*trebleHit*pulse));
            c+=ink(.16)*exp(-abs(q.x)*15.)*step(.78,fract(q.z*.38))*(.05+.45*bassHit*pulse);
#endif
        }else c=facade(q,n,distance);
        c=mix(c,sky,1.-exp(-distance*.022));
    }
    colour=vec4(clamp(c*(.7+intensity*.8),0.,1.),1.);
}

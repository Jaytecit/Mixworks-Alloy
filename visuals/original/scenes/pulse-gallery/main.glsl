#version 330
#define STAGE 0
// Owned image instruments. STAGE: 0 lightbox, 1 orbiting plane, 2 mechanical iris.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
uniform float bassTime,midTime,trebleTime,pulse,scale,warp,detail,hue,imageMix,intensity;
out vec4 colour;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 palette(float t){return .06+.84*pow(.5+.5*cos(vec3(0.,2.1,4.2)+t+hue*6.283185),vec3(1.25));}
vec3 picture(vec2 uv){
    vec3 base=palette(uv.x*2.+uv.y)*.07;
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));
    return mix(base,mix(a.rgb,b.rgb,b.a),max(a.a,b.a)*imageMix);
}
float rectangle(vec2 p,vec2 size){vec2 q=abs(p)-size;return length(max(q,0.))+min(max(q.x,q.y),0.);}
vec3 plane(vec2 p,vec3 centre,float yaw,float roll,vec2 size,vec3 background){
    vec3 eye=vec3(0,0,4.),ray=normalize(vec3(p,-2.8));
    vec3 u=vec3(cos(yaw),0,-sin(yaw)),v=vec3(0,1,0);
    vec3 x=u*cos(roll)+v*sin(roll),y=v*cos(roll)-u*sin(roll),n=cross(x,y);
    float divisor=dot(ray,n);if(abs(divisor)<.05)return background;
    float depth=dot(centre-eye,n)/divisor;if(depth<=0.)return background;
    vec3 hit=eye+ray*depth-centre;vec2 local=vec2(dot(hit,x),dot(hit,y));
    float d=rectangle(local,size),edge=1.-smoothstep(.0,.018,d);
    vec3 face=picture(local/(2.*size)+.5)*(.94+.06*intensity);
    float rim=exp(-abs(d)*75.);face+=palette(local.x*.4+.3)*rim*.28;
    float bevel=1.-smoothstep(-.07,-.015,d);face*=.82+.18*bevel;
    vec3 c=mix(background,face,edge);
    c+=palette(.3)*exp(-abs(d-.045)*75.)*(.06+.18*trebleHit*pulse);
    return c;
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,aspect=vec2(resolution.x/resolution.y,1.);
    vec2 p=(uv-.5)*2.*aspect;
    float low=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse,t=travelTime;
    vec3 c=vec3(.008,.012,.024);
#if STAGE == 0
    // One frontal lightbox; only its frame depth breathes with bass.
    vec2 size=vec2(1.35,.76)*(.87+scale*.30);
    float d=rectangle(p,size),outer=rectangle(p,size+vec2(.14+.04*low));
    float shadow=1.-smoothstep(.0,.12,rectangle(p-vec2(.05,-.06),size+.13));
    c*=1.-shadow*.6;
    float frame=(1.-smoothstep(.0,.012,outer))*smoothstep(-.015,.01,d);
    float bevel=.25+.75*smoothstep(.005,.09,abs(d));
    c=mix(c,palette(.45+p.x*.06)*bevel*(.35+.8*low),frame);
    if(d<0.)c=picture(p/(2.*size)+.5)*(.94+.06*intensity);
    float chase=pow(.5+.5*cos(atan(p.y,p.x)*2.-midImpulse*2.4-t*.4),10.);
    c+=palette(.8)*exp(-abs(d-.075)*90.)*(.10+.75*mi*chase);
    vec2 corner=abs(p)-size-vec2(.11);
    c+=palette(.2)*exp(-dot(corner,corner)*450.)*(.03+.7*hi);
    float plinth=1.-smoothstep(.025,.045,abs(p.y+size.y+.20));
    c+=palette(.4)*plinth*exp(-pow(abs(p.x)*.65,4.))*.055;
#elif STAGE == 1
    // The complete image is a moving perspective plane, not a fixed centre decal.
    float phase=t*.86+midImpulse*.45;
    float ring=abs(length(p/vec2(1.43,.64))-1.);
    c+=palette(.3)*exp(-ring*85.)*(.10+.14*midHit);
    vec3 centre=vec3(.65*sin(phase),.19*cos(phase*.8),.32*cos(phase));
    c=plane(p,centre,.56*sin(phase+.4),.13*sin(phase*.7),vec2(1.43,.86)*(.86+scale*.32),c);
    for(int i=0;i<3;i++){
        float a=phase+float(i)*2.094395+trebleImpulse*.17;
        vec2 position=vec2(1.43*cos(a),.64*sin(a));
        float r=.035+.012*float(i)+hi*.025,dist=length(p-position);
        vec3 light=palette(float(i)*1.8+.4);
        c+=light*exp(-dist*35.)*(.10+.22*hi);
        c=mix(c,light*(.35+.65*sqrt(max(0.,1.-dist*dist/(r*r)))),1.-smoothstep(r,r+.009,dist));
    }
#else
    // Hard shutter leaves reveal and occlude a source plane behind them.
    float r=length(p),angle=atan(p.y,p.x)+midImpulse*.20+t*.07;
    float wedge=6.283185/8.;
    float radius=.35+.055*sin(t*.6)+low*.34+.08*bassTime/(1.+bassTime);
    float local=mod(angle+wedge*.5,wedge)-wedge*.5;
    float aperture=radius/max(.8,cos(local));
    float hole=1.-smoothstep(aperture-.008,aperture+.008,r);
    float shell=1.-smoothstep(1.04,1.06,r);
    float id=floor((angle+wedge*.5)/wedge);
    float crease=abs(mod(angle+(r-radius)*.75+wedge*.5,wedge)-wedge*.5);
    vec3 blade=palette(.35+id*.14)*(.16+.36*smoothstep(-wedge*.5,wedge*.5,local));
    blade+=palette(.8)*exp(-crease*100.)*(.13+.35*mi);
    blade+=vec3(.02,.04,.07)*pow(max(0.,1.-abs(local)*2.),6.);
    c=mix(c,blade,shell);
    c=mix(c,picture(p/vec2(2.5,1.6)+.5),hole);
    c+=palette(.1)*exp(-abs(r-1.015)*100.)*(.18+.4*hi);
    c+=palette(.7)*exp(-abs(r-aperture)*130.)*.12;
    for(int i=0;i<8;i++){
        float a=float(i)*wedge;vec2 screw=vec2(cos(a),sin(a))*.965;
        c+=palette(.6)*exp(-dot(p-screw,p-screw)*750.)*(.07+.23*hi);
    }
#endif
    colour=vec4(clamp(c,0.,1.),1.);
}

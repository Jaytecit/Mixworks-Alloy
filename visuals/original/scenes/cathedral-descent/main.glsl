// Mixworks Cathedral Descent. Authored architectural raymarch, 2026-11-02.
// Repeated vaulted bays, stained light, polished stone and a distant rose window.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
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

float clock;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float centreAt(float z){return (.12+.72*mid)*sin(z*.22-midTime*1.25);}
vec3 architecture(vec3 p){
    p.x-=centreAt(p.z);
    // A travelling vault deformation, with the floor anchored in world space.
    p.y=2.6+(p.y-2.6)/(1.+.20*bass*sin(p.z*.48-bassTime*4.));
    return p;
}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 cold(){return mix(vec3(.012,.38,.58),vec3(.13,.025,.62),hue*.65);}
vec3 warm(){return vec3(.95,.26,.035);}
vec2 scene(vec3 p){
    p.y-=.48*structureHeight(fract(vec2(p.x*.14,p.z*.19)+.5));

    float floorDistance=p.y;
    p=architecture(p);
    float z=mod(p.z+2.4,4.8)-2.4;
    float arc=length(vec2(p.x,max(p.y-2.6,0.)));
    float rib=length(vec2(arc-2.85,z))-.17;
    // Nested vault fillets and low parapets add scale at every bay.
    rib=min(rib,length(vec2(arc-3.16,z))-.075);
    float rail=length(vec2(abs(p.x)-2.9,p.y-.34))-.09;
    float d=min(rib,rail);float id=1.;
    float wall=3.48-arc;
    if(wall<d){d=wall;id=2.;}
    if(floorDistance<d){d=floorDistance;id=3.;}
    return vec2(d,id);
}
vec2 march(vec3 ro,vec3 rd){
    float t=.03;float id=0.;
    for(int i=0;i<88;i++){
        vec2 s=scene(ro+rd*t);
        if(s.x<max(.001,t*.0006)){id=s.y;break;}
        t+=max(.001,s.x*.65);
        if(t>54.)break;
    }
    return vec2(min(t,54.),id);
}
vec3 normalAt(vec3 p){
    const vec2 e=vec2(.003,0);
    return normalize(vec3(scene(p+e.xyy).x-scene(p-e.xyy).x,scene(p+e.yxy).x-scene(p-e.yxy).x,scene(p+e.yyx).x-scene(p-e.yyx).x));
}
vec3 windowImage(vec2 p){
    // p is measured in wall metres, so picture proportions survive perspective.
    float ar=resolution.x/resolution.y;
    float ratioA=ar*mediaAspect.y/mediaAspect.x;
    float ratioB=ar*nextMediaAspect.y/nextMediaAspect.x;
    vec2 a=p/vec2(2.2,2.2/ratioA)+.5;
    vec2 b=p/vec2(2.2,2.2/ratioB)+.5;
    a=1.-abs(mod(a,2.)-1.);b=1.-abs(mod(b,2.)-1.);
    return mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend);
}
vec3 farLight(vec3 ro,vec3 rd){
    float t=32./max(.01,rd.z);
    vec3 portal=ro+rd*t;
    vec2 p=portal.xy-vec2(centreAt(portal.z),2.9);
    float r=length(p),a=atan(p.y,p.x);
    float rings=exp(-abs(r-2.2)*14.)+exp(-abs(r-1.8)*20.)*.5;
    float petals=pow(.5+.5*cos(a*12.-trebleTime*4.),12.)*(1.-smoothstep(1.2,2.2,r))*smoothstep(.3,1.4,r);
    vec3 c=cold()*(.025+exp(-r*r*.12)*.12)+warm()*rings*.8;
    c+=mix(cold(),warm(),.5+.5*sin(a*6.))*petals*.55;
    vec4 logo=artwork(p/3.+.5);
    c=mix(c,linearRGB(logo.rgb)*(.8+mid*.5),logo.a*imageMix);
    return c;
}
vec3 surface(vec3 p,vec3 rd,float id,float distance){
    vec3 n=normalAt(p);
    p=architecture(p);
    float bay=floor((p.z+2.4)/4.8);
    float local=mod(p.z,4.8)-2.4;
    float phase=.5+.5*sin(bay*.72-clock*.8+midTime*.3);
    vec3 accent=mix(cold(),warm(),smoothstep(.28,.85,phase));
    vec3 lamp=normalize(vec3(-p.x,3.8-p.y,1.5));
    float diffuse=max(0.,dot(n,lamp));
    float fresnel=pow(1.-max(0.,dot(n,-rd)),3.);
    float spec=pow(max(0.,dot(reflect(-lamp,n),-rd)),40.);
    vec3 c;
    if(id<1.5){
        float edge=pow(1.-abs(n.z),5.);
        float flutes=.5+.5*cos(atan(p.x,max(.01,p.y-2.6))*70.);
        c=vec3(.012,.007,.004)*(.35+diffuse)+accent*(.025+edge*.65)*( .7+flutes*.3);
        c+=warm()*spec*.8+cold()*fresnel*.22;
        c+=accent*pow(.5+.5*sin(p.y*2.-clock*2.+bay),12.)*(.04+bassHit*pulse*.16);
    }else{
        float opening=(1.-smoothstep(1.06,1.18,abs(local)))*smoothstep(.7,.82,p.y)*(1.-smoothstep(4.5,4.8,p.y));
        vec2 leading=turn(.55*sin(trebleTime*1.4+bay*.2))*vec2(local,p.y-2.7);
        float leadX=abs(sin(leading.x*8.5)),leadY=abs(sin(leading.y*5.));
        float lead=smoothstep(.05,.13,min(leadX,leadY));
        vec3 glass=mix(cold(),warm(),.5+.5*sin(local*2.+p.y*1.8+bay*.8));
        glass*=.65+.35*sin(p.y*3.+sin(local*4.));
        if(hasImage!=0)glass=mix(glass,nestedMaterial(vec2(local,p.y-2.7)*.3+.5,windowImage(vec2(local,p.y-2.7)),1.),imageMix*.9);
        c=vec3(.002,.004,.011)*(.5+diffuse)+accent*fresnel*.18;
        c+=glass*opening*(.04+lead*.96)*(.95+mid*.18);
        float frame=exp(-abs(abs(local)-1.2)*70.)*smoothstep(.65,.8,p.y)*(1.-smoothstep(4.65,4.85,p.y));
        c+=warm()*frame*.65;
        float seams=pow(.5+.5*cos(p.y*12.),18.)*pow(.5+.5*cos(local*8.),3.);
        c+=cold()*seams*.035*detail;
        float theta=atan(p.x,max(.01,p.y-2.6));
        float vault=pow(.5+.5*cos(theta*18.),34.);
        float crossrib=pow(.5+.5*cos(theta*12.+sin(p.z*1.3)*1.2),32.);
        c+=mix(cold(),warm(),phase)*(vault*.12+crossrib*.035)*smoothstep(3.9,4.8,p.y)*( .6+detail*.5);
    }
    float haze=1.-exp(-distance*.016);
    return mix(c,vec3(.001,.003,.012)+cold()*.009,haze);
}
vec3 upper(vec3 ro,vec3 rd){
    vec2 hit=march(ro,rd);
    if(hit.y==0.)return farLight(ro,rd);
    return surface(ro+rd*hit.x,rd,hit.y,hit.x);
}
void main(){
    clock=travelTime*.35+bassTime*5.2+bassImpulse*5.5;
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    vec3 ro=vec3(centreAt(clock*1.15)+.16*sin(midImpulse*.4)*orbit,1.45+.12*sin(midTime*.7),clock*1.15);
    vec3 target=vec3(centreAt(ro.z+6.)+(focusX-.5)*.45,ro.y+.25+(focusY-.5)*.35,ro.z+6.);
    uv=turn(.045*sin(midImpulse*.3)+.025*sin(trebleImpulse*.3))*uv;
    vec3 f=normalize(target-ro),r=normalize(cross(f,vec3(0,1,0))),u=cross(r,f);
    vec3 rd=normalize(f*(1.4+scale*.38)-r*uv.x+u*uv.y);
    vec2 hit=march(ro,rd);vec3 c;
#ifdef GEOMETRY_PROBE
    // Test-only depth output: brightness/material reactions cannot pass this.
    colour=vec4(vec3(exp(-hit.x*.06)),1);return;
#endif
    if(hit.y==0.)c=farLight(ro,rd);
    else if(hit.y>2.5){
        vec3 p=ro+rd*hit.x;
        // The reflected architecture is traced in the same space as its piers.
        vec3 n=normalize(vec3(.012*warp*sin(p.z*3.+clock),1.,.009*warp*cos(p.x*6.-clock)));
        vec3 reflection=upper(p+vec3(0,.012,0),reflect(rd,n));
        float fresnel=.30+.55*pow(1.-max(0.,dot(n,-rd)),4.);
        c=reflection*fresnel;
        float aa=max(.008,hit.x/resolution.y*1.3);
        float longitudinal=abs(abs(p.x)-1.65);
        float joint=min(abs(mod(p.z+.6,1.2)-.6),abs(mod(p.x+.6,1.2)-.6));
        c+=cold()*.018+warm()*(1.-smoothstep(.012,.012+aa,longitudinal))*(.6+bass*.3);
        c*=.8+.2*smoothstep(0.,aa,joint);
        float wave=exp(-pow((mod(p.z-clock*2.4,12.)-6.)*1.8,2.));
        c+=cold()*wave*.045*(.3+bassHit);
    }else c=surface(ro+rd*hit.x,rd,hit.y,hit.x);
    // Sparse airborne motes with bounded coverage, moving independently of travel.
    vec2 cell=floor(uv*65.);vec2 v=fract(uv*65.)-.5;
    float seed=fract(sin(dot(cell,vec2(127.1,311.7)))*43758.5);
    float mote=exp(-dot(v,v)*120.)*step(.993,seed)*(.5+.5*sin(clock+seed*140.));
    c+=vec3(.25,.5,.7)*mote*(.15+trebleHit*.6)*detail;
    c*=.9+.36*intensity;
    c*=vec3(.88)+vec3(colourR,colourG,colourB)*.18;
    colour=vec4(max(c,vec3(0)),1);
}

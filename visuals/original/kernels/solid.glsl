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

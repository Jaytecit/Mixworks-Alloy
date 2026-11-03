float relief(vec2 p,float t){
#if FAMILY == 6
    vec2 centre=.4*vec2(sin(midTime*1.7),cos(midTime*1.3));
    p=centre+rot(.65*midSwing()*exp(-length(p-centre)*.45))*(p-centre);
    p+=vec2(.45*lowSwing()*sin(p.y*1.7),.3*bass*sin(p.x*2.-bassTime*6.));
    p+=.055*treble*vec2(sin(p.y*12.-trebleTime*13.),cos(p.x*11.+trebleTime*9.));
#elif FAMILY == 7
    p=rot(.55*midSwing())*p;
    p+=vec2(.36*lowSwing(),.16*highSwing());
    t+=bassTime*3.+trebleTime*2.;
#else
    p.x+=.22*lowSwing()*sin(p.y*2.4);
    p.y+=.25*midSwing()*cos(p.x*1.5);
    p=rot(.12*highSwing())*p;
    t+=midTime*4.;
#endif
    p+=warp*.6*vec2(sin(p.y*2.+t*.3),cos(p.x*2.3-t*.2));
#if FAMILY == 6
    if(VARIANT==0)return .5+.24*sin(p.x*2.+fbm(vec3(p*.7,t*.13))*7.+t*.2);
    if(VARIANT==1)return .5+.28*sin(length(p)*4.-t*.4+sin(atan(p.y,p.x)*3.)*.5);
    if(VARIANT==2)return fbm(vec3(p*2.,t*.12))+.12*sin(p.x*9.+p.y*2.+t);
    if(VARIANT==3)return .5+.18*sin(p.x*4.+t)*cos(p.y*4.-t*.6)+.08*sin(length(p)*9.-t);
    return .5+.2*sin(p.y*4.+sin(p.x*1.6+t*.15)*3.+t*.3);
#elif FAMILY == 7
    float a=atan(p.y,p.x),r=length(p);
    if(VARIANT==1)return .5+.25*sin(r*16.-t*2.)+.1*cos(p.x*10.+t);
    if(VARIANT==2)return .5+.24*sin(p.x*6.+sin(p.y*4.+t)*2.);
    if(VARIANT==3)return .5+.2*sin(p.x*9.+t)*sin(p.y*7.-t*.6);
    return .5+sin(a*(3.+float(VARIANT%7))+r*(9.+detail*12.)-t)*.25+sin(r*5.+t*.3)*.15;
#else
    vec2 cell=floor(p*3.),f=fract(p*3.);float d=8.,d2=8.;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){vec2 g=vec2(x,y),o=vec2(hash(cell+g),hash(cell+g+4.));o=.5+.38*sin(t*.2+o*6.283);float v=length(g+o-f);if(v<d){d2=d;d=v;}else d2=min(d2,v);}
    if(VARIANT==1)return .55+.2*cos(d*12.);
    if(VARIANT==2)return .25+.5*smoothstep(.01,.15,d2-d);
    if(VARIANT==3)return .3+sqrt(d)*.5+.04*sin(d*55.);
    return .7*sqrt(d)+sin(d*35.-t)*.035;
#endif
}
void main(){vec2 p=domain();float t=clockTime();float h=relief(p,t);float e=.008;vec3 n=normalize(vec3(relief(p-vec2(e,0),t)-relief(p+vec2(e,0),t),relief(p-vec2(0,e),t)-relief(p+vec2(0,e),t),e*(1.8-bass*.7)));
    vec3 c=material(vec3(p,h)*1.8,n,h*3.+float(VARIANT));float line=pow(.5+.5*cos(h*(24.+detail*22.)+t*.3),18.);
    c+=palette(h+.4)*line*.22;c*=.6+.7*h;finish(c,p);}

void main(){vec2 p=domain();float t=clockTime();vec3 c=vec3(0);
#if FAMILY == 12
    // Escape-time Julia / Mandelbrot fields with orbit-trap relief.
    p=rot(.4*midSwing())*p+vec2(.16*lowSwing(),.1*highSwing());
    vec2 z=p*(.75+warp*.5),k=vec2(-.72+.12*sin(t*.11+midTime),.18+.1*cos(t*.13+trebleTime));if(VARIANT%2==0){k=z+vec2(-.4,0);z=vec2(0);}
    float count=0.,trap=9.;for(int i=0;i<64;i++){z=vec2(z.x*z.x-z.y*z.y,2.*z.x*z.y)+k;trap=min(trap,abs(length(z)-.7));if(dot(z,z)>40.)break;count+=1.;}
    c=palette(count*.023+hue*.2)*(.25+exp(-trap*8.));c*=.25+.75*(1.-smoothstep(40.,64.,count));
#else
    // Repeated folding, orbit-trap threads and angular mandala.
    p=rot(.32*midSwing())*p;vec2 q=p;float glow=0.;
    if(VARIANT==1)q=vec2(atan(p.y,p.x),log(length(p)+.2));
    if(VARIANT==2)q=rot(t*.1)*p/(dot(p,p)+.5);
    if(VARIANT==3)q=vec2(length(p),atan(p.y,p.x)*.7);
    if(VARIANT==4)q=sin(p*2.);
    if(VARIANT==5)q=abs(fract(p*.5+t*.02)*2.-1.);
    for(int i=0;i<7;i++){q=abs(q)-vec2(.55+.13*lowSwing(),.34+.1*midSwing());q=rot(.55+float(VARIANT%5)*.25+warp*.2+.10*highSwing()*sin(float(i)+trebleTime*3.))*q;q*=1.35;float d=abs(length(q)-.3-bass*.05);if(VARIANT%3==1)d=abs(max(abs(q.x),abs(q.y))-.35);if(VARIANT%3==2)d=abs(q.x*q.y)*.5;glow+=exp(-d*(30.+detail*60.))/float(i+1);}
    c=palette(length(q)*.04+length(p)*.2)*glow*(.55+mid*.6);c+=palette(.7)*pow(max(0.,cos(atan(p.y,p.x)*(6.+float(VARIANT%5)*2.)+t*.2)),32.)*exp(-abs(length(p)-.7)*12.)*.4;
#endif
    c=media(p*.3+.5,c);finish(c,p);}

vec3 opticalInk(vec2 uv){vec2 p=uv-.5;float ribs=sin(length(p)*18.-bassTime*5.+sin(atan(p.y,p.x)*3.+midTime*2.));return palette(ribs*.2+p.x*.2)*(.3+.7*smoothstep(-.65,.3,ribs));}
void main(){vec2 p=domain();float t=clockTime();float cells=2.+float(VARIANT%4)+floor(detail*2.);
    vec2 q=rot(float(VARIANT%3)*.4)*p*cells;
    q.x+=.55*lowSwing()*sin(floor(q.y)*1.7);q.y+=.4*midSwing()*cos(floor(q.x)*1.3);
    vec2 id=floor(q),f=fract(q)-.5;float seed=hash(id);
    f=rot(.7*midSwing()*sin(seed*12.)+.5*highSwing()+.13*trebleHit)*f;
    float r=length(f),beat=.5+.5*sin(projectBeat*3.141593+seed*6.283)*bass;float line=0.;
#if FAMILY == 14
    // Graphic assemblage with independent cell shapes and cutouts.
    float shape=mod(floor(seed*9.)+floor(mode*3.99)+float(VARIANT),4.);
    if(shape<1.)line=1.-smoothstep(.2,.22,r);else if(shape<2.)line=1.-smoothstep(.19,.21,max(abs(f.x),abs(f.y)));else if(shape<3.)line=1.-smoothstep(.04,.06,abs(r-(.18+.1*beat)));else line=1.-smoothstep(.03,.05,min(abs(f.x),abs(f.y)));
    if(VARIANT==0){vec3 img=media(p*.5+.5,palette(seed)*.15);vec4 sprite=cutoutArt(f/(.65+.2*beat)+.5);if(hasCutout!=0)img=mix(img,sprite.rgb,sprite.a*imageMix);float mask=1.-smoothstep(.4,.48,max(abs(f.x),abs(f.y)));finish(img*mask,p);return;}
    if(VARIANT==1)line=1.-smoothstep(.02,.035,abs(abs(f.x)+abs(f.y)-(.3+.1*beat)));
    if(VARIANT==2)line=1.-smoothstep(.05,.07,min(abs(f.x),abs(f.y)));
    if(VARIANT==3)line=step(.0,sin(f.x*20.+t))*step(.0,sin(f.y*20.-t));
    if(VARIANT==4)line=step(f.y,.3*sin(f.x*8.+t+seed*4.));
    if(VARIANT==5)line=1.-smoothstep(.02,.04,abs(length(f+vec2(.5,.5))-.6));
    if(VARIANT==6)line=smoothstep(.2,.24,abs(f.x))*smoothstep(.2,.24,abs(f.y));
#elif FAMILY == 15
    // Scanline discontinuities, channel separation, block masking.
    float row=floor(p.y*(18.+detail*40.));vec2 uv=p*.5+.5;
    uv.x+=sin(row*13.+floor(projectBeat*2.))*(.015+.13*treble)*warp;
    uv+=vec2(.12*lowSwing()*sin(row),.10*midSwing());
    if(VARIANT==1)uv=abs(fract(uv*3.+vec2(t*.02,0))*2.-1.);
    if(VARIANT==2)uv.x=floor(uv.x*16.)/16.;
    if(VARIANT==3)uv.y+=hash(vec2(floor(uv.x*24.),1))*.3*sin(t);
    vec3 src=media(uv,palette(uv.y+sin(uv.x*8.+t)*.2));
    if(VARIANT==4)src=floor(src*4.)/4.;
    if(VARIANT==5)src=abs(src-media(uv+vec2(.012,0),palette(uv.x)))*2.;
    float scan=.5+.5*sin(p.y*resolution.y+trebleTime*12.);src*=.8+.2*scan;finish(src,p);return;
#elif FAMILY == 16
    // Image lens / mirrored tiles / rippled photograph, independent media mode.
    vec2 optical=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    optical=rot(.09*midSwing())*optical;
    p=optical;
    vec2 uv=optical*.5+.5;float m=floor(mode*3.99);
    if(VARIANT==0){uv=vec2(atan(p.y,p.x)/6.283+.5,length(p)*.3);uv.y+=(.06*treble+.025*trebleHit)*sin(atan(p.y,p.x)*8.-trebleTime*7.);}
    else if(VARIANT==1)uv=abs(fract(uv*3.)*2.-1.);
    else if(VARIANT==2)uv=uv;
    else if(VARIANT==3)uv+=warp*(.01+.025*bass)*vec2(sin(p.y*9.-bassTime*7.),cos(p.x*8.-trebleTime*9.));
    else if(VARIANT==4)uv=.5+(uv-.5)*(1.+warp*length(p)*length(p));
    else uv+=vec2(.04*lowSwing(),.025*midSwing());
    uv+=vec2(.025*lowSwing(),.012*highSwing());
    vec3 base=media(uv,opticalInk(uv));
    if(VARIANT==2){vec4 sprite=cutoutArt((rot(.2*midSwing())*p-vec2(.28*lowSwing(),.15*highSwing()))/1.6+.5);base=mix(base,sprite.rgb,sprite.a*imageMix);}
    if(VARIANT==5){base=vec3(0);vec2 direction=vec2(cos(midTime*2.+midSwing()),sin(midTime*2.+midSwing()));for(int j=-4;j<=4;j++){vec2 tap=uv+direction*float(j)*(.003+.04*treble+.012*trebleHit);base+=media(tap,opticalInk(tap))/9.;}}
    finish(base,p);return;
#else
    line=exp(-abs(r-.2-.06*sin(t+seed*8.))*70.);
#endif
    vec3 c=mix(palette(seed)*.025,media((id+f)/cells*.3+.5,palette(seed+beat*.08)),line);finish(c,p);}

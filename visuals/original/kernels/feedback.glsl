// Bounded temporal field. This is a new simulation, not reference simulation code.
uniform float deltaTime;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec2 p=domain();float t=clockTime();vec2 px=1./resolution;float style=float(VARIANT%5);vec2 flow=vec2(sin(p.y*(2.+style*.7)+t*.2),cos(p.x*(2.5+style*.4)-t*.17));
    if(VARIANT%3==1)flow=vec2(-p.y,p.x)/(length(p)+.4);if(VARIANT%3==2)flow=rot(t*.1)*vec2(sin(p.y*5.),.5);
    // Alternating jets, snare shear and high-frequency eddies move the history.
    flow=rot(1.2*midSwing())*flow*(.4+1.8*bass);
    flow+=vec2(1.8*lowSwing()*cos(p.y*2.),1.4*midSwing()*sin(p.x*2.));
    flow+=.7*treble*vec2(sin(p.y*9.-trebleTime*8.),cos(p.x*8.+trebleTime*7.));
    float stepSize=clamp(deltaTime*60.,0.,3.);
    vec2 at=uv+flow*(.001+warp*.003)*stepSize;vec3 prev=historyValid!=0?texture(historyImage,clamp(at,0.,1.)).rgb:vec3(0);
    vec3 c;
#if FAMILY == 17
    // Reaction-like diffusion: local Laplacian and bounded nonlinear growth.
    vec3 lap=texture(historyImage,clamp(at+vec2(px.x,0),0.,1.)).rgb+texture(historyImage,clamp(at-vec2(px.x,0),0.,1.)).rgb+texture(historyImage,clamp(at+vec2(0,px.y),0.,1.)).rgb+texture(historyImage,clamp(at-vec2(0,px.y),0.,1.)).rgb-4.*prev;
    vec2 source=p+vec2(.28*lowSwing(),.24*midSwing());
    float n=fbm(vec3(source*2.,t*.13+trebleTime));
    float field;
    if(VARIANT==0)field=.5+.5*sin(source.x*9.+n*9.+source.y*2.);
    else if(VARIANT==1)field=pow(.5+.5*cos(source.x*7.+n*4.)*cos(source.y*7.-n*4.),2.);
    else if(VARIANT==2)field=smoothstep(.32,.67,fbm(vec3(source*3.,midTime*.7)));
    else if(VARIANT==3)field=.5+.5*sin(source.y*12.+sin(source.x*3.+midTime)*3.+n*3.);
    else if(VARIANT==4)field=pow(abs(sin(source.x*7.+n*6.)*sin(source.y*6.-n*6.)),.45);
    else if(VARIANT==5){vec2 tile=fract(source*4.+n*.3)-.5;field=exp(-dot(tile,tile)*22.);}
    else if(VARIANT==6)field=.5+.5*sin(length(source)*13.+atan(source.y,source.x)*3.-bassTime*4.+n*4.);
    else field=pow(.5+.5*sin(source.x*14.+sin(source.y*7.+n*3.)*3.),5.);
    vec3 seed=palette(field*.45+float(VARIANT)*.13)*(.025+.65*field);
    c=clamp(prev+lap*min(.20,.08*stepSize)+(seed-prev)*min(.16,.07*stepSize),0.,1.6);
    if(historyValid==0)c=seed;
#else
    p=rot(.55*midSwing())*p+vec2(.25*lowSwing(),.18*highSwing());
    float ribbon=exp(-abs(p.y-.5*sin(p.x*(2.+style)+t*.7+bassTime*3.)-.15*sin(p.x*11.-trebleTime*7.))*(18.+detail*24.));
    if(VARIANT%3==1)ribbon=exp(-abs(length(p)-.7-.18*sin(atan(p.y,p.x)*(3.+style)+t))*30.);
    if(VARIANT%3==2)ribbon=pow(fbm(vec3(p*(3.+style),t*.2)),8.)*5.;
    vec3 injection=palette(p.x*.15+t*.03)*ribbon*(.1+mid*.4+beatPulse*.2*pulse);
    c=prev*pow(.85+feedback*.12,stepSize)+injection*stepSize;
    if(historyValid==0)c=injection;
#endif
    c=mix(c,media(uv+flow*.03,c),hasImage!=0?.025:0.);colour=vec4(c,1);
}

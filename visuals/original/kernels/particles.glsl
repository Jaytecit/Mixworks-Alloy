// Musical positions are shared by lights, strands and substantial focal artworks.
vec2 node(float id,float t){
    float a=id*2.399+t*.13+midTime*.7;vec2 centre;
#if FAMILY == 10
    centre=vec2(sin(id*1.73+t*.23),cos(id*.93-t*.17))*(.4+hash(vec2(id,8))*2.);
    if(VARIANT==1)centre=vec2(cos(a),sin(a))*(.2+id*.025);
    if(VARIANT==2)centre=vec2(sin(id*.618+t*.3)*2.,sin(id+t)*.4);
    if(VARIANT==3)centre=vec2(sin(id*1.7+t*.2),cos(id*.9+t*.3))*(.6+.2*sin(midTime));
    if(VARIANT==4)centre=vec2(id/12.-2.,sin(id*.4+t*.6)*.6);
    centre+=vec2(.38*lowSwing()*cos(id*.8),.44*midSwing()*sin(id*.7));
    centre+=.16*treble*vec2(cos(id+trebleTime*5.),sin(id-trebleTime*7.));
#elif FAMILY == 11
    centre=vec2(cos(a+.5*midSwing()),sin(a+.5*midSwing()))*(.3+mod(id,8.)*.18+.25*lowSwing());
    if(VARIANT==0)centre=vec2(id/12.-2.,sin(id*.4+midTime*2.)*(.1+bass*.7)+.22*lowSwing()*cos(id));
    centre+=(.4*highSwing()+.18*trebleHit)*vec2(sin(id),cos(id));
#else
    centre=(vec2(hash(vec2(id,1)),hash(vec2(id,2)))-.5)*5.;
    centre=rot(.25*midSwing()+t*.03)*centre;
    centre+=vec2(.35*lowSwing(),.22*highSwing())*sin(id);
#endif
    float dep=.5+.5*sin(id*1.71+t*.06+bassTime*.8);
    return centre/(.7+dep*2.4);
}
void main(){vec2 p=domain();float t=clockTime();vec3 c=vec3(.002,.004,.012);
    if(hasImage!=0)c=mix(c,imagePlane(p*.5+.5)*.32,imageMix);
#if FAMILY == 9
    for(int z=0;z<7;z++){
        float depth=1.+float(z)*.7;vec2 q=rot(t*.02+float(z)*.13+.2*midSwing())*p*depth;
        q+=vec2(.4*lowSwing(),.25*highSwing())*depth;
        float cloud=pow(fbm(vec3(q*1.5,float(z)*2.+t*.07+midTime)),4.);
        c+=palette(float(z)*.11+length(q)*.06)*cloud*.5/depth;
    }
#endif
    for(int i=0;i<48;i++){
        float id=float(i);vec2 centre=node(id,t);float r=length(p-centre);
        float size=.009+detail*.018;float bright=.35+hash(vec2(id,6))*.6;
        c+=palette(id*.031)*bright*(exp(-r*r/(size*size))+.008/(.005+r*r*90.));
#if FAMILY == 10
        vec2 next=node(mod(id+1.,48.),t),v=next-centre;
        float d=length(p-centre-v*clamp(dot(p-centre,v)/max(dot(v,v),.001),0.,1.));
        c+=palette(id*.03)*exp(-d*200.)*.09;
#endif
    }
#if FAMILY == 10 || FAMILY == 11
    // Three large orbital artworks instead of 48 repeated tiny stickers.
    for(int j=0;j<3;j++){
        float id=float(j);vec2 centre=node(id*13.+3.,t)*.55;
        vec2 local=rot(.25*midSwing()+id*2.094)*(p-centre)/(.62+detail*.38);
        vec4 art=cutoutArt(local+.5);
        c=mix(c,art.rgb*(.72+.28*cos(local.y*1.4)),art.a*imageMix);
    }
#endif
    finish(c,p);
}

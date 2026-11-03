#version 330
#define FAMILY 12
#define VARIANT 3
// Original layered relief compositions. Recipe inserts FAMILY and VARIANT.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bassTime,midTime,trebleTime,bassImpulse,midImpulse,trebleImpulse;
uniform float bassHit,midHit,trebleHit,bass,mid,treble;
uniform float scale,detail,imageMix,hue,intensity,pulse,warp;
out vec4 colour;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float band(float d,float width){return 1.-smoothstep(width,width+.035,abs(d));}
vec3 tint(float x){return .055+.82*pow(.5+.5*cos(6.283185*(vec3(.02,.32,.61)+hue+x)),vec3(1.6));}
// Height, coverage, local material coordinate. Each branch has its own topology.
vec3 relief(vec2 p,float layer){
    float v=float(VARIANT),t=travelTime*.17,lo=bassImpulse*pulse*2.4,mi=midImpulse*pulse*2.1,hi=trebleImpulse*pulse*1.8;
    float h=0.,mask=0.,id=0.;
    p*=.85+scale*.65;
    // Music changes the geometry: bass opens depth, mids shear the structures,
    // treble sends a travelling bend. Family variation avoids a universal zoom.
    p/=1.+bassHit*pulse*(.16+.04*mod(float(FAMILY),3.));
    p.x+=midHit*pulse*.22*sin(p.y*(1.5+mod(float(FAMILY),4.))+mi);
    p.y+=trebleHit*pulse*.12*sin(p.x*3.-hi);
    p+=vec2(.13*layer,-.1*layer);
#if FAMILY == 0 // woven strata: straight, bowed, fan, basket
    if(VARIANT==1)p.y+=.35*sin(p.x*2.-lo*.6);
    if(VARIANT==2)p=vec2(atan(p.y,p.x)*1.2,length(p)*1.4);
    if(VARIANT==3)p=turn(.78)*p+vec2(.2*sin(p.y*3.+mi),0);
    float a=p.y*4.+t+lo*.32,b=p.x*3.-t*.7-mi*.28;
    float wa=band(fract(a)-.5,.29),wb=band(fract(b)-.5,.23);
    float over=step(.5,mod(floor(a)+floor(b),2.));
    h=wa*(.28+.12*cos(a*6.283))+wb*(.18+.15*over);mask=max(wa,wb);id=floor(a)*.09+floor(b)*.13;
#elif FAMILY == 1 // accordion: terraces, radial fan, zigzag, divided sails
    vec2 q=p;if(VARIANT==1)q=vec2(atan(p.y,p.x)*1.3,length(p));
    if(VARIANT==2)q.x+=abs(fract(p.y*1.7)-.5)*.7;
    if(VARIANT==3)q.x+=sign(p.y)*.45;
    float f=q.x*(3.+v*.6)+t*.5+mi*.45;
    h=(1.-abs(fract(f)*2.-1.))*(.35+.13*sin(q.y*2.+lo));
    mask=band(fract(q.y*(VARIANT==3?1.8:.6))-.5,.43);id=floor(f)*.13;
#elif FAMILY == 2 // lanterns: staggered orchard, suspended chains, bouquet, crowns
    vec2 q=p;
    if(VARIANT==2)q=vec2(atan(p.y,p.x)*1.2,length(p)*1.5);
    if(VARIANT==3)q.y+=.3*cos(p.x*3.+lo*.5);
    q*=vec2(VARIANT==1?3.:2.1,VARIANT==1?1.25:2.1);
    q.y-=t*.45+lo*.18;q.x+=.22*sin(floor(q.y)*2.+mi*.6);
    vec2 cell=floor(q),f=fract(q)-.5;id=hash(cell);
    float r=length(f*vec2(1.,VARIANT==1?.65:1.));
    mask=1.-smoothstep(.35,.39,r);h=sqrt(max(0.,.15-r*r))*(.9+.25*sin(mi+id*6.));
    float stem=band(f.x,.015)*step(f.y,0.);mask=max(mask,stem*.65);
#elif FAMILY == 3 // shutters: rotating panels, iris, chevrons, counter-sliding gates
    vec2 q=p;if(VARIANT==1)q=vec2(atan(p.y,p.x),length(p));
    if(VARIANT==2)q=turn(.785)*q;
    q*=vec2(2.4,VARIANT==3?1.4:2.4);vec2 cell=floor(q),f=fract(q)-.5;id=hash(cell);
    float a=mi*.65+id*2.;
    if(VARIANT==3)f.x+=.22*sin(a+lo*.5);else f=turn(a)*f;
    float width=.2+.12*sin(lo*.65+id*5.);
    mask=band(f.x,width)*band(f.y,.43);h=mask*(.2+.18*(f.x+.5));
#elif FAMILY == 4 // porcelain: scallops, shell fans, crossed breakers, nested bowls
    vec2 q=p;
    if(VARIANT==1)q=vec2(atan(p.y,p.x)*.9,length(p));
    if(VARIANT==2)q=turn(.7)*q;
    float f=q.y*5.+sin(q.x*(VARIANT==3?5.:2.7))*1.3-t-lo*.5;
    if(VARIANT==3)f=length(q)*7.+sin(atan(q.y,q.x)*6.)*.5-t-lo*.4;
    float ridge=.5+.5*cos(f*3.14159);h=pow(ridge,2.)*.42;
    mask=.8+.2*ridge;id=floor(f)*.08;
#elif FAMILY == 5 // contour terraces: islands, fault line, basin, river valley
    vec2 q=p;float terrain=sin(q.x*2.+t)*cos(q.y*2.3-mi*.3);
    if(VARIANT==1)terrain+=abs(q.x)*.8+step(0.,q.x)*.4;
    if(VARIANT==2)terrain=length(q)*.75+.2*sin(atan(q.y,q.x)*5.+mi*.4);
    if(VARIANT==3)terrain=abs(q.y+.35*sin(q.x*2.+t))+.2*sin(q.x*4.);
    terrain+=lo*.08;float stepper=terrain*5.;
    h=(floor(stepper)+smoothstep(.75,.95,fract(stepper)))*.075;
    mask=.98;id=floor(stepper)*.11;
#elif FAMILY == 6 // rotors: disc, twin gears, orbital rings, eccentric petals
    vec2 q=p;
    if(VARIANT==1)q.x=abs(q.x)-.85;
    if(VARIANT==2)q=turn(.32)*q*vec2(.72,1.3);
    if(VARIANT==3)q-=vec2(.3*cos(t),.18*sin(t));
    float r=length(q),a=atan(q.y,q.x)+mi*.4+t*.2;
    float sectors=VARIANT==3?5.:8.+v*2.;
    float rr=r+.06*sin(a*sectors+lo*.6);
    mask=band(fract(rr*2.2-t*.2)-.5,.3)*band(fract(a/6.283*sectors)-.5,.4);
    h=mask*(.23+.17*sin(a*sectors)+.08*cos(rr*12.-lo));id=floor(rr*2.2)*.18+a*.04;
#elif FAMILY == 7 // switchyard: parallel junctions, meanders, overpass, radial interchange
    vec2 q=p;
    if(VARIANT==2)q=turn(.55)*q;
    if(VARIANT==3)q=vec2(atan(p.y,p.x)*1.2,length(p));
    float road=q.y*3.+sin(q.x*(1.4+v*.3)-t)*(.7+v*.15)-lo*.15;
    float d=fract(road)-.5;mask=band(d,.28);h=mask*(.24+.1*cos(d*8.));
    if(VARIANT==0 || VARIANT==2){float cross=band(fract(q.x*2.+t*.4+mi*.2)-.5,.12);h=max(h,cross*.42);mask=max(mask,cross);}
    id=floor(road)*.15+q.x*.08;
#elif FAMILY == 8 // flotilla: diamond channels, offset barges, radial fleet, zigzag sails
    vec2 q=p;if(VARIANT==2)q=vec2(atan(p.y,p.x)*1.2,length(p)*1.3);
    q*=vec2(2.,1.8);q.x-=t+lo*.3;
    q.y+=.25*sin(floor(q.x)*1.7+mi*.4);
    vec2 cell=floor(q),f=fract(q)-.5;id=hash(cell);
    if(VARIANT==3)f=turn(sign(sin(id*10.))*.6)*f;
    float d=VARIANT==1?max(abs(f.x)*.8,abs(f.y)*1.4):abs(f.x)+abs(f.y)*(.6+id*.6);
    mask=1.-smoothstep(.39,.43,d);h=mask*(.42-d*.6);
#elif FAMILY == 9 // optics: lenticular wall, spherical instruments, prisms, focus ripples
    vec2 q=p*vec2(VARIANT==0?4.:2.,VARIANT==0?1.:2.);
    q+=vec2(t*.25+mi*.18,lo*.12);vec2 cell=floor(q),f=fract(q)-.5;id=hash(cell);
    float d=VARIANT==0?abs(f.x):length(f);
    if(VARIANT==2)d=max(abs(f.x)*.866+f.y*.5,-f.y);
    if(VARIANT==3)d=abs(fract(length(p)*1.7-t*.12-mi*.08)-.5)*.86;
    mask=1.-smoothstep(.4,.46,d);h=sqrt(max(0.,.2-d*d))*.8;
#elif FAMILY == 10 // coral: branching fans, fern beds, river roots, radial anemone
    vec2 q=p;if(VARIANT==3)q=vec2(atan(p.y,p.x),length(p));
    if(VARIANT==1)q.x+=floor(q.y*1.2)*.5;
    if(VARIANT==2)q=q.yx;
    q.x+=.12*sin(q.y*2.+t+lo*.3);
    float root=abs(fract(q.x*1.4)-.5),branch=abs(sin(q.y*7.+mi*.3))* .38;
    float d=min(root,abs(root-branch));
    mask=1.-smoothstep(.035,.075,d);h=mask*(.24+.12*cos(q.y*3.-t));id=floor(q.x*1.4)*.15+q.y*.09;
#elif FAMILY == 11 // rain pavilion: curtains, waterfall steps, diagonal rain, converging falls
    vec2 q=p;if(VARIANT==2)q=turn(.5)*q;
    if(VARIANT==3)q.x*=1.+max(-.7,q.y)*.35;
    q.x+=.08*sin(q.y*3.+mi*.5);
    float lane=floor(q.x*7.),f=fract(q.x*7.)-.5;
    float drop=fract(q.y*(VARIANT==1?3.:1.3)+t*(.8+hash(vec2(lane))*.5)+lo*.35);
    mask=band(f,.3)*smoothstep(0.,.16,drop)*(1.-smoothstep(.7,1.,drop));
    h=mask*(.3+.1*cos(f*7.));id=lane*.12;
#elif FAMILY == 12 // paper theatre: scalloped layers, mountain wings, cloud stage, revolving fans
    vec2 q=p;if(VARIANT==3)q=vec2(atan(p.y,p.x)*1.2,length(p));
    float edge=VARIANT==1?abs(sin(q.x*2.+t)):.5+.5*sin(q.x*(VARIANT==2?4.:2.)+t+mi*.2);
    float tier=q.y*2.5+edge*.9+lo*.17;
    float f=fract(tier);mask=smoothstep(.03,.1,f);h=(floor(tier)*.04+.3*exp(-f*3.))*mask;id=floor(tier)*.13;
#else // mosaic kiln: staggered bricks, rhombus relief, hex-like scales, spinning stamps
    vec2 q=p;if(VARIANT==1)q=turn(.785)*q;
    q*=vec2(3.,VARIANT==2?3.5:2.4);q.x+=mod(floor(q.y),2.)*.5;
    q.y+=t*.3+lo*.12;vec2 cell=floor(q),f=fract(q)-.5;id=hash(cell);
    if(VARIANT==3)f=turn(mi*.4+id*2.)*f;
    float d=VARIANT==2?max(abs(f.x)*.866+abs(f.y)*.5,abs(f.y)):max(abs(f.x),abs(f.y));
    mask=1.-smoothstep(.38,.44,d);h=mask*(.2+.14*sin(id*8.+mi*.6)+.12*cos(d*6.));
#endif
    return vec3(h,mask,id);
}
vec3 imageMaterial(vec2 uv,float id){
    vec3 base=tint(id*.2);
    if(hasImage==0)return base;
#if FAMILY == 13
    // One complete, contained image per tile; no nested fragments.
    vec4 wholeArt=sampleFittedMedia(uv);
    vec4 cut=sampleCutout(uv);
    return mix(base,mix(wholeArt.rgb,cut.rgb,cut.a),max(wholeArt.a,cut.a)*min(1.,imageMix*1.5));
#endif
    uv=1.-abs(mod(uv,2.)-1.);
    vec3 art=texture(userImage,uv).rgb;
    vec2 q=uv*3.,cell=floor(q),p=fract(q)-.5;
    vec2 child=turn(midImpulse*.65+midTime*.18+id)*p;
    float edge=max(abs(child.x),abs(child.y));
    vec3 nested=texture(userImage,child*.85+.5).rgb;
    vec2 grand=turn(-trebleImpulse*1.1-trebleTime*.2)*(child-vec2(.12,-.08))*2.6;
    nested=mix(nested,texture(userImage,grand+.5).rgb,(1.-smoothstep(.19,.25,length(grand)))*.8);
    art=mix(art,nested,(1.-smoothstep(.3,.36,edge))*.65);
    vec4 cut=sampleCutout(uv);art=mix(art,cut.rgb,cut.a*.7);
    return mix(base,art,imageMix*.72);
}
void main(){
    vec2 p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 bg=tint(.42)*(.13+.18*(1.-uv.y));
    float back=.5+.5*sin(p.x*7.+sin(p.y*5.-travelTime*.2)+midTime*.18);
    bg+=tint(.7)*pow(back,10.)*.055;
    vec3 c=bg;
    for(int layer=1;layer>=0;layer--){
        float l=float(layer);vec2 q=p*(1.+l*.25)+vec2(l*.2,-l*.08);
        vec3 s=relief(q,l);float eps=.004;
        vec2 grad=vec2(relief(q+vec2(eps,0),l).x-relief(q-vec2(eps,0),l).x,relief(q+vec2(0,eps),l).x-relief(q-vec2(0,eps),l).x)/(2.*eps);
        vec3 n=normalize(vec3(-grad*.6,1.));
        float light=.28+.72*max(0.,dot(n,normalize(vec3(-.45,.7,1.))));
        float spec=pow(max(0.,dot(n,normalize(vec3(-.4,.5,1.)))),26.);
        vec2 material=q*.27+.5+grad*.014*warp+vec2(bassImpulse*.018,midImpulse*.013);
#if FAMILY == 5 || FAMILY == 12
        // Each terrace carries its own image chart and directional shadow.
        // This breaks up a flat screen-wide photograph into physical layers.
        material=turn(s.z*.8)*(material-.5)*1.35+.5+vec2(s.z*.8,s.z*1.3);
        // Keep broad shaded faces readable; retain directional layer edges.
        light=max(.48,light)*(.82+.18*smoothstep(.015,.18,abs(s.x-relief(q+vec2(.04,.08),l).x)));
#elif FAMILY == 13
        // Reconstruct the same local tile chart used by relief(), including its
        // musical transform, so imagery stays attached to its moving carrier.
        vec2 tile=q*(.85+scale*.65);
        tile/=1.+bassHit*pulse*(.16+.04*mod(float(FAMILY),3.));
        tile.x+=midHit*pulse*.22*sin(tile.y*(1.5+mod(float(FAMILY),4.))+midImpulse*pulse*2.1);
        tile.y+=trebleHit*pulse*.12*sin(tile.x*3.-trebleImpulse*pulse*1.8);
        tile+=vec2(.13*l,-.1*l);
        if(VARIANT==1)tile=turn(.785)*tile;
        tile*=vec2(3.,VARIANT==2?3.5:2.4);tile.x+=mod(floor(tile.y),2.)*.5;
        tile.y+=travelTime*.17*.3+bassImpulse*pulse*2.4*.12;
        tile=fract(tile)-.5;
        if(VARIANT==3)tile=turn(midImpulse*pulse*2.1*.4+s.z*2.)*tile;
        material=tile/.76+.5;
#elif FAMILY == 2 || FAMILY == 3 || FAMILY == 8 || FAMILY == 9
        material=turn(s.z*1.7)*(material-.5)+.5+vec2(s.z*.6,-s.z*.4);
#endif
        vec3 body=imageMaterial(material,s.z)*light+tint(s.z+.5)*spec*(.13+.35*intensity);
        float rim=pow(clamp(length(grad)*.12,0.,1.),2.);
        body+=tint(s.z+trebleImpulse*.03)*rim*(.12+.65*trebleHit*pulse);
        body*=1.-l*.6;
        c=mix(c,body,clamp(s.y*(1.-l*.4),0.,1.));
    }
    c*=.86+.14*(1.-smoothstep(.3,1.3,length(p)*.55));
    colour=vec4(clamp(c,0.,1.),1.);
}

#version 330
#define FAMILY 4
// Original Alloy settings for image-fragment scenes. FAMILY is supplied per scene.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float sceneTime,bass,mid,treble,bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
uniform float bassTime,midTime,trebleTime,mode,hue,mirror,symmetry,intensity;
uniform sampler2D handoffAppearance;
uniform float generationBlend;
uniform float projectBeat;
out vec4 colour;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
vec3 tint(float x){return .12+.88*pow(.5+.5*cos(6.28318*(vec3(.03,.36,.68)+hue+x)),vec3(1.7));}
float line(float d,float width){return exp(-abs(d)/width);}
vec3 setting(vec2 p){
    float v=floor(mode*3.+.5),b=bassImpulse,m=midImpulse,h=trebleImpulse;
    float t=sceneTime*.08+bassTime*.16;
    vec3 c=vec3(.009,.014,.03);float form=0.,accent=0.;
#if FAMILY == 0
    // Receding broken mineral terraces; sparks peel upward from their edges.
    for(int j=0;j<5;j++){float z=float(j)+1.;vec2 q=p*(1.+z*.18);float ridge=-.7+z*.18+.12*sin(q.x*(2.+v)+z+t+m*.2);float d=q.y-ridge;
    float slab=smoothstep(-.19,-.14,d)*(1.-smoothstep(-.01,.015,d));
    c+=tint(z*.09)*(slab*(.1+.04*z)+exp(-abs(d)*12.)*(.06+z*.014));accent+=line(d,.008)*( .35+.35*sin(q.x*7.-b*3.+z));}
    form=pow(noise(p*4.+vec2(0,-t-h*.4)),6.)*.7;
#elif FAMILY == 1
    // Magnetic chamber: curved field lines span two animated electrode towers.
    vec2 q=turn(.15*sin(m*.35))*p;float poles=abs(abs(q.x)-(.85+.06*sin(b)));
    form=(line(poles,.075)+.2*(1.-smoothstep(.07,.12,poles)))* (1.-smoothstep(.3,1.2,abs(q.y)));
    float a=atan(q.y,q.x-.85)-atan(q.y,q.x+.85);
    accent=pow(.5+.5*cos(a*(6.+v*2.)-m*2.),28.)*exp(-dot(q,q)*.4);
#elif FAMILY == 2
    // Two flowing plasma vortices in a luminous vapour bed.
    for(int j=0;j<2;j++){vec2 q=p-vec2(j==0?-.65:.65,.2*sin(float(j)+m*.3));float r=length(q),a=atan(q.y,q.x);
    float f=sin(a*(3.+v)+r*10.-t*2.-b*(j==0?1.:-1.));form+=exp(-r*1.5)*(.5+.5*f)*.3;accent+=line(f,.07)*exp(-r*2.)*.35;}
#elif FAMILY == 3
    // Woven light canopy: staggered ribbons with depth and travelling stitches.
    for(int j=0;j<7;j++){float z=float(j);float ribbon=p.y-(z-3.)*.25-.12*sin(p.x*(1.5+v*.4)+z+m*.6);
    c+=tint(z*.08)*line(ribbon,.09)*.28;accent+=line(ribbon,.009)*(.3+.7*pow(.5+.5*sin(p.x*8.-b*4.+z),6.));}
#elif FAMILY == 4
    // Cellular circuit floor; pulses travel between illuminated junctions.
    vec2 q=turn(.3+v*.3)*p*(3.+v*.4);q+=vec2(m*.12,b*.08);vec2 cell=floor(q),f=fract(q)-.5;
    float path=min(abs(f.x),abs(f.y));form=line(path,.022)*.3;
    accent=exp(-dot(f,f)*100.)*(.3+.7*pow(.5+.5*sin(dot(cell,vec2(1.7,2.3))-h*3.),4.));
#elif FAMILY == 5
    // Tilted orbital instruments above a shaded planetary horizon.
    vec2 q=turn(.35+v*.3+m*.08)*p;float r=length(q*vec2(.7,1.6));
    for(int j=0;j<4;j++)accent+=line(r-(.5+float(j)*.22+.05*sin(b+float(j))),.008)*.28;
    float planet=length(p-vec2(.1,-1.65));form=exp(-abs(planet-1.1)*16.)*.6;
    c+=tint(.7)*(1.-smoothstep(1.05,1.12,planet))*(.08+.12*noise(p*5.+vec2(t,0)));
#elif FAMILY == 6
    // Layered silk curtains; separate strands carry upward ember trails.
    vec2 q=p;float waves=q.x*(8.+v*2.)+sin(q.y*2.+m)*1.4;
    form=pow(.5+.5*sin(waves),5.)*(.12+.18*noise(q*2.));
    accent=pow(.5+.5*sin(waves),24.)*pow(.5+.5*sin(q.y*5.-b*3.+waves*.2),6.);
#elif FAMILY == 7
    // Mechanical rotor deck: independently advancing toothed concentric rings.
    float r=length(p),a=atan(p.y,p.x);float shell=floor(r*5.);float tooth=pow(.5+.5*cos(a*(12.+v*4.)+m*(mod(shell,2.)*2.-1.)),10.);
    form=line(fract(r*5.)-.5,.18)*(.16+.12*tooth);accent=line(fract(r*5.)-.5,.035)*tooth*.65;
#else
    // Star-forming clouds and a foreshortened accretion sheet with comet trails.
    vec2 q=turn(.2+v*.4)*p;float r=length(q*vec2(.65,2.2));
    float cloud=noise(p*2.+vec2(t,m*.1))*.6+noise(p*5.-vec2(h*.1,t))*.3;
    form=pow(cloud,3.)*.8;accent=line(r-(.7+.05*sin(b)),.06)*(.5+.5*sin(atan(q.y,q.x)*6.-m*2.));
#endif
    c+=tint(.15)*form*1.65+tint(.55)*accent*(.26+.6*bassHit+.3*midHit);
    // Depth-separated moving motes, spatially local treble glints, never a full-screen flash.
    vec2 stars=p*vec2(35,24)+vec2(t*.3,-trebleTime*.3-h*.24);vec2 id=floor(stars),spark=fract(stars)-.5;
    float mote=exp(-dot(spark,spark)*95.)*step(.983,hash(id+float(FAMILY)*13.));
    c+=tint(hash(id))*mote*(.12+.6*trebleHit);
    return c;
}
void main(){
    vec2 aspect=vec2(resolution.x/resolution.y,1.);vec2 p=(gl_FragCoord.xy/resolution-.5)*2.*aspect;
    vec2 q=p;if(mirror>.5)q.x=abs(q.x);
    float sectors=floor(symmetry*6.+.5);
    if(sectors>0.){float n=sectors+2.,a=atan(q.y,q.x),r=length(q);a=abs(mod(a+3.141593/n,6.283185/n)-3.141593/n);q=vec2(cos(a),sin(a))*r*.72;}
    vec2 uv=q/(2.*aspect)+.5;uv=1.-abs(mod(uv,2.)-1.);
    vec4 fragments=texture(input0,uv);
    if(sectors>0.){
        // Fold every source sector into the view. Sampling one wedge can lose
        // an entire travelling group when its centre crosses another sector.
        vec4 gathered=vec4(0);float n=sectors+2.;
        for(int j=0;j<8;j++)if(float(j)<n){
            vec2 a=turn(float(j)*6.283185/n)*q;
            vec2 b=turn(float(j)*6.283185/n)*vec2(q.x,-q.y);
            vec2 auv=1.-abs(mod(a/(2.*aspect)+.5,2.)-1.);
            vec2 buv=1.-abs(mod(b/(2.*aspect)+.5,2.)-1.);
            gathered+=texture(input0,auv)+texture(input0,buv);
        }
        fragments=vec4(gathered.rgb/max(1.,gathered.a),min(1.,gathered.a));
    }else if(mirror>.5){
        vec4 other=texture(input0,vec2(1.-uv.x,uv.y));fragments+=other*(1.-fragments.a);
    }
    float passage=sin(mod(projectBeat-12.,32.)*.1963495408);
    float recall=exp(-passage*passage*28.);
    fragments=mix(fragments,texture(input0,uv),recall);
    vec3 bg=setting(q);
    vec2 px=1./resolution;
    vec3 halo=(texture(input0,uv+px*3.).rgb+texture(input0,uv-px*3.).rgb+texture(input0,uv+vec2(px.x,-px.y)*3.).rgb+texture(input0,uv-vec2(px.x,-px.y)*3.).rgb)*.25;
    vec3 c=fragments.rgb+bg*(1.-fragments.a*.85)+halo*(.10+.22*trebleHit);
    c=c/(1.+c*.35);
    if(generationBlend<1.){
        vec2 screenUV=gl_FragCoord.xy/resolution;
        float region=mod(float(FAMILY),3.)<1.?length(p)/(length(aspect)+.001):mod(float(FAMILY),3.)<2.?screenUV.y:screenUV.x;
        float takeover=smoothstep(region-.12,region+.12,generationBlend*1.24-.12);
        c=mix(texture(handoffAppearance,screenUV).rgb,c,takeover);
    }
    colour=vec4(c,1.);
}

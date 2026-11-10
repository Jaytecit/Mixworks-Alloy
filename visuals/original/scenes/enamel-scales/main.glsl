#version 330
#define GLASS_KIND 1
// One continuous source behind distinct glass carriers. GLASS_KIND 0/1.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
uniform float bassTime,midTime,trebleTime,pulse,scale,warp,detail,hue,imageMix,intensity;
out vec4 colour;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec3 palette(float phase){return .06+.82*pow(.5+.5*cos(6.283185*(vec3(.02,.32,.61)+hue+phase)),vec3(1.3));}
vec3 picture(vec2 uv){
    vec3 base=palette(.35)*(.055+.035*(1.-uv.y));
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));
    return mix(base,mix(a.rgb,b.rgb,b.a),max(a.a,b.a)*min(1.,imageMix*1.5));
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float low=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse,t=travelTime;
#if GLASS_KIND == 1
    // View the entire scale array and its one continuous source at an oblique angle.
    // Plane aspect matches the renderer, so perspective does not stretch the image.
    vec3 eye=vec3(.22*sin(t*.8)*low,.13*cos(t*.7)*low,4.3-1.45*low),ray=normalize(vec3(p,-2.8));
    vec3 u=vec3(cos(.42+.16*sin(t*.9+midImpulse)*mi),0,-sin(.42+.16*sin(t*.9+midImpulse)*mi)),v=vec3(0,1,0),n=cross(u,v);
    float divisor=dot(ray,n);
    if(abs(divisor)<.05){colour=vec4(.005,.008,.018,1.);return;}
    vec3 hit=eye+ray*(dot(-eye,n)/divisor);
    p=vec2(dot(hit,u),dot(hit,v));
    vec2 size=vec2(2.9*max(1.,resolution.x/resolution.y/1.777778),2.9/min(1.777778,resolution.x/resolution.y));
    if(any(greaterThan(abs(p),size))){colour=vec4(.005,.008,.018,1.);return;}
    uv=p/(2.*size)+.5;
#endif
    vec3 c=picture(uv);
#if GLASS_KIND == 0
    // Large staggered diamond panes glide over a stationary continuous image.
    float density=1.1+scale*.8;
    vec2 q=turn(.785398)*(p+vec2(t*.38+midImpulse*.12,0))*density;
    q.x+=mod(floor(q.y),2.)*.5;
    vec2 id=floor(q),f=fract(q)-.5;
    float edge=.43-max(abs(f.x),abs(f.y));
    edge+=.24*structureHeight(uv);

    float mask=smoothstep(-.008,.008,edge),rim=1.-smoothstep(.005,.045,edge);
    float seed=hash(id);
    vec2 tilt=turn(-.785398)*(vec2(.16*sin(seed*9.),.11*cos(seed*7.))*(.6+low*.8)+f*.18);
    vec2 bend=tilt*(.025+.04*warp);
    vec3 glass=picture(clamp(uv+bend,0.,1.))*(.90+.08*(f.x+f.y));
    glass+=palette(seed*.18)*(.035+rim*.13);
    glass+=vec3(.35,.55,.65)*pow(max(0.,.65+tilt.x-tilt.y),25.)*(.03+.3*hi);
    c*=1.-mask*.09;
    c=mix(c,glass,mask*.88);
    c+=palette(.5+seed*.1)*rim*mask*(.055+.20*hi);
#else
    // Oblique staggered rows of curved scales, overlapping and hinged at the top.
    float density=.95+scale*.55;
    vec2 q=vec2(p.x*1.18+p.y*.42,p.y*1.82)*density;
    vec2 base=floor(q);
    for(int row=-1;row<=1;row++){
        float y=base.y+float(row),offset=mod(y,2.)*.5;
        for(int column=-1;column<=1;column++){
            vec2 id=vec2(floor(q.x-offset)+float(column),y);
            vec2 centre=vec2(id.x+offset+.5,y+.5),f=q-centre;
            float seed=hash(id),wave=sin(midImpulse*2.2-y*.9+id.x*.35+t*.6);
            float opening=.12+low*.66+(.10+.20*mi)*(.5+.5*wave);
            vec2 local=f;local.y=.48+(f.y-.48)/cos(opening);
            float shape=max(length(vec2(local.x/.64,(local.y-.20)/.95))-1.,(local.y-.48)*1.5);
            shape-=.26*structureHeight(uv);

            float mask=1.-smoothstep(-.01,.015,shape),rim=exp(-abs(shape)*36.);
            float bulge=sqrt(max(0.,1.-local.x*local.x/(.68*.68)));
            vec3 normal=normalize(vec3(local.x*.40,sin(opening)*.65+(local.y-.05)*.20,cos(opening)));
            vec2 bend=normal.xy*(.028+.045*warp);
            vec3 glass=picture(clamp(uv+bend,0.,1.))*(.91+.06*bulge);
            float sheen=pow(max(0.,dot(normal,normalize(vec3(-.4,.6,1.)))),42.);
            glass+=palette(seed*.12)*(.030+rim*.11)+vec3(.22,.45,.55)*sheen*(.025+.18*hi);
            float shade=1.-smoothstep(.025,.085,shape);
            c*=1.-shade*.09;
            c=mix(c,glass,mask*.85);
            c+=palette(.38+seed*.12)*rim*mask*(.055+.09*hi);
        }
    }
#endif
    colour=vec4(clamp(c*(.94+.06*intensity),0.,1.),1.);
}

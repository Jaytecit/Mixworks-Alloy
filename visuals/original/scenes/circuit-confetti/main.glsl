#version 330
#define SIGNAL_KIND 3
// Expanded only into the five revised signal scenes. Broken Spectrum is preserved.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse,warp,detail,imageMix,pulse;
out vec4 colour;
float signalHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec3 imageAt(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 b=sampleCutout(materialDomain(uv));
    if(hasImage!=0||hasCutout!=0)return mix(a.rgb,b.rgb,b.a);
    return vec3(.15+.7*uv.x,.15+.7*uv.y,.35+.3*sin(uv.x*14.));
}
vec3 remembered(vec2 uv){return texture(input1,clamp(uv,0.,1.)).rgb;}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 cell=floor(uv*vec2(12,7));
    vec4 region=texelFetch(input0,ivec2(clamp(cell,vec2(0),vec2(11,6))),0);
    vec3 c=vec3(0);
#if SIGNAL_KIND == 0
    // Broad interleaved ribbons carry opposite vertical time streams.
    float column=floor(uv.x*12.),side=mod(column,2.)*2.-1.;
    vec2 q=uv;
    q.y=fract(uv.y+side*(travelTime*.09+midImpulse*.055*pulse));
    q.x+=side*bassHit*pulse*(.008+.025*warp);
    c=mix(imageAt(q),remembered(q),side>0.?.90:.38);
    float edge=1.-smoothstep(.015,.07,min(fract(uv.x*12.),1.-fract(uv.x*12.)));
    c*=.78+.22*sin(fract(uv.x*12.)*3.141593);
    c+=edge*(.04+trebleHit*pulse*.45)*vec3(.12,.65,1.);
#elif SIGNAL_KIND == 1
    // A single rolling acquisition bar reveals the fresh scan above old capture.
    float front=fract(travelTime*.31),distance=abs(uv.y-front);
    float beam=exp(-distance*distance*18000.);
    vec2 q=uv+vec2(sin(uv.y*22.)*midHit*pulse*.025,0);
    c=remembered(q);
    float wake=1.-smoothstep(.015,.11,mod(front-uv.y+1.,1.));
    c=mix(c,imageAt(q+vec2(bassHit*pulse*.018,0)),wake*.65);
    c*=.73+.12*cos(uv.y*(140.+detail*220.));
    c+=beam*(.22+bassHit*pulse*.55)*vec3(.1,1.,.7);
    c+=wake*trebleHit*pulse*.10*vec3(.35,.7,1.);
#elif SIGNAL_KIND == 2
    // Actual independently held captures; seams expose the fixed mosaic topology.
    vec2 f=fract(uv*vec2(12,7));
    float edge=smoothstep(.015,.045,min(min(f.x,f.y),min(1.-f.x,1.-f.y)));
    vec2 q=uv+vec2(region.x*.12,region.y*.08);
    c=remembered(q)*edge;
    float seed=signalHash(cell);
    float badge=step(.89,f.x)*step(.86,f.y);
    c+=badge*vec3(.08,.23,.3)*( .4+trebleHit*pulse);
#elif SIGNAL_KIND == 3
    // Detached fragments drift across empty space; each samples its own source patch.
    vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
    for(int i=0;i<24;i++){
        float fi=float(i);vec2 id=vec2(fi,fi*.73);
        float seed=signalHash(id),seed2=signalHash(id+9.);
        float enabled=step(.24,seed);
        float t=travelTime*(.12+seed*.13)+midImpulse*pulse*(.025+seed*.04);
        vec2 centre=vec2(sin(t+seed*6.283),cos(t*(.63+seed*.4)+seed2*6.283))*vec2(1.25,.68);
        float angle=seed*6.283+travelTime*(seed2-.5)*.45+midHit*pulse*.25;
        vec2 d=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*(p-centre);
        vec2 size=vec2(.11+seed*.10,.055+seed2*.07)*(1.+bassHit*pulse*.55);
        vec2 local=d/size;
        float mask=(1.-smoothstep(.86,1.,max(abs(local.x),abs(local.y))))*enabled;
        vec2 source=vec2(.15+seed*.70,.15+seed2*.70)+local*vec2(.12,.10);
        vec3 fragment=mix(imageAt(source),remembered(source),.30);
        float rim=smoothstep(.72,.95,max(abs(local.x),abs(local.y)));
        fragment+=rim*(.06+trebleHit*pulse*.4)*vec3(.3,.65,1.);
        c=mix(c,fragment,mask);
    }
#elif SIGNAL_KIND == 4
    // Broad diagonal strips slide along their cut, leaving readable dark joins.
    float coordinate=(uv.x+uv.y)*5.,strip=floor(coordinate),f=fract(coordinate);
    float side=mod(strip,2.)*2.-1.;
    float slide=side*(.065*sin(travelTime*.65+strip*.78)+midImpulse*pulse*.035);
    vec2 q=uv+vec2(1.,-1.)*slide;
    q+=vec2(1.)*side*bassHit*pulse*(.008+.018*warp);
    c=mix(imageAt(q),remembered(q),side>0.?.70:.12);
    c*=smoothstep(.02,.065,min(f,1.-f));
    float fringe=(1.-smoothstep(.05,.13,min(f,1.-f)))*trebleHit*pulse;
    c+=fringe*vec3(.55,.08,.9);
#endif
    colour=vec4(clamp(c,0.,1.),1.);
}

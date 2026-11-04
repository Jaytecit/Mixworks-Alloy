#version 330
// Owned source-neighbour flock. RG position, BA velocity; pigment is separate.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float simulationReset,simulationTime,stepDelta,motion,warp,feedback,pulse;
uniform float bassHit,midHit,trebleHit,bass,mid,treble,midImpulse,trebleImpulse;
out vec4 fragColor;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec4 member(vec2 cell){return texelFetch(input0,clamp(ivec2(cell),ivec2(0),ivec2(resolution)-1),0);}
void main(){
    vec2 cell=floor(gl_FragCoord.xy),home=(cell+.5)/resolution;
    float seed=hash(cell),t=simulationTime;
    if(simulationReset>.5){fragColor=vec4(home,vec2(cos(seed*6.283),sin(seed*6.283))*.08);return;}
    vec4 s=member(cell);vec2 p=s.xy,v=s.zw,centre=vec2(0),align=vec2(0),separate=vec2(0);
    float count=0.;
    for(int i=0;i<12;i++){
        float angle=float(i)*2.399963;
        vec2 offset=round(vec2(cos(angle),sin(angle))*(2.+mod(float(i),4.)*3.));
        vec4 n=member(cell+offset);vec2 delta=n.xy-p;
        centre+=n.xy;align+=n.zw;count+=1.;
        separate-=delta*max(0.,1.-length(delta)/.075)/max(.002,dot(delta,delta))* .0015;
    }
    centre/=count;align/=count;
    vec3 ink=texture(input1,home).rgb;
    float chroma=ink.r-ink.b;
    vec2 hub=vec2(.5+.18*cos(t*.46+chroma*3.),.5+.17*sin(t*.37+ink.g*4.));
    vec2 q=p-hub,tangent=vec2(-q.y,q.x);
    vec2 force=(centre-p)*(1.8+mid*3.)+(align-v)*(1.+feedback*2.)+separate*(.5+bass*2.);
    force+=tangent*(1.4+mid*2.)+(home-p)*(.9+.5*(1.-warp));
    force+=vec2(cos(t*.7+home.y*8.+midImpulse),sin(t*.6-home.x*9.+trebleImpulse))*.11;
    force+=normalize(q+vec2(.001))*bassHit*pulse*.8+vec2(ink.r-ink.b,ink.g-.5)*trebleHit*.5;
    float dt=stepDelta*(.7+motion);
    v=(v+force*dt)*exp(-dt*(.55+.7*(1.-feedback)));
    v*=min(1.,.55/max(.001,length(v)));p+=v*dt;
    for(int a=0;a<2;a++){
        if(p[a]<.04){p[a]=.04;v[a]=abs(v[a])*.8;}
        if(p[a]>.96){p[a]=.96;v[a]=-abs(v[a])*.8;}
    }
    fragColor=vec4(p,v);
}

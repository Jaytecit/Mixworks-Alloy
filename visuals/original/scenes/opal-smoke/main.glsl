#version 330
#define FLOW_KIND 2
// Owned feedback instruments. FLOW_KIND: 0 ink wake, 1 rising embers, 2 smoke.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform sampler2D historyImage;
uniform int historyValid;
uniform float deltaTime,bass,mid,treble,bassHit,midHit,trebleHit;
uniform float bassTime,midTime,trebleTime,bassImpulse,midImpulse,trebleImpulse;
uniform float scale,warp,detail,hue,imageMix,intensity,pulse,feedback,orbit,focusX,focusY;
uniform float colourR,colourG,colourB;
out vec4 colour;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.13,311.71)))*43757.37);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
float cloud(vec2 p){return noise(p)*.57+noise(p*2.03+7.1)*.28+noise(p*4.1+13.)*.15;}
vec3 ink(float phase){return (.06+.94*pow(.5+.5*cos(6.283185*(phase+hue+vec3(.03,.32,.61))),vec3(1.4)))*mix(vec3(.6),vec3(colourR,colourG,colourB)*1.6,.55);}
vec3 sourceInk(vec2 uv,vec3 fallback){
    vec4 a=hasImage!=0?sampleMedia(uv):vec4(0),b=sampleCutout(uv);
    vec3 art=mix(a.rgb,b.rgb,b.a);
    return mix(fallback,art,max(a.a,b.a)*imageMix*.55);
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,aspect=vec2(resolution.x/resolution.y,1.);
    vec2 p=(uv-.5)*2.*aspect;
    p-=vec2(focusX-.5,focusY-.5)*.8;p*=.8+scale*.7;
    float t=travelTime,lo=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse;
    float stepSize=clamp(deltaTime*60.,0.,3.);
    vec2 flow;vec3 injection;float decay;
#if FLOW_KIND == 0
    // A thin continuous calligraphic stroke, carried sideways into a long wake.
    float bend=.24*sin(p.x*1.8-t*.52+midImpulse*.8)+.075*sin(p.x*5.5+midTime*.8);
    bend+=.014*hi*sin(p.x*72.-trebleImpulse*3.);
    float width=.012+.013*(1.-detail)+.008*mi;
    float stroke=exp(-pow((p.y-bend)/width,2.));
    stroke+=.24*exp(-pow((p.y-bend-.085)/(.006+width*.4),2.));
    float nib=exp(-pow((p.x-1.05)*1.2,2.));
    injection=sourceInk(vec2(p.x*.22+.5,bend*.5+.5),ink(p.x*.10+.2))*stroke*(.07+.30*mid+.24*mi)*(nib*.7+.3);
    flow=vec2(-.0042,.0008*sin(p.x*2.+midImpulse))*(.7+warp*.6);
    decay=.90+feedback*.075;
#elif FLOW_KIND == 1
    // Separate ballistic jets, rather than a continuous sinusoidal ribbon.
    float sparks=0.;
    for(int i=0;i<14;i++){
        float seed=hash(vec2(i,5)),phase=fract(t*(.23+seed*.19)+seed+bassImpulse*(.06+seed*.04));
        float x=(float(i)/13.-.5)*3.0+.10*sin(phase*5.+midImpulse+seed*12.)*(.4+warp);
        float y=-.83+phase*(1.25+.45*bass);
        float dx=(p.x-x)*(.8+seed*.5),dy=p.y-y;
        float head=exp(-dx*dx*9000.-dy*dy*2200.);
        float tail=exp(-dx*dx*3500.)*exp(min(0.,dy)*25.)*step(dy,0.)*smoothstep(-.17,0.,dy);
        sparks+=(head+tail*.45)*pow(1.-phase,1.6)*(.12+1.9*lo+.25*bass);
    }
    injection=sourceInk(vec2(uv.x,.45),ink(.02+uv.y*.08))*sparks;
    injection+=ink(.13)*sparks*hi*.35;
    flow=vec2(.0008*sin(p.y*5.+midImpulse)*(.3+mi),-.0045-.003*bass);
    decay=.64+feedback*.18;
#else
    // Broad, softly lit billows. Mids roll the volume; bass adds local pressure.
    vec2 q=p;
    q.x+=.30*sin(q.y*2.-t*.13+midImpulse*.22)*( .3+warp);
    float n=cloud(q*2.2+vec2(t*.055,-t*.16));
    q+=vec2(n-.5,cloud(q.yx*1.8-t*.07)-.5)*(.45+warp*.35);
    float body=exp(-pow(q.x/(.43+.24*clamp(q.y+.8,0.,1.7)),2.))*smoothstep(-1.0,-.62,q.y)*(1.-smoothstep(.35,1.05,q.y));
    float billow=smoothstep(.24,.76,cloud(q*3.1+vec2(-t*.035,-midTime*.45)));
    float density=body*(.28+.72*billow);
    float light=.35+.65*cloud(q*2.8+vec2(.3,.4)-t*.04);
    injection=sourceInk(uv,ink(.42+n*.15))*density*(.012+.025*mid+.013*mi)*light;
    injection+=ink(.62)*density*lo*.009;
    flow=vec2(.0014*sin(p.y*2.-midImpulse*.3)*( .3+warp),-.0013-.0010*mid);
    decay=.938+feedback*.049;
#endif
    vec2 chart=uv+flow*stepSize;
    vec3 previous=historyValid!=0?texture(historyImage,clamp(chart,0.,1.)).rgb:vec3(0);
#if FLOW_KIND == 2
    vec2 px=1./resolution;
    vec3 diffuse=(texture(historyImage,clamp(chart+vec2(px.x*2.,0),0.,1.)).rgb+texture(historyImage,clamp(chart-vec2(px.x*2.,0),0.,1.)).rgb+texture(historyImage,clamp(chart+vec2(0,px.y*2.),0.,1.)).rgb+texture(historyImage,clamp(chart-vec2(0,px.y*2.),0.,1.)).rgb)*.25;
    if(historyValid!=0)previous=mix(previous,diffuse,.15*min(stepSize,1.));
#endif
    vec3 c=previous*pow(decay,stepSize)+injection*stepSize;
    if(historyValid==0)c=injection;
    colour=vec4(clamp(c*(.94+intensity*.08),0.,2.),1.);
}

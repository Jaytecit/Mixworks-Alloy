#version 330
#define SIGNAL_KIND 2
// Each revised signal instrument has its own acquisition/release rule.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,stepDelta,simulationTime,feedback,motion,detail,bassHit,midHit,trebleHit,pulse;
out vec4 colour;
float signalHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 source=uv;
#if SIGNAL_KIND == 1
    source+=vec2(.055*sin(travelTime*.8),.025*sin(travelTime*.53));
#elif SIGNAL_KIND == 2
    // Even still artwork has a slowly moving capture plane. Each block keeps
    // its earlier plane position until its own acquisition event arrives.
    source+=vec2(.11*sin(travelTime*.7),.065*sin(travelTime*.49));
#endif
    vec4 a=hasImage!=0?sampleFittedMedia(source):vec4(0),b=sampleCutout(materialDomain(source));
    vec4 fresh=vec4(mix(a.rgb,b.rgb,b.a),1.);
    vec4 old=texture(input0,uv);
    if(hasImage==0&&hasCutout==0)fresh=vec4(.15+.7*uv.x,.15+.7*uv.y,.35+.3*sin(uv.x*14.),1.);
    if(simulationReset>.5){colour=fresh;return;}
    float capture=0.;
#if SIGNAL_KIND == 0
    float col=floor(uv.x*12.),side=mod(col,2.);
    float period=.4+feedback*(side>.5?2.4:.7);
    float now=simulationTime/period+signalHash(vec2(col,0));
    capture=step(.5,floor(now)-floor(now-stepDelta/period));
    capture=max(capture,step(.24,bassHit*pulse)*step(.72,signalHash(vec2(col,floor(simulationTime*2.)))));
#elif SIGNAL_KIND == 1
    // The band is wider than one 60Hz step, so every row is acquired on a sweep.
    float front=fract(travelTime*.31);
    capture=1.-step(.045,mod(front-uv.y+1.,1.));
#elif SIGNAL_KIND == 2
    vec2 cell=floor(uv*vec2(12,7));float seed=signalHash(cell);
    float period=.65+feedback*(1.2+seed*4.2);
    float now=simulationTime/period+seed;
    capture=step(.5,floor(now)-floor(now-stepDelta/period));
    // Bass refreshes a sparse subset once per rhythmic window, leaving old blocks.
    float tick=floor(simulationTime*4.)-floor((simulationTime-stepDelta)*4.);
    capture=max(capture,tick*step(.25,bassHit*pulse)*step(.83,signalHash(cell+floor(simulationTime*4.))));
    fresh.rgb=mix(fresh.rgb,fresh.gbr,midHit*pulse*.32*seed);
#elif SIGNAL_KIND == 3
    float rate=.25+(1.-feedback)*.9+trebleHit*pulse*.8;
    capture=1.-exp(-stepDelta*rate);
#elif SIGNAL_KIND == 4
    float strip=floor((uv.x+uv.y)*5.);
    float period=.45+feedback*(.6+mod(strip,2.)*1.4);
    float now=simulationTime/period+signalHash(vec2(strip,3));
    capture=step(.5,floor(now)-floor(now-stepDelta/period));
    capture=max(capture,stepDelta*trebleHit*pulse*.7);
#endif
    colour=mix(old,fresh,clamp(capture,0.,1.));
}

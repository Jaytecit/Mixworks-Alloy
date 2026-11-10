#version 330
#define COMPOSITION 1
// Owned bounded advected vector field. RG velocity, B curl, A source forcing.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset,simulationTime,stepDelta,motion,warp,pulse,bass,mid,treble;
uniform float bassHit,midHit,trebleHit,midImpulse,trebleImpulse;
out vec4 colour;
vec3 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));return mix(a.rgb,b.rgb,b.a);}
float lum(vec2 uv){return dot(art(uv),vec3(.2126,.7152,.0722));}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution,p=(uv-.5)*vec2(1.78,1.);
    float t=simulationTime*.3;
    vec2 grad=vec2(lum(uv+vec2(px.x,0))-lum(uv-vec2(px.x,0)),lum(uv+vec2(0,px.y))-lum(uv-vec2(0,px.y)));
    float source=lum(uv);vec2 force=vec2(0);
#if COMPOSITION == 0
    force=vec2(-p.y,p.x)*.11/(.22+dot(p,p))+vec2(.025,0);
#elif COMPOSITION == 1
    vec2 q=p-vec2(p.x<0.?-.42:.42,0);force=vec2(-q.y,q.x)*sign(p.x)*.13/(.14+dot(q,q));
#elif COMPOSITION == 2
    force=vec2(.015, .025+.12*source)+vec2(sin(p.y*7.+t),0)*.018;
#elif COMPOSITION == 3
    force=vec2(.17*exp(-p.y*p.y*8.),sin(p.x*5.-t*2.)*.045);
#elif COMPOSITION == 4
    force=vec2(-p.x*.16,p.y*.08)+vec2(sin(p.y*7.+t),cos(p.x*5.-t))*.035;
#elif COMPOSITION == 5
    vec2 cell=floor(uv*2.),q=fract(uv*2.)-.5;float spin=mod(cell.x+cell.y,2.)*2.-1.;
    force=vec2(-q.y,q.x)*spin*.28;
#elif COMPOSITION == 6
    vec3 image=art(uv);force=vec2(sin(p.y*6.+t)*.025,-.04-(image.r-image.b)*.16);
#else
    force=vec2(sign(p.y)*(.05+.05*mid),sin(p.x*6.+t)*.035);
#endif
    force+=vec2(-grad.y,grad.x)*(.18+warp*.3)+grad*bassHit*pulse*.45;
    force+=structureCurrent(uv)*.36;

    force+=vec2(sin(p.y*9.+midImpulse),cos(p.x*8.-trebleImpulse))*(midHit*.025+trebleHit*.012)*pulse;
    if(simulationReset>.5){colour=vec4(force*.2,0,source);return;}
    vec2 old=texture(input0,clamp(uv,0.,1.)).rg;
    vec2 chart=clamp(uv-old*stepDelta*(.7+motion),0.,1.);
    vec2 velocity=texture(input0,chart).rg;
    vec2 mean=(texture(input0,chart+vec2(px.x,0)).rg+texture(input0,chart-vec2(px.x,0)).rg+texture(input0,chart+vec2(0,px.y)).rg+texture(input0,chart-vec2(0,px.y)).rg)*.25;
    velocity=mix(velocity,mean,.13)+force*stepDelta;
    velocity*=exp(-stepDelta*.8);velocity=clamp(velocity,-.28,.28);
    float curl=(texture(input0,chart+vec2(px.x,0)).g-texture(input0,chart-vec2(px.x,0)).g
        -texture(input0,chart+vec2(0,px.y)).r+texture(input0,chart-vec2(0,px.y)).r)*.5;
    colour=vec4(velocity,curl,source);
}

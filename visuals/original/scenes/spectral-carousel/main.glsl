#version 330
#define COMPOSITION 4
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float detail,warp,bassHit,midImpulse,trebleImpulse,pulse;
out vec4 colour;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
void main(){
    vec2 uv=materialDomain(gl_FragCoord.xy/resolution),p=(uv-.5)*vec2(1.78,1.),q=uv;
    float shade=1.,t=travelTime*.18;
#if COMPOSITION == 0
    float z=max(.12,1.-abs(p.x)*.65);q=vec2(p.x/z,p.y/z)*.58+.5;
    q=1.-abs(mod(q,2.)-1.);shade=.48+.52*z;
#elif COMPOSITION == 1
    float a=atan(p.y,p.x),r=length(p);a=abs(mod(a+.392699,.785398)-.392699);
    q=.5+rot(t)*vec2(cos(a),sin(a))*r*.8;
#elif COMPOSITION == 2
    vec2 cell=floor(uv*vec2(3,2)),f=fract(uv*vec2(3,2))-.5;
    q=.5+f*(.8+.4*length(f))+.03*sin(cell+t);shade=1.-smoothstep(.35,.72,length(f));
#elif COMPOSITION == 3
    vec2 p0=rot(.3)*(uv-.5);vec2 cell=floor((p0+.5)*vec2(4,2));
    vec2 local=fract((p0+.5)*vec2(4,2))-.5;
    q=.5+rot(cell.x*.12+t*.07)*local*(1.1+cell.y*.15);
    shade=.55+.45*(1.-smoothstep(.12,.5,max(abs(local.x),abs(local.y))));
#elif COMPOSITION == 4
    float a=atan(p.y,p.x)+t,r=length(p);float sector=floor((a+3.141593)/1.047198);
    q=.5+rot(sector*.4+t*.3)*p*.64;shade=.7+.3*cos(sector*2.);
#elif COMPOSITION == 5
    q=vec2(atan(p.x,.65)/2.1+.5,.5+p.y/(.75+abs(p.x)));
    q.x=1.-abs(mod(q.x*3.,2.)-1.);
#elif COMPOSITION == 6
    float slat=mod(floor(uv.x*(30.+detail*40.)),2.);q.x+=mix(-1.,1.,slat)*(.045+.02*sin(t));
#else
    float r=length(p),a=atan(p.y,p.x);q=.5+vec2(cos(a+t*.15),sin(a+t*.15))*r/(1.+r*.3);
#endif
    vec3 c=texture(input0,clamp(q,0.,1.)).rgb*shade;
    vec2 shift=vec2(.003+.007*trebleImpulse*.01,0)*warp;
    c.r=texture(input0,clamp(q+shift,0.,1.)).r*shade;c.b=texture(input0,clamp(q-shift,0.,1.)).b*shade;
    colour=vec4(clamp(pow(max(c,0.),vec3(.74))*1.15,0.,1.),1.);
}

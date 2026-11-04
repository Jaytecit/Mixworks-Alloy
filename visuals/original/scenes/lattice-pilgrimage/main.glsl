#version 330
#define COMPOSITION 1
// Owned solid instruments: local geometry, spatial media and distinct camera paths.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bass,mid,treble,bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse,pulse,warp,detail,imageMix;
out vec4 colour;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 art(vec2 uv){vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));return mix(a.rgb,b.rgb,b.a);}
float feed(vec2 p){return mix(.5,dot(art(fract(p*.12+.5)),vec3(.2126,.7152,.0722)),imageMix);}
float box(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
float hex(vec2 p){p=abs(p);return max(p.x*.866025+p.y*.5,p.y);}
float field(vec3 p){
    float t=travelTime*.34+midImpulse*.08*pulse;
#if COMPOSITION == 0
    vec2 id=floor(p.xz/1.05),q=mod(p.xz,1.05)-.525;
    q.x+=mod(id.y,2.)*.12;
    float height=.15+feed(id)*1.3+(.18+bassHit*pulse*.5)*(.5+.5*sin(id.x*.7+id.y*.9+t));
    return max(hex(q)-.42,abs(p.y-height*.5)-height*.5);
#elif COMPOSITION == 1
    vec3 q=mod(p+1.,2.)-1.;
    float rods=min(length(q.xy),min(length(q.xz),length(q.yz)))-.055;
    float diagonal=length(vec2(q.x-q.y,q.z-q.y))*.577-.045;
    return min(rods,diagonal);
#elif COMPOSITION == 2
    vec3 q=p;q.xz=rot(t*.4)*q.xz;
    float sphere=length(q)-1.3-(feed(q.xy)*.06+bassHit*.05);
    return max(sphere,(dot(abs(q),vec3(1))-1.95)*.57735);
#elif COMPOSITION == 3
    vec2 id=floor(p.xz/1.15);vec3 q=vec3(mod(p.x,1.15)-.575,p.y,mod(p.z,1.15)-.575);
    q.xy=rot(sin(t+id.x*.6+id.y)*(.25+mid*.5))*q.xy;
    return box(q-vec3(0,.45+feed(id)*.4,0),vec3(.38,.12+feed(id)*.5,.4));
#elif COMPOSITION == 4
    vec2 id=floor(p.xz/1.3);vec3 q=vec3(mod(p.x,1.3)-.65,p.y,mod(p.z,1.3)-.65);
    q.xy=rot(sin(t+id.x+id.y)*.3)*q.xy;
    float top=1.25+feed(id)*.4;return max(abs(q.y-top)-.5,max(abs(q.x),abs(q.z))-(top-q.y)*.45-.06);
#elif COMPOSITION == 5
    vec3 q=p;q.z=mod(q.z,1.3)-.65;
    float arch=abs(length(vec2(q.x,q.y-1.2))-(1.7+.04*sin(t+q.z)))-.08;
    return max(abs(q.z)-.09,arch);
#elif COMPOSITION == 6
    vec3 id=floor(p/1.35),q=mod(p+.675,1.35)-.675;
    q.xz=rot(t*.5+id.y*.4)*q.xz;
    float solid=(dot(abs(q),vec3(1))-(.52+feed(id.xz)*.28+bassHit*.07))*.577;
    return max(solid,.46-length(p.xy-vec2(2.3,1.7)));
#else
    vec3 q=p;q.xy=rot(p.z*.18+t*.06)*q.xy;q.z=mod(q.z,1.2)-.6;
    return max(abs(q.z)-.09,abs(hex(q.xy)-1.35)-.07);
#endif
}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,p=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    vec3 eye=vec3(2.7,2.2,-3.4),look=vec3(0,.3,1.);
#if COMPOSITION == 1 || COMPOSITION == 5 || COMPOSITION == 7
    eye=vec3(.27,.56,travelTime*.6+bassImpulse*.15*pulse);look=eye+vec3(.05,.05,1.);
#elif COMPOSITION == 2
    eye=vec3(sin(travelTime*.12)*2.9,.65,cos(travelTime*.12)*2.9);look=vec3(0);
#elif COMPOSITION == 4
    eye=vec3(2.4,.3,-3.);look=vec3(0,1.1,2.);
#elif COMPOSITION == 6
    eye=vec3(2.3,1.7,-3.3+travelTime*.28);look=eye+vec3(-.35,-.1,1.);
#else
    eye.z+=travelTime*.4;look.z+=travelTime*.4;
#if COMPOSITION == 0
    eye=vec3(-1.6,3.4,-3.3+travelTime*.4);look=vec3(0,.2,2.+travelTime*.4);
#elif COMPOSITION == 3
    eye.y=1.35;eye.z-=.6;
#endif
#endif
    vec3 f=normalize(look-eye),r=normalize(cross(f,vec3(0,1,0))),u=cross(r,f),ray=normalize(f*1.5+r*p.x+u*p.y);
    float dist=.03;bool hit=false;vec3 q=eye;
    for(int i=0;i<76;i++){q=eye+ray*dist;float d=field(q);if(d<.006){hit=true;break;}dist+=max(.005,d*.65);if(dist>18.)break;}
    vec3 bg=vec3(.003,.005,.018)+art(uv)*.008,c=bg;
    if(hit){float e=.004;vec3 n=normalize(vec3(field(q+vec3(e,0,0))-field(q-vec3(e,0,0)),field(q+vec3(0,e,0))-field(q-vec3(0,e,0)),field(q+vec3(0,0,e))-field(q-vec3(0,0,e))));
        vec2 chart=fract(q.xz*.16+.5);if(abs(n.z)>.5)chart=fract(q.xy*.18+.5);else if(abs(n.x)>.5)chart=fract(q.zy*.18+.5);
        vec3 ink=art(chart);if(hasImage==0&&hasCutout==0)ink=vec3(.05,.35,.85);
        float diffuse=.32+.68*max(0.,dot(n,normalize(vec3(-.5,1.,-.5))));
        float rim=pow(1.-abs(dot(n,-ray)),3.);c=(ink*.9+vec3(.06,.13,.24))*diffuse+vec3(.02,.7,1.)*(rim*.3+.03);
        c=mix(c,bg,1.-exp(-dist*.055));
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

#version 330
// A real steep image-relief wall and low ground, viewed while travelling along it.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform int hasImage;
uniform float detail,imageMix,pulse,intensity,bassHit,midImpulse,trebleHit;
out vec4 colour;
vec3 sourceArt(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(materialDomain(uv));
    return mix(a.rgb,b.rgb,b.a);
}
float imageHeight(vec2 uv){return hasImage!=0||hasCutout!=0?texture(input0,clamp(uv,0.,1.)).r:.5;}
float strata(vec3 p){return p.y*(6.+detail*6.)+.28*sin(p.z*.6)+midImpulse*.16*pulse;}
float wallTop(float z){return 3.6+.32*sin(z*.25)+.14*sin(z*.63)+bassHit*pulse*.12;}
float wallFace(vec3 p){
    float image=imageHeight(vec2(fract(p.z*.055+.5),clamp((p.y+1.4)/5.3,0.,1.)));
    float ledge=sin(strata(p))*(.035+.055*bassHit*pulse);
    return 1.1+.18*sin(p.z*.35)+.10*sin(p.z*1.1)+ledge+(image-.4)*imageMix*.15;
}
float rockDistance(vec3 p){
    float cliff=max(wallFace(p)-p.x,p.y-wallTop(p.z));
    return min(p.y+1.4,cliff);
}
void main(){
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    vec3 eye=vec3(-2.6,1.5,-3.+travelTime*.85);
    vec3 forward=normalize(vec3(.78,.04,.63));
    vec3 right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*1.45+right*uv.x+up*uv.y);
    vec3 sky=mix(vec3(.008,.014,.035),vec3(.04,.085,.16),clamp(ray.y*.7+.4,0.,1.));
    float distance=.1;vec3 p=eye;bool hit=false;
    for(int i=0;i<96;i++){
        p=eye+ray*distance;float d=rockDistance(p);
        if(d<.007){hit=true;break;}
        distance+=max(.015,d*.48);if(distance>42.)break;
    }
    vec3 c=sky;
    if(hit){
        float e=.012;
        vec3 n=normalize(vec3(rockDistance(p+vec3(e,0,0))-rockDistance(p-vec3(e,0,0)),
            rockDistance(p+vec3(0,e,0))-rockDistance(p-vec3(0,e,0)),
            rockDistance(p+vec3(0,0,e))-rockDistance(p-vec3(0,0,e))));
        bool face=abs(n.x)>.55;
        vec2 chart=face?vec2(fract(p.z*.055+.5),clamp((p.y+1.4)/5.3,0.,1.)):
            fract(p.xz*.055+vec2(.5,.25));
        vec3 image=sourceArt(chart);
        float layer=.5+.5*sin(strata(p));
        vec3 stone=vec3(.055,.105,.15)+layer*vec3(.035,.055,.075);
        float light=.22+.78*max(0.,dot(n,normalize(vec3(-.65,.8,-.35))));
        c=(stone+image*imageMix*.62)*light;
        float seam=pow(.5+.5*cos(strata(p)),20.);
        c+=seam*(.025+.22*trebleHit*pulse)*vec3(.12,.65,1.)*(face?1.:.25);
        float rim=exp(-abs(p.y-wallTop(p.z))*8.);
        c+=rim*vec3(.025,.13,.20);
        c=mix(c,sky,1.-exp(-distance*.035));
    }
    colour=vec4(clamp(c*(.72+.6*intensity),0.,1.),1.);
}

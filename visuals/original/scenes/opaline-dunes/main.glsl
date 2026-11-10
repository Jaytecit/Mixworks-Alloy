#version 330
// Continuous image-influenced dunes: no faceted mesh or triangle-edge finish.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform int hasImage;
uniform float sceneTime,motion,detail,imageMix,pulse,intensity,bassHit,midImpulse,trebleHit;
out vec4 colour;
vec3 sourceArt(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=spriteMaterial(uv);
    return mix(a.rgb,b.rgb,b.a);
}
float windPhase(){return sceneTime*(.22+.35*motion)+midImpulse*.14*pulse;}
float dunePhase(vec2 p){return p.y*.52+.24*sin(p.x*.18)-windPhase();}
float heightAt(vec2 p){
    float phase=dunePhase(p);
    float broad=.62*sin(phase)+.15*sin(phase*2.+.5);
    float crosswind=.13*sin(p.x*.23+p.y*.08);
    vec2 chart=fract(p*.035+vec2(.5,.25));
    float image=hasImage!=0||hasCutout!=0?texture(input0,chart).r:.5;
    float ripples=sin(phase*(15.+detail*12.)+p.x*.25-windPhase()*1.5);
    float grain=(.006+.010*trebleHit*pulse)*ripples;
    return .55*spriteRelief(chart)+broad+crosswind+(image-.4)*imageMix*.16+grain
        +bassHit*pulse*.20*(.6+.4*cos(phase))-.45;
}
void main(){
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    // Travel along the long ridges; wind has its own scene clock.
    vec3 eye=vec3(-3.+travelTime*.55,1.9,-4.+travelTime*.12);
    vec3 forward=normalize(vec3(.12,-.22,1.));
    vec3 right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*1.45+right*uv.x+up*uv.y);
    vec3 sky=mix(vec3(.008,.014,.035),vec3(.035,.075,.14),clamp(ray.y*.7+.35,0.,1.));
    float cirrus=pow(.5+.5*sin(ray.x*9.+sin(ray.y*12.+travelTime*.16)*1.2-travelTime*.28),8.);
    sky+=vec3(.07,.13,.18)*cirrus*smoothstep(-.05,.28,ray.y)*(.45+.50*bassHit*pulse);
    float distance=.2;vec3 p=eye;bool hit=false;
    for(int i=0;i<90;i++){
        p=eye+ray*distance;float gap=p.y-heightAt(p.xz);
        if(gap<.008){hit=true;break;}
        distance+=max(.02,gap*.48);if(distance>42.)break;
    }
    vec3 c=sky;
    if(hit){
        float e=.025;
        vec3 n=normalize(vec3(heightAt(p.xz-vec2(e,0))-heightAt(p.xz+vec2(e,0)),2.*e,
            heightAt(p.xz-vec2(0,e))-heightAt(p.xz+vec2(0,e))));
        float phase=dunePhase(p.xz);
        vec3 image=sourceArt(fract(p.xz*.035+vec2(.5,.25)));
        float crest=.5+.5*cos(phase-.4);
        vec3 sand=vec3(.065,.12,.18)+crest*vec3(.055,.09,.10);
        c=(sand+image*imageMix*.48)*(.20+.80*max(0.,dot(n,normalize(vec3(-.45,.8,-.55)))));
        float ripple=pow(.5+.5*cos(phase*(15.+detail*12.)+p.x*.25-windPhase()*1.5),18.);
        c+=ripple*(.016+.15*trebleHit*pulse)*vec3(.12,.65,1.);
        c+=pow(crest,12.)*(.018+.055*bassHit*pulse)*vec3(.15,.55,.8);
        c=mix(c,sky,1.-exp(-distance*.036));
    }
    colour=vec4(clamp(c*(.72+.6*intensity),0.,1.),1.);
}

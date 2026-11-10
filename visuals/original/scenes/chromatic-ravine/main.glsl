#version 330
// A continuous image-height valley with an optional triangle overlay.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform int hasImage;
uniform float detail,imageMix,pulse,bass,bassHit,kickImpact,lattice;
out vec4 colour;

vec2 chartAt(vec2 p){
    return fract(p*.052+vec2(.5,.25));
}
float tileCoverage(vec2 chart){
    vec2 edge=min(chart,1.-chart);
    // Both sides of the repeat meet the native terrain with zero slope.
    return smoothstep(0.,.11,min(edge.x,edge.y));
}
vec3 sourceArt(vec2 chart,vec3 nativeRock){
    vec4 background=hasImage!=0?sampleFittedMedia(chart):vec4(0);
    vec4 cut=surfaceSpriteMaterial(chart);
    vec3 art=mix(mix(nativeRock,background.rgb,background.a),cut.rgb,cut.a);
    return mix(nativeRock,art,tileCoverage(chart));
}
float heightAt(vec2 p){
    vec2 chart=chartAt(p);
    vec3 field=texture(input0,chart).rgb*tileCoverage(chart);
    float density=mix(.72,1.24,detail);
    float nativeHeight=.4+.25*sin(p.x*.7*density)*cos(p.y*.6*density);
    float lum=nativeHeight*(1.-field.g)+field.r;
    float side=sqrt(p.x*p.x+.09)-.3;
    float walls=1.8*(1.-exp(-side*.42))-.4;
    return lum*(.7+imageMix*1.2)-.8+walls
        +.55*reliefDepth*field.b+(.25*kickImpact+.018*bass)*pulse*exp(-side*.3);
}
float gridLine(vec2 grid){
    vec2 f=fract(grid);
    float edge=min(min(min(f.x,1.-f.x),min(f.y,1.-f.y)),abs(f.x+f.y-1.)*.707);
    float footprint=max(length(dFdx(grid)),length(dFdy(grid)));
    return (1.-smoothstep(.009,.009+max(.003,footprint*.65),edge))
        *(1.-smoothstep(.055,.21,footprint));
}
void main(){
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float travel=travelTime*.72;
    vec3 eye=vec3(.2,1.4,-4.+travel);
    vec3 forward=normalize(vec3(.2*sin(travel*.06),-.24,1.));
    vec3 right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*1.45+right*uv.x+up*uv.y);
    vec3 sky=mix(vec3(.008,.014,.021),vec3(.054,.073,.093),clamp(uv.y*.5+.5,0.,1.));
    float strata=pow(.5+.5*sin(ray.x*8.+sin(ray.y*6.-travelTime*.3)*2.+travelTime*.25),7.);
    float veil=smoothstep(-.08,.35,ray.y)*strata;
    sky+=vec3(.14,.10,.065)*veil*(.55+(.16*bassHit+.28*kickImpact)*pulse);
    float dist=.2,stepSize=0.,previousGap=0.;
    vec3 p=eye;
    bool hit=false;
    for(int i=0;i<80;i++){
        p=eye+ray*dist;
        float gap=p.y-heightAt(p.xz);
        if(gap<.008){
            hit=true;
            if(stepSize>0.){
                float slope=max(.15,(previousGap-gap)/stepSize);
                dist+=clamp(gap/slope,-stepSize,.035);
                p=eye+ray*dist;
            }
            break;
        }
        previousGap=gap;
        // A grazing ray needs to keep moving through the distant terrain.
        stepSize=max(.030+dist*.040,gap*.58);
        dist+=stepSize;
        if(dist>34.)break;
    }
    float rods=0.;
    if(lattice>0.)rods=gridLine(p.xz/(.46+.36*(1.-detail)));
    vec3 c=sky;
    if(hit){
        float e=.042;
        vec3 n=normalize(vec3(heightAt(p.xz-vec2(e,0))-heightAt(p.xz+vec2(e,0)),2.*e,
            heightAt(p.xz-vec2(0,e))-heightAt(p.xz+vec2(0,e))));
        vec3 nativeRock=mix(vec3(.24,.18,.14),vec3(.63,.39,.22),smoothstep(-.45,1.15,p.y));
        vec3 art=sourceArt(chartAt(p.xz),nativeRock);
        float light=.23+.77*max(0.,dot(n,normalize(vec3(-.5,.9,-.4))));
        float strataTone=1.+.055*sin(10.*p.y+sin(p.z*.58));
        c=art*(.4+.6*imageMix)*light*strataTone;
        c+=lattice*rods*vec3(.48,.56,.60)*(.25+(.06*bassHit+.14*kickImpact)*pulse);
        float clarity=exp(-dist*.068)*(1.-smoothstep(18.,34.,dist));
        c=mix(sky,c,clarity);
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

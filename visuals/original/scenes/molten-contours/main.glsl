#version 330
// Scene-local image heightfield. Relief, domain warp and colour are independent.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,motion,scale,hue,intensity,pulse,bass,bassHit,treble,projectBeat;
out vec4 colour;
float columnFlow(float x){
    return .045+.075*(.5+.5*sin(x*17.+sin(x*39.)*1.7+sceneTime*.38));
}
vec2 warped(vec2 uv){
    return uv+vec2(.018*sin(uv.y*13.+sceneTime*.6),columnFlow(uv.x)*(.5+.5*sin(sceneTime*.47+uv.x*9.)))*meltWarp;
}
vec4 sourceAt(vec2 uv,float lod){
    uv=warped(uv);
    vec4 c=vec4(0);
    if(hasCutout!=0){
        vec2 a=(uv-.5)*cutoutAspect+.5,b=(uv-.5)*nextCutoutAspect+.5;
        vec4 x=vec4(0),y=vec4(0);
        if(all(greaterThanEqual(a,vec2(0)))&&all(lessThanEqual(a,vec2(1))))x=textureLod(cutoutImage,a,lod);
        if(all(greaterThanEqual(b,vec2(0)))&&all(lessThanEqual(b,vec2(1))))y=textureLod(nextCutoutImage,b,lod);
        c=mix(x,y,cutoutBlend); // Premultiplied before filtering, not after it.
    }else if(hasImage!=0){
        c=sampleFittedMedia(uv);c.rgb*=c.a;
    }else{
        float ring=1.-smoothstep(.014,.035,abs(length((uv-.5)*vec2(1.65,1.))-.27));
        c=vec4(vec3(.04,.45,.7)*ring,ring);
    }
    return c;
}
float sourceHeight(vec2 uv){
    // A mip-filtered luminance field gives bevels instead of pixel cliffs.
    vec4 c=sourceAt(uv,2.2);
    return dot(c.rgb,vec3(.2126,.7152,.0722));
}
vec2 imageUV(vec2 p){return p/vec2(2.8,1.65)+.5;}
float heightAt(vec2 p,float depth){return depth*sourceHeight(imageUV(p))+.26*structureHeight(imageUV(p));}

vec3 reliefVapour(vec3 result,vec3 eye,vec3 ray,float surfaceZ,float depth,float phase,float strength){
    float envelope=smoothstep(0.,.12,phase)*(1.-smoothstep(.34,.98,phase));
    float puff=strength*envelope;
    if(puff<=.002)return result;
    float rise=.08+.76*phase;
    float spread=1.+.50*phase;
    // Trace several slices above the plate. Luminance locates the lifted
    // surface, and the same premultiplied source supplies its vapour density.
    float jitter=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453);
    for(int i=0;i<12;i++){
        float z=rise+(float(i)+jitter)*.0875;
        if(z<surfaceZ)continue;
        vec3 q=eye+ray*((z-eye.z)/ray.z);
        vec2 uv=(imageUV(q.xy)-.5)/spread+.5;
        vec2 drift=sin(uv.yx*vec2(12.,17.)+vec2(z*7.,-z*8.)+projectBeat*.85);
        uv+=drift*phase*(.055+.05*treble);
        if(any(lessThan(uv,vec2(0.)))||any(greaterThan(uv,vec2(1.))))continue;
        vec4 art=sourceAt(uv,2.8+phase*3.2);
        float height=depth*dot(art.rgb,vec3(.2126,.7152,.0722));
        float shell=exp(-abs(z-rise-.10-height)/(.085+.21*phase));
        float density=art.a*shell*puff*(.30+.14*treble);
        vec3 smoke=mix(art.rgb/max(art.a,.0001),vec3(.59,.73,.88),.30+phase*.24);
        result=mix(result,smoke,density);
    }
    return result;
}
void main(){
    vec2 screen=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    screen*=.82+scale*.5;
    vec3 eye=vec3(.18,-1.28,2.8),aim=vec3(0,.04,0);
    vec3 forward=normalize(aim-eye),right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
    vec3 ray=normalize(forward*2.25+right*screen.x+up*screen.y);
    float phase=fract(projectBeat);
    float attack=clamp(max(bassHit*1.35,bass*exp(-8.*phase)*1.3),0.,1.)*pulse;
    float depth=.48*reliefDepth*(1.+2.15*attack);
    float top=depth,t=max(0.,(top-eye.z)/ray.z);
    float stepSize=(top+.025)/max(.15,-ray.z)/36.;
    vec3 p=eye+ray*t;
    for(int i=0;i<40;i++){
        p=eye+ray*t;
        if(p.z<=heightAt(p.xy,depth))break;
        t+=stepSize;
    }
    // Three bisections soften the ray/height crossing without smoothing RGB.
    float lo=max(0.,t-stepSize),hi=t;
    for(int i=0;i<3;i++){
        float mid=(lo+hi)*.5;vec3 q=eye+ray*mid;
        if(q.z>heightAt(q.xy,depth))lo=mid;else hi=mid;
    }
    p=eye+ray*hi;
    vec2 uv=imageUV(p.xy);
    vec3 result=vec3(.008,.014,.025);
    if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){
        float e=.007;
        vec3 normal=normalize(vec3(heightAt(p.xy-vec2(e,0),depth)-heightAt(p.xy+vec2(e,0),depth),
                                   heightAt(p.xy-vec2(0,e),depth)-heightAt(p.xy+vec2(0,e),depth),2.*e));
        vec3 light=normalize(vec3(-.65,-.5,1.4));
        vec4 art=sourceAt(uv,0.);
        vec3 base=mix(vec3(.035,.055,.075),art.rgb/max(art.a,.0001),art.a);
        // Potential source displacement drives colour even with depth/warp off.
        float flow=columnFlow(uv.x)+sourceHeight(uv)*.7;
        vec3 neon=.5+.5*cos(6.283185*(flow+sceneTime*.035+hue+vec3(0.,.32,.64)));
        base=mix(base,neon*max(.14,max(base.r,max(base.g,base.b))),displacementColour*art.a);
        float diffuse=.25+.75*max(0.,dot(normal,light));
        float spec=pow(max(0.,dot(reflect(-light,normal),-ray)),42.);
        result=base*diffuse+vec3(.6,.8,1.)*spec*(.12+.18*reliefDepth);
        float edge=smoothstep(0.,.012,min(min(uv.x,1.-uv.x),min(uv.y,1.-uv.y)));
        result=mix(vec3(.008,.014,.025),result,edge);
    }
    float vapour=reliefDepth*pulse*clamp(max(bass*1.9,bassHit),0.,1.);
    result=reliefVapour(result,eye,ray,max(p.z,0.),depth,phase,vapour);
    colour=vec4(clamp(result,0.,1.),1.);
}

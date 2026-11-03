#version 330
// Original liquid enamel composition. Shared flow drives independent image
// charts, pigment transport and illuminated contours at several spatial scales.
uniform vec2 resolution;
uniform sampler2D input0,input1,input2;
uniform sampler2D handoffAppearance;
uniform sampler2D userImage;
uniform int hasImage;
uniform float sceneTime,bassTime,midTime,trebleTime,bass,mid,treble,trebleHit,bassHit;
uniform float bassImpulse,midImpulse,trebleImpulse;
uniform float intensity,imageMix,warp,detail,pulse,scale,generationBlend;
out vec4 colour;
vec2 reflectUV(vec2 p){return 1.-abs(mod(p,2.)-1.);}
vec4 art(vec2 uv){
    vec4 c=hasImage!=0?sampleFittedMedia(reflectUV(uv)):vec4(0);
    vec4 s=sampleCutout(reflectUV(uv));return vec4(mix(c.rgb,s.rgb,s.a),max(c.a,s.a));
}
float heightAt(vec2 uv){
    vec2 chart=texture(input1,uv).xy;
    // Integrated band energy advances the surface: a fading hit slows travel
    // rather than pulling the surface back to its pre-hit position.
    vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.)
        +vec2(-travelTime*.045-bassTime*.18-bassImpulse*.35,midTime*.12+midImpulse*.30);
    float broad=sin(p.x*5.+sin(p.y*4.))+cos(p.y*5.7-p.x*2.);
    float image=dot(art(chart).rgb,vec3(.21,.72,.07));
    return broad*.40+image*.025*imageMix;
}
vec3 srgb(vec3 c){return mix(12.92*c,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,step(vec3(.0031308),c));}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    vec4 flow=texture(input0,uv);vec2 chart=texture(input1,uv).xy;
    vec2 g=vec2(heightAt(uv+vec2(px.x,0))-heightAt(uv-vec2(px.x,0)),heightAt(uv+vec2(0,px.y))-heightAt(uv-vec2(0,px.y)));
    vec3 n=normalize(vec3(-g*32.,1.));
    vec2 material=reflectUV(uv+g*warp*.55);
    vec3 dye=texture(input2,material).rgb;
    float h=heightAt(uv);
    // A small composition nested inside the transported parent chart. It turns
    // independently and is revealed in pools, not in a grid of image rectangles.
    vec2 nested=chart-.5;
    float angle=midTime*.40+midImpulse*.65+travelTime*.06;
    nested=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*nested;
    vec4 inlay=art(nested*1.75+.5+vec2(trebleTime*.06+trebleImpulse*.16,0));
    float lum=dot(inlay.rgb,vec3(.21,.72,.07))*inlay.a;
    float pools=smoothstep(-.25,.30,h);
    dye*=mix(.45,1.25,smoothstep(.08,.7,lum)*pools*.7+.3);
    vec3 albedo=pow(max(dye,vec3(0)),vec3(2.2));
    float diffuse=.32+1.05*max(0.,dot(n,normalize(vec3(-.5,.7,1.))));
    float spec=pow(max(0.,dot(n,normalize(vec3(-.3,.4,1.)))),42.);
    float ridge=pow(.5+.5*sin(h*22.+flow.z*3.),26.);
    vec3 c=albedo*diffuse+vec3(.4,.75,.85)*spec*.24;
    c+=dye*ridge*(.12+.13*detail+trebleHit*.65*pulse);
    float edge=length(g);
    c=mix(c,c*.34,smoothstep(.035,.09,edge)*.45);
    c*=.8+intensity*.35;
    c=c/(1.+c*.36);
    vec3 result=clamp(srgb(c),0.,1.);
    if(generationBlend<1.){
        // The outgoing architecture bends into currents; an uneven liquid front
        // reveals the enamel. Full-resolution source keeps the first frame sharp.
        float a=generationBlend;
        vec2 d=flow.xy/vec2(textureSize(input0,0));
        vec2 oldUV=reflectUV(uv-d*a*1.5+g*a*2.);
        vec3 old=texture(handoffAppearance,oldUV).rgb;
        float front=clamp(uv.x*.57+uv.y*.20+.10*sin(uv.y*13.+h*3.)+.06*sin(uv.x*19.),0.,1.);
        float reveal=smoothstep(front-.10,front+.10,a*1.3-.20);
        result=mix(old,result,reveal);
    }
    colour=vec4(result,1);
}

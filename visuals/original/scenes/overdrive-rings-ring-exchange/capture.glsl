#version 330
uniform float bassImpulse,midImpulse,trebleImpulse,bassTime,midTime,trebleTime;
#define FAMILY 7
// RGBA is retained premultiplied pigment/coverage, never a final-image overlay.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float simulationReset,simulationTime,stepDelta;
uniform float bassHit,trebleHit,imageMix;
uniform float generationBlend;
out vec4 fragColor;
// Original premultiplied sampling: hidden RGB cannot stain captured edges.
uniform sampler2D userImage;
uniform int hasImage;
vec4 pmSample(sampler2D tex,vec2 uv) {
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec4(0);
    ivec2 sz=textureSize(tex,0);vec2 p=uv*vec2(sz)-.5;ivec2 q=ivec2(floor(p));vec2 f=fract(p);
    vec4 a=texelFetch(tex,clamp(q,ivec2(0),sz-1),0),b=texelFetch(tex,clamp(q+ivec2(1,0),ivec2(0),sz-1),0);
    vec4 c=texelFetch(tex,clamp(q+ivec2(0,1),ivec2(0),sz-1),0),d=texelFetch(tex,clamp(q+ivec2(1),ivec2(0),sz-1),0);
    a.rgb*=a.a;b.rgb*=b.a;c.rgb*=c.a;d.rgb*=d.a;
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
// Original Alloy material composition. Child charts inherit their parent chart,
// then turn, advect and refract independently; the grandchild inherits the child.
mat2 nestedTurn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 nestedLinear(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 nestedInk(vec2 uv){
    uv=1.-abs(mod(uv,2.)-1.);
    vec3 bg=mix(nestedLinear(textureLod(userImage,uv,0.).rgb),nestedLinear(textureLod(nextImage,uv,0.).rgb),imageBlend);
    vec4 cut=sampleCutout(uv);
    return mix(bg,nestedLinear(cut.rgb),cut.a*.85);
}
vec3 nestedMaterial(vec2 uv,vec3 parent,float style){
    vec2 chart=(uv-.5)*(2.4+mod(style,3.)*.55);
    vec2 cell=floor(chart+.5),p=fract(chart+.5)-.5;
    float id=dot(cell,vec2(1.71,2.93))+style;
    float turn=midImpulse*(.8+mod(style,3.)*.28)+midTime*.25;
    vec2 child=nestedTurn(turn+id*.37)*p;
    // Liquid inlays, angular frames and ribbon windows have distinct domains.
    if(mod(style,3.)<1.)child+=.07*vec2(sin(child.y*9.-bassImpulse*3.),cos(child.x*8.+trebleImpulse*4.));
    else if(mod(style,3.)<2.)child.x+=.12*sin(child.y*6.+bassImpulse*2.);
    else child=nestedTurn(.3*sin(length(child)*8.-bassImpulse*3.))*child;
    float shape=mod(style,3.)<1.?length(child):max(abs(child.x),abs(child.y));
    float mask=1.-smoothstep(.31,.36,shape);
    vec3 ink=nestedInk(child/(.65+.13*bassHit)+.5);
    vec2 grand=nestedTurn(-trebleImpulse*1.9-trebleTime*.4)*(child-vec2(.12*sin(id),.1*cos(id)))*2.8;
    float inner=1.-smoothstep(.22,.29,max(abs(grand.x),abs(grand.y)));
    ink=mix(ink,nestedInk(grand+.5),inner*.85);
    vec3 rim=.5+.5*cos(vec3(0,2.1,4.2)+id*.4+trebleImpulse*.35);
    ink+=rim*exp(-abs(shape-.32)*100.)*(.15+.8*trebleHit);
    return mix(parent,ink,mask*.78);
}

vec4 nestedMedia(vec2 uv) {
    vec2 p=(uv-.5)/(.68+.16*imageZoom);
    float a=(imageRotation-.5)*6.283185;p=mat2(cos(a),-sin(a),sin(a),cos(a))*p;
    vec4 bg=vec4(0),cut=vec4(0);
    if(hasImage!=0)bg=mix(pmSample(userImage,p*mediaAspect/min(mediaAspect.x,mediaAspect.y)+.5),pmSample(nextImage,p*nextMediaAspect/min(nextMediaAspect.x,nextMediaAspect.y)+.5),imageBlend);
    // Contain both dimensions; fitting only height crops wide sprites.
    vec2 fit=vec2((resolution.x/resolution.y)*cutoutAspect.x/cutoutAspect.y,1.);
    vec2 nextFit=vec2((resolution.x/resolution.y)*nextCutoutAspect.x/nextCutoutAspect.y,1.);
    if(hasCutout!=0)cut=mix(pmSample(cutoutImage,p*fit/min(fit.x,fit.y)+.5),pmSample(nextCutoutImage,p*nextFit/min(nextFit.x,nextFit.y)+.5),cutoutBlend);
    vec4 merged=cut+bg*(1.-cut.a);
 merged.rgb=mix(merged.rgb,nestedMaterial(uv,merged.rgb,7.0),merged.a*.35);return merged;
}

void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 vec4 fresh=nestedMedia(uv);
 if(simulationReset>.5){
  if(hasImage==0&&hasCutout==0){
   vec2 q=(uv-.5)*vec2(1.7778,1.);float a=atan(q.y,q.x);
   float ring=1.-smoothstep(.012,.03,abs(length(q)-(.24+.04*cos(a*5.))));
   fresh=vec4(vec3(.2+.6*uv.x,.4+.4*uv.y,.9)*ring,ring);
  }
  fragColor=fresh*imageMix;return;
 }
 vec4 old=texture(input0,uv);
 float present=float(hasImage!=0||hasCutout!=0);
 float refresh=(1.-exp(-stepDelta*(.2+4.*max(bassHit,trebleHit))*imageMix*(.1+.9*generationBlend)))*present;
 fragColor=mix(old,fresh,refresh);
}

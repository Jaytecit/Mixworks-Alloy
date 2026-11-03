#version 330
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
vec4 nestedMedia(vec2 uv) {
    vec2 p=(uv-.5)/(.68+.16*imageZoom);
    float a=(imageRotation-.5)*6.283185;p=mat2(cos(a),-sin(a),sin(a),cos(a))*p;
    vec4 bg=vec4(0),cut=vec4(0);
    if(hasImage!=0)bg=mix(pmSample(userImage,p*mediaAspect/min(mediaAspect.x,mediaAspect.y)+.5),pmSample(nextImage,p*nextMediaAspect/min(nextMediaAspect.x,nextMediaAspect.y)+.5),imageBlend);
    // Contain both dimensions; fitting only height crops wide sprites.
    vec2 fit=vec2((resolution.x/resolution.y)*cutoutAspect.x/cutoutAspect.y,1.);
    vec2 nextFit=vec2((resolution.x/resolution.y)*nextCutoutAspect.x/nextCutoutAspect.y,1.);
    if(hasCutout!=0)cut=mix(pmSample(cutoutImage,p*fit/min(fit.x,fit.y)+.5),pmSample(nextCutoutImage,p*nextFit/min(nextFit.x,nextFit.y)+.5),cutoutBlend);
    return cut+bg*(1.-cut.a);
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

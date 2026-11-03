#version 330
// RGBA is retained premultiplied pigment/coverage, never a final-image overlay.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float simulationReset,simulationTime,stepDelta;
uniform float bass,mid,treble,bassHit,trebleHit,motion,warp,feedback,imageMix,detail,pulse;
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

float lum(vec3 c){return dot(c,vec3(.2126,.7152,.0722));}
vec4 bounded(vec2 uv){return texture(input0,clamp(uv,vec2(.001),vec2(.999)))*step(0.,uv.x)*step(uv.x,1.)*step(0.,uv.y)*step(uv.y,1.);}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
 vec4 fresh=nestedMedia(uv);float l=lum(fresh.rgb);
 // Capture bright regions and coloured edges; bass opens darker image regions.
 float threshold=.14+.22*detail-.16*bass;
 float retained=lum(bounded(uv).rgb);
 float select=smoothstep(threshold-.06,threshold+.06,l-retained*.48)*fresh.a;
 // Bright lettering is admitted on strong attacks, not continually repainted.
 if(simulationReset<.5)select*=1.-smoothstep(.62+.38*bassHit,.78+.38*bassHit,l);
 // Capture colour bands into the pigment itself; subsequent motion follows them.
 vec3 pigment=.5+.5*cos(6.283185*(vec3(.03,.39,.72)+l*.65));
 pigment=mix(fresh.rgb,pigment*sqrt(l),.8);
 vec4 capture=vec4(pigment*select,select);
 if(simulationReset>.5){
  if(hasImage==0&&hasCutout==0){float ring=exp(-100.*abs(length((uv-.5)*vec2(1.7778,1.))-.24));capture=vec4(vec3(.15,.55,.85)*ring,ring);}
  fragColor=capture*imageMix;return;
 }
 vec4 local=bounded(uv);
 vec3 rgb=local.rgb/max(.08,local.a);
 vec2 gradient=vec2(lum(bounded(uv+vec2(px.x,0)).rgb)-lum(bounded(uv-vec2(px.x,0)).rgb),lum(bounded(uv+vec2(0,px.y)).rgb)-lum(bounded(uv-vec2(0,px.y)).rgb));
 // The deposited colour steers subsequent samples. Spatial shear, not a beat swing.
 vec2 velocity=vec2(rgb.r-rgb.b,rgb.g-rgb.r)*(45.+115.*warp)*( .35+motion);
 velocity+=vec2(-gradient.y,gradient.x)*(20.+60.*trebleHit*pulse);
 velocity+=vec2((uv.y-.5)*32.,(uv.x-.5)*-18.)*(.2+mid);
 velocity+=vec2(-gradient.y,gradient.x)*(45.+90.*mid);
 velocity+=vec2(-(uv.y-.5),uv.x-.5)*10.*lum(rgb)*( .35+motion);
 vec2 back=uv-velocity*stepDelta*px;
 vec4 old=bounded(back);
 // Finite pigment lifetime prevents a faint, ever-growing blanket of old colour.
 float attack=pulse*max(bassHit,trebleHit);
 old*=exp(-stepDelta*(.12+(1.-feedback)*.35));
 // Selective replacement opens the dark image regions as new fragments arrive.
 old*=exp(-stepDelta*attack*5.*fresh.a*(1.-select));
 float gate=.06+2.*bassHit*pulse+3.*trebleHit*pulse;
 float replace=(1.-exp(-stepDelta*gate*imageMix))*select;
 vec4 colour=mix(old,capture,replace);
 fragColor=vec4(clamp(colour.rgb,0.,1.),clamp(colour.a,0.,1.));
}

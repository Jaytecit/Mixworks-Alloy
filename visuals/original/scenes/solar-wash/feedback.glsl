#version 330
// Retained colour steers a recursive wash; input1 is a slow blurred reservoir.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float simulationReset,simulationTime,stepDelta;
uniform float bass,mid,treble,bassHit,midHit,trebleHit,beatPulse,motion,warp,feedback,imageMix,pulse,focusX,focusY;
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
    vec2 p=(uv-.5)/(.90+.08*imageZoom);
    float a=(imageRotation-.5)*6.283185;p=mat2(cos(a),-sin(a),sin(a),cos(a))*p;
    vec4 bg=vec4(0),cut=vec4(0);
    if(hasImage!=0)bg=mix(pmSample(userImage,p*mediaAspect/min(mediaAspect.x,mediaAspect.y)+.5),pmSample(nextImage,p*nextMediaAspect/min(nextMediaAspect.x,nextMediaAspect.y)+.5),imageBlend);
    p*=vec2(resolution.x/resolution.y,1.);
    if(hasCutout!=0)cut=mix(pmSample(cutoutImage,p*cutoutAspect/cutoutAspect.y+.5),pmSample(nextCutoutImage,p*nextCutoutAspect/nextCutoutAspect.y+.5),cutoutBlend);
    return cut+bg*(1.-cut.a);
}

void main(){
 vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
 // A positive pressure pulse: attack/beat energy expands incoming material and
 // drives old pigment outward. It never reverses the whole feedback field.
 float push=pulse*clamp(1.25*bassHit+1.8*beatPulse,0.,1.);
 vec2 centre=mix(vec2(.5),vec2(focusX,focusY),.3),p=uv-centre;
 vec2 mediaUV=centre+p/(1.+.14*push);
 vec4 fresh=nestedMedia(mediaUV);float mask=smoothstep(.08,.72,dot(fresh.rgb,vec3(.2126,.7152,.0722)))*fresh.a;
 vec3 seed=mix(fresh.rgb,vec3(mask,mask*.24,mask*.04),.6);
 if(simulationReset>.5){
  if(hasImage==0&&hasCutout==0){float r=length((uv-.5)*vec2(1.7778,1.));mask=exp(-90.*abs(r-.25));seed=vec3(1.,.22,.035)*mask;}
  fragColor=vec4(seed*imageMix,mask*imageMix);return;
 }
 vec4 slow=texture(input1,uv);
 // Expanding image contours and nonlinear colour currents, with delayed memory.
 vec2 velocity=p*(14.+38.*bass+26.*bassHit*pulse);
 velocity+=structureCurrent(mediaUV)*55.;

 velocity+=vec2(slow.g-slow.b,slow.b-slow.r)*(10.+40.*warp);
 velocity+=vec2(-p.y,p.x)*(3.+12.*mid+22.*midHit);
 vec2 radial=p/max(.08,length(p));
 velocity+=radial*push*(28.+85.*mask)*fresh.a;
 vec2 back=uv-velocity*stepDelta*px*(.4+motion);
 vec4 old=texture(input0,clamp(back,0.,1.));
 // Delayed material recurs through a larger, rotated chart, forming real echoes.
 float turn=.07+.06*mid;
 vec2 echo=mat2(cos(turn),-sin(turn),sin(turn),cos(turn))*p*(.79+.08*slow.a);
 vec4 delay=texture(input1,clamp(centre+echo,0.,1.));
 delay.rgb=mix(delay.rgb,delay.brg,.16+.12*mid);
 // Do not immediately refill a new opening with yesterday's blurred pigment.
 vec4 next=mix(old,delay,stepDelta*(2.5+treble*2.)*(1.-.75*push));
 // Rotate around the neutral colour axis without averaging chroma to grey.
 float angle=stepDelta*.4*(.2+mid);vec3 axis=vec3(.577350269);
 next.rgb=next.rgb*cos(angle)+cross(axis,next.rgb)*sin(angle)+axis*dot(axis,next.rgb)*(1.-cos(angle));
 float peak=max(next.r,max(next.g,next.b));
 next.rgb*=1.+stepDelta*.18*(.65-peak)*clamp(next.a,0.,1.);
 float decay=.13+(1.-feedback)*.4;
 next*=exp(-stepDelta*decay);
 // Clear within the expanding image footprint, including its dark regions,
 // then deposit new sharp pigment into the same state (never an output overlay).
 next*=exp(-stepDelta*push*10.*fresh.a);
 float inject=(1.-exp(-stepDelta*(.12+mid*.25+push*24.+trebleHit*pulse*6.)))*imageMix*mask;
 next.rgb=mix(next.rgb,seed,inject);next.a=mix(next.a,mask,inject);
 fragColor=clamp(next,0.,1.);
}

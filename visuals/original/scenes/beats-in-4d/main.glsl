// This file is released under CC0 1.0 Universal (Public Domain Dedication).
// To the extent possible under law, MÃ¥rten RÃ¥nge has waived all copyright
// and related or neighboring rights to this work.
// See <https://creativecommons.org/publicdomain/zero/1.0/> for details.

// Alloy adaptation: GLSL built-in tanh replaces the unknown-licence approximation.
// Own PCM controller, media fitting and normalized controls; no SSF helpers/scripts.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
uniform vec2 resolution;
uniform float bass,bassHit,bassTime,onset,pulse,warp,scale,orbit,detail,imageMix;
uniform float projectBeat,mid,treble,midTime,trebleTime,midHit,trebleHit;
uniform sampler2D userImage;
uniform int hasImage;
out vec4 colour;
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

vec4 sceneMedia(){vec2 uv=gl_FragCoord.xy/resolution;vec4 bg=hasImage!=0?sampleFittedMedia(uv):vec4(0);vec4 fg=sampleCutout(uv);return vec4(nestedMaterial(uv,mix(bg.rgb,fg.rgb,fg.a),4.),max(bg.a,fg.a));}
mat2 rot(float a) {
  float
      c=cos(a)
    , s=sin(a)
    ;
  return mat2(c,s,-s,c);
}

void main() {
  float
    i
  , z=0.
  , d
  , k
  , B=.7*(pow(bass,3.)+pow(bassHit,3.))*pulse*2.
  , t=travelTime*.5+bassTime*1.5+bassImpulse*3.2
  , H=bassHit

  , T=.1*t
  ;
  vec4 media = sceneMedia();
  float lumMed = dot(media.rgb, vec3(1.0))/3.0;
  float mediaOn = media.a*imageMix;
  vec2
    C=gl_FragCoord.xy
  , r=resolution
  , U=2.*(gl_FragCoord.xy/resolution-.5)
  , S=sin(U*(8.+detail*42.)*mix(1.0,lumMed,mediaOn))
  ;
  vec3
    I=normalize(vec3(C-.5*r,(1.+H*(warp*.15)*dot(U,U)*S.x*S.y)*r.y))
  , o=vec3(0)
  ;
  vec4
    p
  , P
  , M
  ;

  mat2
    R0=rot(T)
  , R1=rot(t*.1*(.3+orbit*1.4)+midImpulse*1.4+midTime*.2)
  , R2=rot(t*.1+trebleImpulse*1.6+trebleTime*.3)
  ;

  for(i=0.;i<79.;++i) {
    p=vec4(z*I,.05+scale*.30),

    p.z-=3.;

    p.xw*=R0;
    p.yw*=R1;
    p.zw*=R2;

    p*=k=9./max(dot(p,p),1.e-5);

    P=p-=.5*t;

    p=abs(p-round(p));

    d=abs(
     min(
       min(
           sqrt(min(min(dot(p.xz,p.xz),dot(p.yz,p.yz)), dot(p.xy,p.xy)))+(1.-1.)*.2
         , length(p)-.2-(onset*warp)*.2+(1.-1.)*.2
         )
       , 1.*min(p.w-(bassHit*warp)*.2*length((gl_FragCoord.xy/resolution-.5)),min(p.x,min(p.z,p.y)))+.05)
       )/k;

    if (mediaOn>0.001){
      vec4 img = sceneMedia();
      p=1.+sin(P.z+log2(k)+mix(vec4(0,1,2,0),img*3.,mediaOn));
    } else{
      p=1.+sin(P.z+log2(k)+vec4(0,1,2,0));
    }
    o+=.3*exp(.6*k+6.*B-6.)*1.*vec3(.3333,.6666,1.);
    o+=.8*p.w/max(d,1e-3)*p.xyz;
    z+=.8*d+1e-3;
  }

  o=clamp(o/1e4,0.,1.);
  o=tanh(o)/0.9;
  o=mix(o,vec3(0),isnan(o));

  colour=vec4(o,1);
}

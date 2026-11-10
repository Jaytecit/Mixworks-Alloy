#version 330
uniform sampler2D userImage;
uniform float bassImpulse,midImpulse,trebleImpulse;
#define FAMILY 6
// One texel per persistent fragment: position.xy, velocity.xy in screen UV.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float simulationReset,simulationTime,stepDelta,projectBeat;
uniform float bass,mid,treble,bassHit,midHit,trebleHit,motion,warp,feedback,pulse,mode,focusX,focusY;
uniform float generationBlend;
out vec4 fragColor;
const float TAU=6.2831853;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec2 turn(vec2 p,float a){return mat2(cos(a),sin(a),-sin(a),cos(a))*p;}
vec2 safe(vec2 p){return p/max(.025,length(p));}
vec4 old(vec2 cell){return texelFetch(input0,clamp(ivec2(cell),ivec2(0),ivec2(resolution)-1),0);}
float light(vec2 uv){vec4 c=texture(input1,clamp(uv,0.,1.));return dot(c.rgb,vec3(.2126,.7152,.0722));}
void main(){
 vec2 cell=floor(gl_FragCoord.xy),home=(cell+.5)/resolution,h=home-.5;
 if(simulationReset>.5){fragColor=vec4(home,0,0);return;}
 vec4 state=old(cell),ink=texture(input1,home);
 vec2 p=state.xy,v=state.zw,q=p-vec2(focusX,focusY),rad=safe(h),tangent=vec2(-rad.y,rad.x);
 float seed=hash(cell),a=atan(h.y,h.x),r=length(h),t=simulationTime*(.16+motion*.5)+bassImpulse*.85+midImpulse*1.8+trebleImpulse*.45,b=bass,hit=bassHit*pulse*2.8;
 float m=mid,hi=trebleHit*pulse*2.2,d=.04+.22*warp;
 int kind=int(floor(mode*3.+.5));
 vec3 colour=ink.rgb/max(.01,ink.a);
 vec2 grad=vec2(light(home+vec2(.006,0))-light(home-vec2(.006,0)),light(home+vec2(0,.006))-light(home-vec2(0,.006)));
 float edge=clamp(length(grad)*5.,0.,1.);
 vec2 target=home,force=vec2(0);float spring=2.8,drag=2.3-1.4*feedback;
#if FAMILY == 0
 // A physical break, boundary peeling, spectral sorting, and independent recall.
 if(kind==0){force=rad*hit*(2.+3.*seed)*d;spring=.9+3.*m;}
 if(kind==1){target=home+safe(grad+rad*.03)*edge*d*(.2+b);force=tangent*hi*edge;spring=3.;}
 if(kind==2){target=home+vec2(colour.r-colour.b,colour.g-(colour.r+colour.b)*.5)*d*(.4+2.*b);force=rad*hit*d;}
 if(kind==3){float release=smoothstep(.15,.5,b);target=mix(home,vec2(hash(cell+7.),hash(cell+13.))*.8+.1,release*warp);spring=1.5+5.*m;force=rad*hit*d;}
#elif FAMILY == 1
 if(kind==0){vec2 pole=vec2(.5+(colour.r>colour.b?-.22:.22),.5);target=mix(home,pole+turn(h,t*(colour.r>colour.b?1.:-1.))*.3,.35+.4*b);force=safe(pole-p)*hit*d;}
 if(kind==1){target=home+vec2(-grad.y,grad.x)*d*1.2;force=vec2(-grad.y,grad.x)*(.1+.65*m)+safe(grad)*hi*.15;spring=2.2;}
 if(kind==2){float signPole=mod(cell.x+cell.y,2.)*2.-1.;target=home+vec2(signPole*d*(b+.2),signPole*sin(home.x*TAU)*d*m);force=vec2(signPole*hit,hi*edge)*d;}
 if(kind==3){target=vec2(mix(home.x,(floor(home.x*7.)+.5)/7.,.6+.3*b),home.y+sin(home.x*42.+t)*d*m);force=vec2(hi*(colour.r-colour.b),hit*(seed-.5))*d;}
#elif FAMILY == 2
 spring=1.2;
 if(kind==0){target=.5+turn(h,t*(.2+m))*(.8+.4*b);force=vec2(-q.y,q.x)*(.5+m)+rad*hit*d;}
 if(kind==1){vec2 centre=vec2(home.x<.5?.3:.7,.5);vec2 local=home-centre;target=centre+turn(local,t*(home.x<.5?1.:-1.))*(.8+.3*b);force=safe(target-p)*hi*d;}
 if(kind==2){vec2 centre=(floor(home*vec2(5,3))+.5)/vec2(5,3);target=centre+turn(home-centre,t*(1.+colour.r*2.))*(1.+b);force=safe(p-centre)*hit*d;}
 if(kind==3){target=home+vec2(sin(t+colour.b*TAU+home.y*8.),cos(t*.8+colour.r*TAU+home.x*8.))*d*(.3+m);force=vec2(0,(colour.r-colour.b)*b*.5)+tangent*hi*d;}
#elif FAMILY == 3
 spring=5.;
 if(kind==0){float strand=mod(cell.y,2.)*2.-1.;target=home+vec2(strand*d*b,sin(home.x*TAU*2.+t)*d*m);force=vec2(hit*strand,hi*edge)*d;}
 if(kind==1){float side=home.x<.5?-1.:1.;float opening=smoothstep(.1,.9,home.y+b*.5);target=home+vec2(side*d*b*opening,0);force=vec2(side*hit*opening,hi*(seed-.5))*d;}
 if(kind==2){float fold=sin(home.x*TAU*6.);target=home+vec2(fold*d*b*.35,fold*d*(m+.15));force=vec2(0,fold*hit*d);}
 if(kind==3){float strand=mod(cell.x+cell.y,3.)-1.;target=home+vec2(1,-1)*strand*d*sin(home.x*9.+home.y*9.+t)*(m+.25);force=vec2(1,1)*hit*strand*d;}
#elif FAMILY == 4
 // True neighbour state coupling, with displacement-relative springs (no collapse).
 vec2 neighbour=(old(cell+vec2(1,0)).xy-(clamp(cell+vec2(1,0),vec2(0),resolution-1.)+.5)/resolution
                +old(cell-vec2(1,0)).xy-(clamp(cell-vec2(1,0),vec2(0),resolution-1.)+.5)/resolution
                +old(cell+vec2(0,1)).xy-(clamp(cell+vec2(0,1),vec2(0),resolution-1.)+.5)/resolution
                +old(cell-vec2(0,1)).xy-(clamp(cell-vec2(0,1),vec2(0),resolution-1.)+.5)/resolution)*.25;
 if(kind==0){force=(neighbour-(p-home))*(8.+16.*m)+rad*hit*d*(.2+edge*3.);spring=1.2;}
 if(kind==1){float front=exp(-80.*pow(home.x-fract(t*.22),2.));force=vec2(0,(seed-.5)*front)*(1.+hit*5.)*d+(neighbour-(p-home))*4.;spring=1.5;}
 if(kind==2){vec2 nv=(old(cell+vec2(1,0)).zw+old(cell-vec2(1,0)).zw+old(cell+vec2(0,1)).zw+old(cell-vec2(0,1)).zw)*.25;force=(nv-v)*(3.+m*8.)+vec2(colour.r-colour.b,colour.g-.5)*hit*d*4.;spring=.7;}
 if(kind==3){vec2 cluster=floor(home*vec2(4,3));vec2 centre=(cluster+.5)/vec2(4,3);target=centre+turn(home-centre,t*(mod(cluster.x+cluster.y,2.)*2.-1.))*(1.+2.*b);force=(neighbour-(p-home))*2.+rad*hi*d;}
#elif FAMILY == 5
 spring=3.;
 if(kind==0){target=.5+turn(h,t/(.3+2.*r))*(.8+.5*b);force=rad*hit*d;}
 if(kind==1){float angle=a+t*(.3+m);target=.5+vec2(cos(angle),sin(angle))*r*(.8+.28*cos(angle*5.)+.3*b);force=tangent*hi*d;}
 if(kind==2){vec2 centre=vec2(.5+sin(t)*.18*(home.x<.5?-1.:1.),.5);target=centre+turn(h*.55,t*(home.x<.5?1.:-1.))*(.8+b);force=rad*hit*d;}
 if(kind==3){target=.5+turn(turn(h,t)*vec2(1.,.4+.4*m),t*.23)*(1.+b*.3);force=tangent*hi*d;}
#elif FAMILY == 6
 spring=1.6;
 if(kind==0){target=home+vec2(sin(home.y*9.+t+home.x*5.)*d*m,-d*b*(.4+colour.r));force=vec2(0,-hit*d*(.4+seed));}
 if(kind==1){target=home+vec2(sin(t*2.+home.y*12.)*d*(.1+m),d*b*(.4+edge));force=vec2(hi*(seed-.5),hit)*d;}
 if(kind==2){vec2 hinge=vec2(home.x,.9);target=hinge+turn(home-hinge,sin(t*(.8+home.x)+home.x*8.)*d*(.3+b));force=vec2(hit*(seed-.5),0)*d;}
 if(kind==3){target=home+safe(vec2(-grad.y,grad.x)+vec2(.01,0))*edge*d*(.5+b);force=vec2(-grad.y,grad.x)*hi*2.;spring=.8+3.*m;}
#elif FAMILY == 7
 spring=4.;float shell=floor(r*9.)/9.;
 if(kind==0){float wave=sin(r*24.-t*4.);target=.5+rad*(r+wave*d*b);force=rad*hit*d*(.5+seed);}
 if(kind==1){target=.5+turn(h,t*(.4+m)+r*4.)*(.7+.5*b);force=tangent*(.2+m)*d+rad*hit*d;}
 if(kind==2){float teeth=sin(a*12.);target=.5+turn(rad,t*(mod(floor(r*9.),2.)*2.-1.))*(r+teeth*d*b*.5);force=tangent*hi*d;}
 if(kind==3){float band=mod(floor(r*9.),2.)*2.-1.;target=.5+turn(rad,t*.4)*(r+band*d*b);force=rad*band*hit*d;}
#else
 spring=.8;
 if(kind==0){force=rad*hit*d*(3.+seed*4.);spring=.65+3.*m;target=home;}
 if(kind==1){vec2 well=vec2(colour.r>colour.b?.28:.72,.5);vec2 delta=well-p;force=safe(delta)*(.03+.12*b)/( .1+dot(delta,delta)*8.)+vec2(-delta.y,delta.x)*(.2+m);spring=.35;}
 if(kind==2){vec2 direction=safe(vec2(colour.r-colour.b+.3,colour.g-.5));target=home+direction*d*(.3+b);force=direction*hit*d*4.+vec2(-direction.y,direction.x)*hi*d;}
 if(kind==3){target=.5+turn(h,t/(.2+r*3.))*vec2(1.,.35+.4*m);force=-safe(q)*b*.06+vec2(-q.y,q.x)*hit*d;}
#endif
 // Midrange drives independent circulation, not global side-to-side travel.
 force+=tangent*midHit*d*(.7+edge)*1.8;
 // Band-driven vector field moves paths, not just fragment sizes.
 vec2 stream=vec2(sin(home.y*8.+midImpulse*2.),cos(home.x*7.-trebleImpulse*2.));
 target+=stream*d*(.22+.65*m)+tangent*d*b*.7;
 force+=stream*(midHit*.9+trebleHit*.5)*pulse;
 // Keep the physical flock moving; the display crosses its source plane.

 // Source-derived edge/colour forces and persistent damping; no frame history blur.
 force+=safe(grad+vec2(.001))*hi*edge*d*.6;
 float dt=stepDelta*(.35+1.3*motion);
 force+=structureCurrent(home)*1.4;
 v=(v+(spring*(target-p)+force)*dt*(.15+.85*generationBlend))*exp(-drag*dt);
 float speed=length(v);v*=min(1.,.8/max(.001,speed));p+=v*dt;
 // Reflect instead of silently losing fragments at the boundary.
 for(int axis=0;axis<2;axis++){
  if(p[axis]<.025){p[axis]=.025;v[axis]=abs(v[axis])*.65;}
  if(p[axis]>.975){p[axis]=.975;v[axis]=-abs(v[axis])*.65;}
 }
 fragColor=vec4(p,v);
}

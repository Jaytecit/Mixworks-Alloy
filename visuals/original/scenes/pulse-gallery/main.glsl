#version 330
#define STAGE 0
// Original solid instruments around a single intact image. STAGE selects topology.
uniform vec2 resolution;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
uniform float bassTime,midTime,trebleTime,pulse,scale,warp,detail,hue,imageMix,intensity;
out vec4 colour;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 palette(float t){return .12+.8*(.5+.5*cos(vec3(0.,2.1,4.2)+t+hue*6.283));}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,aspect=vec2(resolution.x/resolution.y,1.);
 vec2 p=(uv-.5)*2.*aspect;
 float low=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse;
 vec3 c=vec3(.012,.018,.035);float depth=0.;
 // Project thick solid frames through depth. Band impulses advance continuously.
 for(int i=11;i>=0;i--){
  float count=6.+floor(detail*6.);if(float(i)>=count)continue;
  float z=fract(float(i)/count+bassImpulse*.085+bassTime*.025+travelTime*.025);
  float size=.30+z*(2.+scale*1.5);
  vec2 q=p;
#if STAGE == 0
  q=turn(midImpulse*.3+float(i)*.16)*q;
  float edge=max(abs(q.x)/(1.1+mi*.4),abs(q.y));
#elif STAGE == 1
  q.x-=sin(z*5.+midImpulse)*(.25+mi*.35);
  q=turn(midImpulse*.45+z*1.8)*q;
  float edge=length(q*vec2(1.,.55+.35*sin(z*3.+trebleImpulse)));
#else
  q=turn(-midImpulse*.3+z)*q;
  float angle=atan(q.y,q.x);
  float edge=length(q)*(1.+(.08+mi*.18)*cos(angle*6.+trebleImpulse));
#endif
  float d=abs(edge-size*(1.+low*.22));
  float width=.025+z*.045+hi*.028;
  float face=1.-smoothstep(width,width+.012,d);
  float bevel=.35+.65*(1.-smoothstep(0.,width,d));
  c=mix(c,palette(z*4.+float(i)*.25)*bevel,face*(.25+.7*z));
  c+=palette(z*5.)*exp(-d*45.)*(.018+hi*.055);
 }
 // All corners remain in the image chart. Music moves the complete plane.
 vec2 chart=uv-.5;
#if STAGE == 0
 chart/= .76+low*.16;
 chart.x+=mi*(.015+warp*.025)*sin(chart.y*5.+midImpulse);
#elif STAGE == 1
 chart=turn((.06+warp*.1)*sin(midImpulse)*mi)*chart;
 chart/= .72+low*.18;
 chart+=vec2(.035*sin(midImpulse),.025*cos(trebleImpulse))*mi;
#else
 chart/= .74+low*.17;
 chart*=1.+hi*(.04+warp*.08)*sin(length(chart)*9.-trebleImpulse*3.);
#endif
 chart+=.5;
 vec4 art=hasImage!=0?sampleFittedMedia(chart):vec4(0);
 vec4 cut=sampleCutout(chart);
 art=vec4(mix(art.rgb,cut.rgb,cut.a),max(art.a,cut.a));
 // The image is readable; the surrounding instrument provides different motion.
 float opacity=art.a*min(1.,imageMix*1.4);
 c=mix(c,art.rgb*(.94+.06*intensity),opacity);
 colour=vec4(clamp(c,0.,1.),1.);
}

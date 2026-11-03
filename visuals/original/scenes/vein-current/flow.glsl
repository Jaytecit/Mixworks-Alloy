#version 330
// Original advected velocity/pigment system. XY = signed pixels/second,
// Z = pigment, W = signed relief. Never interpret this as display RGB.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset, simulationTime, stepDelta;
uniform float bass, mid, treble, bassHit, trebleHit, motion, feedback, scale, pulse;
out vec4 fragColor;
vec4 media(vec2 uv) {
    vec4 c=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 s=sampleCutout(uv);return vec4(mix(c.rgb,s.rgb,s.a),max(c.a,s.a));
}
float ink(vec2 uv) {vec4 c=media(uv);return dot(c.rgb,vec3(.2126,.7152,.0722))*c.a;}
vec2 field(vec2 p,float t) {
    // Curl of two smooth stream functions: locally circulating, no clock reversal.
    float k=14.+scale*12.;
    vec2 v=vec2(k*sin(k*p.x+t*.19)*cos(k*p.y-t*.13),
               -k*cos(k*p.x+t*.19)*sin(k*p.y-t*.13));
    v+=.42*vec2(37.*sin(31.*p.x-37.*p.y+t*.31),31.*sin(31.*p.x-37.*p.y+t*.31));
    v+=.24*vec2(73.*sin(61.*p.x+t*.27)*cos(73.*p.y),-61.*cos(61.*p.x+t*.27)*sin(73.*p.y));
    return v;
}
void main() {
    vec2 uv=gl_FragCoord.xy/resolution, px=1./resolution;
    vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
    float seed=ink(uv);
    vec2 ig=vec2(ink(uv+vec2(px.x,0))-ink(uv-vec2(px.x,0)),ink(uv+vec2(0,px.y))-ink(uv-vec2(0,px.y)));
    float pattern=.5+.25*sin(p.x*83.+2.*sin(p.y*39.))+.25*cos(p.y*71.-p.x*31.);
    if(simulationReset>.5) {
        fragColor=vec4(field(p,0.)*.25+vec2(-ig.y,ig.x)*feedback*18.,mix(pattern,seed,feedback*.85),pattern*.4+seed*feedback);return;
    }
    float dt=stepDelta;
    vec4 old=texture(input0,uv);
    vec2 back=uv-old.xy*dt*px;
    vec4 c=texture(input0,back);
    vec4 l=texture(input0,back-vec2(px.x,0)),r=texture(input0,back+vec2(px.x,0));
    vec4 b=texture(input0,back-vec2(0,px.y)),t=texture(input0,back+vec2(0,px.y));
    vec4 lap=l+r+b+t-4.*c;
    vec2 slope=vec2(r.w-l.w,t.w-b.w)*.5;
    // Sustained bass maintains shear; impulses add local vorticity with inertia.
    vec2 drive=field(p,simulationTime)*(.65+motion)*(1.+3.5*bass+2.5*bassHit*pulse);
    vec2 imageForce=vec2(-ig.y,ig.x)*feedback*(22.+45.*mid);
    vec2 fine=vec2(-slope.y,slope.x)*(28.+110.*trebleHit*pulse);
    vec2 v=c.xy+dt*(drive+imageForce+fine-c.xy*(.5+.8*(1.-mid))+lap.xy*5.);
    v=clamp(v,vec2(-80),vec2(80));
    v*=smoothstep(vec2(0),px*5.,uv)*smoothstep(vec2(0),px*5.,1.-uv);
    float dye=c.z+dt*(lap.z*1.7+(seed-c.z)*feedback*(.1+.32*mid));
    float relief=c.w+dt*(lap.w*.35+(dye-c.w)*.35+.10*(r.y-l.y-t.x+b.x));
    fragColor=vec4(v,clamp(dye,0.,1.),clamp(relief,-2.,2.));
}

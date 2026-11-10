// Original Mixworks spatial Prism Bloom, 2026-11-02.
// Authored from analytic surfaces; no imported shader, helper or texture code.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
uniform vec2 resolution, imageSize;
uniform float sceneTime, projectBeat, beatPulse, energy, bass, mid, treble, onset, intensity, motion, imageMix;
uniform sampler2D userImage;
uniform int hasImage;
uniform float bassHit,midHit,trebleHit,bassTime,midTime,trebleTime;
out vec4 colour;
mat2 artTurn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
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

vec3 imagePlane(vec2 uv){
    vec2 p=artTurn((imageRotation-.5)*6.283185)*(uv-.5)/(1.+imageZoom*.25);
    p.x/=resolution.x/resolution.y;
    vec2 a=1.-abs(mod(p*mediaAspect+.5,2.)-1.);
    vec2 b=1.-abs(mod(p*nextMediaAspect+.5,2.)-1.);
    return nestedMaterial(uv,mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend),2.0);
}
vec4 alphaPixel(sampler2D tex,ivec2 p,ivec2 size){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);vec4 c=texelFetch(tex,p,0);return vec4(linearRGB(c.rgb)*c.a,c.a);}
vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){ivec2 s=textureSize(tex,0);vec2 p=((uv-.5)*aspect+.5)*vec2(s)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);return mix(mix(alphaPixel(tex,i,s),alphaPixel(tex,i+ivec2(1,0),s),f.x),mix(alphaPixel(tex,i+ivec2(0,1),s),alphaPixel(tex,i+ivec2(1),s),f.x),f.y);}
vec4 cutoutArt(vec2 uv){if(hasCutout==0)return vec4(0);vec4 c=mix(alphaArt(cutoutImage,uv,cutoutAspect),alphaArt(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);return vec4(c.rgb/max(c.a,.00001),c.a);}

float phase;
mat2 turn(float a) { return mat2(cos(a),-sin(a),sin(a),cos(a)); }
vec3 localPoint(vec3 p) {
    p.xz = turn(phase*.31+bassImpulse*1.9)*p.xz;
    p.xy = turn(.22*sin(phase*.6)+midImpulse*1.6)*p.xy;
    return p;
}
// A bounded cellular shell surrounding a separately rotating faceted heart.
// Conservative steps through the periodic implicit field prevent hidden faces
// from drawing over foreground surfaces. All lighting uses the hit normal.
vec2 surface(vec3 p) {
    p.y-=.42*structureHeight(p.xz*.23+.5);

    vec3 q=localPoint(p);
    float r=length(q);
    float frequency=4.8+.28*sin(phase*.47);
    vec3 cell=q*frequency;
    float weave=dot(sin(cell),cos(cell.zxy));
    float lattice=abs(weave)/(frequency*3.)-(.038+.009*bass);
    float shell=abs(r-1.62)-.21;
    float cut=dot(q,normalize(vec3(.6,.3,1.)))-1.20;
    float outer=max(max(lattice,shell),cut);
    vec3 inner=q;
    inner.xy=turn(-phase*.7-midImpulse*2.3)*inner.xy;
    inner.yz=turn(.5+phase*.27+bassImpulse*.8)*inner.yz;
    float facet=max(max(abs(inner.x),abs(inner.y)),abs(inner.z));
    facet=max(facet,(abs(inner.x)+abs(inner.y)+abs(inner.z))*.59);
    float heart=(facet-.66-.10*bassHit)*.7;
    // An inner filament layer appears through the outer cells, with its own motion.
    vec3 f=q*7.2;
    f.xy=turn(phase*.13+trebleTime*2.+trebleImpulse*2.3)*f.xy;
    float filigree=max(abs(length(q)-1.12)-.055,
        abs(dot(sin(f+phase*.12),cos(f.yzx)))/21.6-.015);
    vec2 result=outer<heart?vec2(outer,0.):vec2(heart,1.);
    return filigree<result.x?vec2(filigree,2.):result;
}
vec3 normalAt(vec3 p) {
    const float e=.0018;
    return normalize(vec3(surface(p+vec3(e,0,0)).x-surface(p-vec3(e,0,0)).x,
        surface(p+vec3(0,e,0)).x-surface(p-vec3(0,e,0)).x,
        surface(p+vec3(0,0,e)).x-surface(p-vec3(0,0,e)).x));
}
vec3 environment(vec3 ray) {
    float horizon=pow(max(0.,1.-abs(ray.y)),6.);
    vec3 c=mix(vec3(.002,.004,.009),vec3(.012,.025,.045),horizon);
    // Long studio lights describe the curved surfaces and their moving normals.
    c+=vec3(.6,2.5,3.2)*pow(max(0.,dot(ray,normalize(vec3(-1,.7,1)))),48.);
    c+=vec3(3.5,1.3,.28)*pow(max(0.,dot(ray,normalize(vec3(1,.2,-.6)))),66.);
    c+=vec3(.64,.20,1.2)*pow(max(0.,dot(ray,normalize(vec3(.2,-.9,.2)))),32.);
    c+=vec3(2.4,2.7,2.8)*exp(-pow((ray.x+.34)/.035,2.))*pow(max(0.,ray.y),3.);
    return c;
}
vec3 material(vec3 p,vec3 n,vec3 rd,float id) {
    vec3 q=localPoint(p);
    float facing=max(0.,dot(n,-rd));
    float fresnel=pow(1.-facing,4.);
    float bands=.5+.5*sin(length(q*vec3(1.,1.3,.8))*45.+sin(q.y*13.+phase)*1.4-phase*.8);
    float veins=pow(bands,14.);
    vec3 tint=mix(vec3(.008,.035,.05),vec3(.025,.13,.12),.5+.5*sin(q.y*3.-q.x*2.+phase*.2));
    bool lip=id<.5 && abs(length(q)-1.62)>.204;
    if(lip) tint=vec3(.23,.115,.035);
    if(id>.5 && id<1.5) tint=vec3(.42,.12,.045);
    if(id>1.5) tint=vec3(.04,.23,.32);
    if(hasImage!=0) {
        vec2 uv=vec2(atan(q.z,q.x)/6.28318+.5,asin(clamp(q.y/max(length(q),.001),-1.,1.))/3.14159+.5);
        tint=mix(tint,mix(linearRGB(textureLod(userImage,uv,0.).rgb),linearRGB(textureLod(nextImage,uv,0.).rgb),imageBlend),imageMix);
    }
    float ao=1.;
    for(int i=1;i<=4;i++) {
        float d=float(i)*.065;
        ao-=max(0.,d-surface(p+n*d).x)*(.5/float(i));
    }
    ao=clamp(ao,.22,1.);
    vec3 key=normalize(vec3(-.7,1.2,1.8));
    float diffuse=max(0.,dot(n,key));
    float shadow=1., travel=.045;
    for(int i=0;i<12;i++) {
        float h=surface(p+n*.008+key*travel).x;
        shadow=min(shadow,10.*max(h,0.)/travel);
        travel+=clamp(h,.025,.15);
    }
    vec3 reflected=environment(reflect(rd,n));
    // A second bent environment ray gives coloured glass-like transmission.
    // This is an artistic approximation, not a physical refractive path trace.
    vec3 transmitted=environment(refract(rd,n,.72)+vec3(.07*sin(q.y*9.),0,0));
    vec3 c=tint*(.22+diffuse*.9*clamp(shadow,.18,1.))*ao;
    c+=reflected*(.66+.85*fresnel)*ao;
    c+=transmitted*tint*(.5+.25*mid)*(1.-fresnel);
    c+=vec3(.12,.64,.74)*veins*(.035+.12*treble)*ao;
    if(lip) c+=vec3(.3,.12,.025)*(.12+.12*treble);
    if(id>.5 && id<1.5) c+=vec3(1.2,.24,.025)*(.19+.36*beatPulse+onset*.12);
    if(id>1.5) c+=vec3(.02,.4,.62)*(.22+.32*mid);
    return c*(.75+.5*intensity);
}
void main() {
    phase=sceneTime*(.19+.26*motion);
    // Preview-only rhythmic camera surges: retain the original surface/material.
    float punch=clamp(max(bassHit,pow(1.-fract(projectBeat),3.)*bass*1.35),0.,1.);
    float snap=floor(projectBeat*.5)*.55;
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    uv=turn(.20*punch*sin(floor(projectBeat)*1.570796))*uv;
    float orbit=.25*sin(phase*.23)+phase*.14+snap;
    float radius=4.6-2.20*punch;
    vec3 eye=vec3(radius*sin(orbit),.45+.5*sin(phase*.19),radius*cos(orbit));
    vec3 aim=vec3(0,.08*sin(phase*.4),0);
    vec3 forward=normalize(aim-eye);
    vec3 right=normalize(cross(forward,vec3(0,1,0)));
    vec3 up=cross(right,forward);
    vec3 ray=normalize(forward*(2.05+.15*punch)+uv.x*right+uv.y*up);
    vec3 c=environment(ray)*.20;
    // A subdued, three-dimensional dust field sits behind the sculpture.
    vec3 starRay=ray; starRay.xz=turn(.04*phase)*starRay.xz;
    vec2 sky=vec2(atan(starRay.z,starRay.x),asin(starRay.y))*95.;
    vec2 tile=floor(sky);
    float hash=fract(sin(dot(tile,vec2(17.17,91.73)))*4317.13);
    float star=exp(-dot(fract(sky)-.5,fract(sky)-.5)*240.)*step(.965,hash);
    c+=vec3(.12,.22,.29)*star;
    float distance=0.; vec2 hit=vec2(1.,0.);
    bool found=false;
    // Skip empty space outside the known bounding sphere.
    float b=dot(eye,ray), disc=b*b-dot(eye,eye)+3.61;
    if(disc>0.) {
        distance=max(0.,-b-sqrt(disc));
        float end=-b+sqrt(disc);
        for(int i=0;i<144;i++) {
            hit=surface(eye+ray*distance);
            if(hit.x<.0013) { found=true; break; }
            distance+=max(.0007,hit.x*.8);
            if(distance>end) break;
        }
    }
    if(found) {
        vec3 p=eye+ray*distance;
        c=material(p,normalAt(p),ray,hit.y);
        c=mix(c,vec3(.015,.035,.055),1.-exp(-.025*distance));
    }
    c*=1.-.18*smoothstep(.3,2.,length(uv));
    colour=vec4(max(c,vec3(0)),1);
}

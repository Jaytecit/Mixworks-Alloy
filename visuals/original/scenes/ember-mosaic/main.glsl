// Mixworks Ember Mosaic, authored 2026-11-02.
// Study of cellular mineral relief after inspecting Voronoi Geode's structure.
// Independently authored field/material implementation; no bundled source or assets copied.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
uniform vec2 resolution;
uniform float sceneTime, audioMotion, beatPulse, projectBeat, energy, bass, mid, treble, onset, intensity, motion, imageMix;
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
    return nestedMaterial(uv,mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend),6.0);
}
vec4 alphaPixel(sampler2D tex,ivec2 p,ivec2 size){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);vec4 c=texelFetch(tex,p,0);return vec4(linearRGB(c.rgb)*c.a,c.a);}
vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){ivec2 s=textureSize(tex,0);vec2 p=((uv-.5)*aspect+.5)*vec2(s)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);return mix(mix(alphaPixel(tex,i,s),alphaPixel(tex,i+ivec2(1,0),s),f.x),mix(alphaPixel(tex,i+ivec2(0,1),s),alphaPixel(tex,i+ivec2(1),s),f.x),f.y);}
vec4 cutoutArt(vec2 uv){if(hasCutout==0)return vec4(0);vec4 c=mix(alphaArt(cutoutImage,uv,cutoutAspect),alphaArt(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);return vec4(c.rgb/max(c.a,.00001),c.a);}

float clock;
mat2 rotate(float a) { return mat2(cos(a),-sin(a),sin(a),cos(a)); }
vec2 seedAt(ivec2 cell) {
    uvec2 v=uvec2(cell)*uvec2(1664525u,1013904223u)+uvec2(73621u,94711u);
    v^=v.yx>>11u; v*=uvec2(2246822519u,3266489917u); v^=v>>15u;
    vec2 phase=vec2(v&65535u)/65535.*6.283185;
    return vec2(.5)+.34*sin(phase+clock*vec2(.91,1.13)+vec2(bassImpulse*2.8,midTime*2.));
}
// Nearest/next-nearest squared distances create polygonal faces. Retain the
// closest site's coordinate for a stable per-cell mineral pattern.
vec4 cellField(vec2 p) {
    vec2 scaled=p*3.25; ivec2 cell=ivec2(floor(scaled));
    float first=100.,second=100.; vec2 winner=vec2(0);
    for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++) {
        ivec2 id=cell+ivec2(x,y);
        vec2 point=vec2(id)+seedAt(id), delta=point-scaled;
        float d=dot(delta,delta);
        if(d<first) { second=first; first=d; winner=point; }
        else second=min(second,d);
    }
    return vec4(max(0.,second-first),first,winner);
}
float heightAt(vec2 p) {
    float gap=cellField(p).x;
    float crystal=.035+.185*min(gap,1.);
    float frame=.145+.025*smoothstep(0.,.045,gap);
    return mix(frame,crystal,smoothstep(.075,.14,gap));
}
vec3 mineral(vec2 p,vec2 site) {
    vec2 q=p*9.+site*.41;
    q+=vec2(sin(q.y*1.1),cos(q.x*.87))*.72;
    float bands=.5+.5*sin(q.x*3.+q.y+2.*sin(q.y*.83)+sin(q.x*1.6));
    float fine=.5+.5*sin(q.x*19.+q.y*4.+3.*sin(q.y*2.));
    vec3 ink=mix(vec3(.012,.0007,.002),vec3(.48,.009,.003),pow(bands,2.2));
    ink+=vec3(.62,.14,.018)*pow(bands,17.)*(.35+.65*fine);
    ink*=.65+.45*fine;
    return ink;
}
vec3 sky(vec3 r,vec2 p) {
    float wave=sin(r.x*8.+r.z*3.+sceneTime*.28)+cos(r.y*7.-r.z*4.-sceneTime*.37);
    float cold=pow(smoothstep(.1,1.8,wave),3.);
    float warm=pow(smoothstep(.1,1.8,-wave),5.);
    vec3 c=vec3(.06,.63,.82)*cold+vec3(.75,.06,.017)*warm;
    c+=vec3(.65,.8,1.)*pow(max(0.,dot(r,normalize(vec3(-.2,.5,1.)))),34.);
    return c*(.25+energy*.8+onset*.45);
}
void main() {
    clock=sceneTime*(.13+.2*motion)+audioMotion*(1.+motion);
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    uv=rotate(midImpulse*.14)*uv;
    vec3 eye=vec3(.10*sin(bassImpulse*.5),.07*sin(trebleImpulse*.6),2.2);
    vec3 ray=normalize(vec3(uv,-2.65));
    // Intersect the thin bounded relief slab. Fourteen subdivisions converge to
    // subpixel depth for this front-facing camera without a full-volume march.
    float nearT=(eye.z-.25)/-ray.z, farT=eye.z/-ray.z;
    for(int i=0;i<14;i++) {
        float distance=(nearT+farT)*.5; vec3 point=eye+ray*distance;
        if(point.z>heightAt(point.xy)) nearT=distance; else farT=distance;
    }
    vec3 p=eye+ray*(nearT+farT)*.5;
    vec4 cell=cellField(p.xy);
    float e=.003, h=heightAt(p.xy);
    float xp=heightAt(p.xy+vec2(e,0)),xm=heightAt(p.xy-vec2(e,0));
    float yp=heightAt(p.xy+vec2(0,e)),ym=heightAt(p.xy-vec2(0,e));
    vec3 n=normalize(vec3(-(xp-xm)/(2.*e),-(yp-ym)/(2.*e),1.));
    float rim=1.-smoothstep(.08,.15,cell.x);
    vec3 pigment=mineral(p.xy,cell.zw);
    if(hasImage!=0) pigment=mix(pigment,imagePlane(uv*.5+.5+n.xy*.008),imageMix);
    vec4 art=cutoutArt(p.xy/1.3+.5);pigment=mix(pigment,art.rgb,art.a*imageMix);
    pigment=mix(pigment,vec3(.004,.008,.012),rim);
    vec3 lamp=normalize(vec3(-.35,.6,2.)-p);
    float diffuse=max(0.,dot(n,lamp));
    float highlight=pow(max(0.,dot(reflect(-lamp,n),-ray)),mix(48.,95.,rim));
    vec3 light=pigment*(.35+.95*diffuse)*(1.+bass*.4);
    light+=sky(reflect(ray,n),p.xy)*mix(.6,.34,rim);
    light+=vec3(.7,.82,.9)*highlight*(.6+.6*treble);
    // Visible cut boundaries come from curvature and cell seams, not outlines
    // pasted over the scene. Broad faces retain their own moving reflections.
    float bend=abs(xp+xm+yp+ym-4.*h);
    float edge=smoothstep(.001,.014,bend);
    light*=1.-edge*.68;
    float pulseRadius=mod(bassImpulse*1.8,2.3);
    float wave=exp(-pow((length(uv)-pulseRadius)*13.,2.))*bass;
    light+=pigment*wave*.75;
    light*=.7+.45*intensity;
    colour=vec4(light,1);
}

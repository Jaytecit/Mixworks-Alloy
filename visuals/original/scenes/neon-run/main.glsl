// Original Mixworks Neon Run: layered polygon gates in a flying perspective.
#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
uniform vec2 resolution;
uniform float sceneTime, beatPulse, projectBeat, energy, bass, mid, treble, onset, intensity, motion, imageMix;
uniform sampler2D userImage;
uniform int hasImage;
uniform float audioMotion;
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
    return nestedMaterial(uv,mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend),1.0);
}
vec4 alphaPixel(sampler2D tex,ivec2 p,ivec2 size){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);vec4 c=texelFetch(tex,p,0);return vec4(linearRGB(c.rgb)*c.a,c.a);}
vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){ivec2 s=textureSize(tex,0);vec2 p=((uv-.5)*aspect+.5)*vec2(s)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);return mix(mix(alphaPixel(tex,i,s),alphaPixel(tex,i+ivec2(1,0),s),f.x),mix(alphaPixel(tex,i+ivec2(0,1),s),alphaPixel(tex,i+ivec2(1),s),f.x),f.y);}
vec4 cutoutArt(vec2 uv){if(hasCutout==0)return vec4(0);vec4 c=mix(alphaArt(cutoutImage,uv,cutoutAspect),alphaArt(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);return vec4(c.rgb/max(c.a,.00001),c.a);}

mat2 rotate(float a) { return mat2(cos(a),-sin(a),sin(a),cos(a)); }
float octagon(vec2 p) {
    p=abs(p);
    return max(dot(p,vec2(.92388,.38268)),dot(p,vec2(.38268,.92388)));
}
void main() {
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float t=sceneTime*(.65+motion*.9);
    float kick=beatPulse*.7+onset*.3;
    vec2 ray=rotate(.13*sin(midImpulse*.35))*uv/1.75;
    vec3 colourOut=vec3(.001,.002,.009);
    float travel=t*.6+bassTime*5.+bassImpulse*6.;
    float nearest=.45+mod(-travel,1.65);
    // Back-to-front perspective planes; opaque ribs hide distant geometry.
    for(int i=17;i>=0;i--) {
        float z=nearest+float(i)*1.65;
        float worldZ=z+travel;
        vec2 centre=vec2(.30*sin(worldZ*.17)+.26*sin(midImpulse*1.3+worldZ*.2),.18*sin(trebleImpulse*1.4+worldZ*.2));
        vec2 p=rotate(.12*worldZ+.32*sin(t*.32))*(ray*z-centre);
        float radius=1.32+.13*sin(worldZ*.32-t*.7)+.10*bass+.19*bassHit;
        float distance=abs(octagon(p)-radius);
        float aa=max(fwidth(distance),.001);
        float body=1.-smoothstep(.07,.07+aa,distance);
        float edge=1.-smoothstep(.009,.009+aa,abs(distance-.052));
        float face=1.-smoothstep(.012,.012+aa,distance);
        float illumination=.35+.65*pow(.5+.5*sin(worldZ*.8-projectBeat*1.57),3.);
        float hue=.5+.5*sin(worldZ*.27+t*.24);
        vec3 ink=mix(vec3(.015,.72,1.9),vec3(1.8,.02,.42),hue);
        if(hasImage!=0) ink=mix(ink,imagePlane(p*.24+.5),imageMix);
        float fog=exp(-z*.065);
        colourOut=mix(colourOut,vec3(.007,.011,.025)*(.6+mid),body*fog);
        colourOut+=ink*(edge*(.55+illumination+kick*1.8)+face*(.09+.24*treble))*fog;
        float glow=exp(-distance*18.)*.075*pow(fog,1.3);
        colourOut+=ink*glow*(1.+energy);
        // Offset little light modules distinguish each gate from a plain outline.
        float angle=atan(p.y,p.x);
        float modules=pow(max(0.,cos(angle*8.-trebleTime*8.)),26.);
        colourOut+=vec3(.2,.9,1.7)*modules*exp(-pow((distance-.12)*32.,2.))*fog*(.12+.8*treble);
    }
    float r=length(uv);
    colourOut+=vec3(.035,.009,.08)*exp(-r*r*6.);
    colourOut*=.65+.5*intensity;
    colour=vec4(colourOut,1);
}

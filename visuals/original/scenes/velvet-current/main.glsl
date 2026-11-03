// Original Mixworks Velvet Current: moving liquid relief, pearlescent contours.
#version 330
uniform vec2 resolution;
uniform float sceneTime, beatPulse, projectBeat, energy, bass, mid, treble, onset, intensity, motion, imageMix;
uniform sampler2D userImage;
uniform int hasImage;
uniform float audioMotion;
uniform float bassHit,midHit,trebleHit,bassTime,midTime,trebleTime;
out vec4 colour;
mat2 artTurn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 linearRGB(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 imagePlane(vec2 uv){
    vec2 p=artTurn((imageRotation-.5)*6.283185)*(uv-.5)/(1.+imageZoom*.25);
    p.x/=resolution.x/resolution.y;
    vec2 a=1.-abs(mod(p*mediaAspect+.5,2.)-1.);
    vec2 b=1.-abs(mod(p*nextMediaAspect+.5,2.)-1.);
    return mix(linearRGB(textureLod(userImage,a,0.).rgb),linearRGB(textureLod(nextImage,b,0.).rgb),imageBlend);
}
vec4 alphaPixel(sampler2D tex,ivec2 p,ivec2 size){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size)))return vec4(0);vec4 c=texelFetch(tex,p,0);return vec4(linearRGB(c.rgb)*c.a,c.a);}
vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){ivec2 s=textureSize(tex,0);vec2 p=((uv-.5)*aspect+.5)*vec2(s)-.5;ivec2 i=ivec2(floor(p));vec2 f=fract(p);return mix(mix(alphaPixel(tex,i,s),alphaPixel(tex,i+ivec2(1,0),s),f.x),mix(alphaPixel(tex,i+ivec2(0,1),s),alphaPixel(tex,i+ivec2(1),s),f.x),f.y);}
vec4 cutoutArt(vec2 uv){if(hasCutout==0)return vec4(0);vec4 c=mix(alphaArt(cutoutImage,uv,cutoutAspect),alphaArt(nextCutoutImage,uv,nextCutoutAspect),cutoutBlend);return vec4(c.rgb/max(c.a,.00001),c.a);}

mat2 rotate(float a) { return mat2(cos(a),-sin(a),sin(a),cos(a)); }
float t;
vec2 flow(vec2 p) {
    p=rotate(.55*mid*sin(projectBeat*1.570796))*p;
    p+=vec2(.38*bass*sin(projectBeat*3.141593),.16*treble*sin(p.x*8.-trebleTime*8.));
    p+=.34*vec2(sin(p.y*1.8+t*.7),cos(p.x*1.6-t*.53));
    p+=.22*vec2(cos(p.y*3.2-p.x+t*.4),sin(p.x*3.1+p.y-t*.61));
    return p;
}
float relief(vec2 p) {
    vec2 q=flow(p);
    float ridge=sin(q.x*3.+sin(q.y*2.7+t*.4)*(1.+.7*bass)-t*.6);
    float folds=sin(q.y*3.9-q.x*1.4+t*.8);
    return ridge*.36+folds*.20+sin(q.x*8.+q.y*5.-t)*(.035+.08*mid);
}
void main() {
    t=sceneTime*(.45+motion*.8)+audioMotion*.35;
    vec2 uv=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    vec2 p=uv*1.55+vec2(.35*sin(bassTime*3.),.3*cos(midTime*2.));
    p+=.1*midHit*vec2(sin(uv.y*3.),cos(uv.x*2.));
    float h=relief(p);
    float e=.003;
    vec3 normal=normalize(vec3(-(relief(p+vec2(e,0))-relief(p-vec2(e,0)))/(2.*e),
        -(relief(p+vec2(0,e))-relief(p-vec2(0,e)))/(2.*e),1.5));
    vec2 q=flow(p);
    float ink=.5+.5*sin(q.x*2.+q.y*1.4+t*.32+h*2.);
    vec3 pigment=mix(vec3(.018,.025,.12),vec3(.48,.018,.12),smoothstep(.1,.85,ink));
    pigment=mix(pigment,vec3(.015,.42,.33),pow(.5+.5*sin(h*6.+q.y+t*.35),4.)*.75);
    if(hasImage!=0) pigment=mix(pigment,imagePlane(uv*.5+.5+normal.xy*.012),imageMix);
    vec4 art=cutoutArt((rotate(.2*mid*sin(projectBeat*1.570796))*uv-vec2(.2*bass*sin(projectBeat*3.141593),0))/1.5+.5);pigment=mix(pigment,art.rgb,art.a*imageMix);
    float shade=.18+.82*max(0.,dot(normal,normalize(vec3(-.4,.7,1.))));
    float spec=pow(max(0.,dot(normal,normalize(vec3(-.45,.55,1.)))),45.);
    float reflectBand=pow(max(0.,1.-abs(normal.x-.13)),32.);
    float contour=sin(h*(32.+8.*mid)+q.x*2.-t*1.4-trebleTime*7.);
    float aa=max(fwidth(contour),.025);
    float gold=1.-smoothstep(.025,.025+aa,abs(contour));
    float fine=pow(.5+.5*sin(h*140.+q.y*9.+t*2.),18.);
    vec3 light=pigment*shade*(.75+.6*energy);
    light+=vec3(1.3,.64,.18)*gold*(.12+.4*treble)*(.35+.65*normal.z);
    light+=vec3(.28,.6,.72)*reflectBand*.34;
    light+=vec3(1.8,1.15,.66)*spec*(.5+.8*beatPulse+.5*onset);
    light+=vec3(.1,.22,.3)*fine*.11;
    light*=.7+.4*intensity;
    light*=1.-.15*smoothstep(.4,2.3,length(uv));
    colour=vec4(light,1);
}

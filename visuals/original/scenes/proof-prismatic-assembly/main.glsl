#version 330
// Analytic rigid geometry with a true parent/child/grandchild transform chain.
// Percussive turns, articulated depth kicks and band-specific edge emission.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float intensity,imageMix,detail,scale,bassHit,midHit,trebleHit,pulse,mode;
out vec4 colour;
mat3 rz(float a){float c=cos(a),s=sin(a);return mat3(c,s,0,-s,c,0,0,0,1);}
mat3 ry(float a){float c=cos(a),s=sin(a);return mat3(c,0,-s,0,1,0,s,0,c);}
mat3 rx(float a){float c=cos(a),s=sin(a);return mat3(1,0,0,0,c,s,0,-s,c);}
vec3 palette(float id){
    float k=mod(id,4.);
    if(k<1.)return vec3(.95,.51,.12);
    if(k<2.)return vec3(.48,.17,.88);
    if(k<3.)return vec3(.84,.87,.96);
    return vec3(.11,.19,.48);
}
vec4 art(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(uv):vec4(0),b=sampleCutout(uv);
    return vec4(mix(a.rgb,b.rgb,b.a),max(a.a,b.a));
}
// Ray-box slab test. Normals and image coordinates stay rigid in local space.
void panel(vec3 ro,vec3 rd,vec3 centre,mat3 basis,vec3 extent,float id,float level,
           inout float nearest,inout vec3 result){
    vec3 o=transpose(basis)*(ro-centre),d=transpose(basis)*rd;
    vec3 inv=sign(d+vec3(1e-9))/max(abs(d),vec3(1e-7));
    vec3 a=(-extent-o)*inv,b=(extent-o)*inv;
    vec3 lo=min(a,b),hi=max(a,b);
    float entry=max(max(lo.x,lo.y),lo.z),leave=min(min(hi.x,hi.y),hi.z);
    if(entry<0.||entry>leave||entry>=nearest)return;
    vec3 p=o+d*entry;
    vec3 n=vec3(0);
    if(lo.x>=lo.y&&lo.x>=lo.z)n.x=-sign(d.x);
    else if(lo.y>=lo.z)n.y=-sign(d.y);else n.z=-sign(d.z);
    vec3 normal=basis*n;
    vec2 uv=abs(n.x)>.5?p.zy/extent.zy*.5+.5:abs(n.y)>.5?p.xz/extent.xz*.5+.5:p.xy/extent.xy*.5+.5;
    vec3 pigment=palette(id+level);
    float accent=(level<.5?bassHit:level<1.5?midHit:trebleHit)*pulse;
    pigment=mix(pigment,palette(id+level+1.),accent*.40);
    float rim=min(min(uv.x,1.-uv.x),min(uv.y,1.-uv.y));
    vec3 base=pigment;
    {
        // Each physical frame holds a complete composition. Nesting comes from
        // geometry, not a recursive copy of the whole screen or a material warp.
        vec4 image=art(uv);
        float ink=dot(image.rgb,vec3(.21,.72,.07));
        base=mix(pigment*.09,pigment*(.08+ink*1.25),image.a*imageMix);
        float border=1.-smoothstep(.025,.045,rim);
        base=mix(base,palette(id+2.)*.6,border);
        float engraving=pow(.5+.5*cos((uv.x+uv.y)*120.),12.);
        base+=pigment*.09*engraving*border*detail;
    }
    vec3 light=normalize(vec3(-.5,.85,-.7));
    float diffuse=.16+1.05*max(0.,dot(normal,light));
    float spec=pow(max(0.,dot(normal,normalize(light-rd))),50.);
    float bevel=exp(-rim*85.);
    vec3 c=base*diffuse+vec3(.9,.78,.62)*spec*.45+pigment*bevel*.16;
    c+=pigment*accent*(bevel*6.+.22);
    // Atmospheric separation: back layers recede, close edges stay crisp.
    c=mix(c,vec3(.026,.016,.048),1.-exp(-entry*.018));
    nearest=entry;result=c;
}
void main(){
    vec2 screen=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    if(mode>.25){
        // Fold the view into four or six reflected sectors. Identical seam rays
        // keep the mirrored sculpture continuous without a drifting screen spin.
        float sectors=mode>.75?6.:4.;
        float sector=6.2831853/sectors;
        float angle=abs(mod(atan(screen.y,screen.x)+sector*.5,sector)-sector*.5);
        screen=length(screen)*.43*vec2(cos(angle),sin(angle))+vec2(0.,.48);
    }
    vec3 ro=vec3(0,0,-7.),rd=normalize(vec3(screen,1.85));
    float vignette=exp(-dot(screen,screen)*.45);
    vec3 result=mix(vec3(.004,.003,.01),vec3(.025,.012,.045),vignette);
    result+=vec3(.055,.017,.003)*exp(-dot(screen-vec2(-1.,.6),screen-vec2(-1.,.6))*1.5);
    float nearest=100.;
    for(int i=0;i<16;i++){
        float id=float(i);vec4 s=texelFetch(input0,ivec2(i,0),0);
        // Reflected compositions hold their spatial anchors: percussion, not a
        // conveyor across the frame, supplies the dominant visible movement.
        float u=mode>.25?id/16.:fract(s.x),row=mod(id,3.)-1.;
        // Wrap only outside the view, including the extents of nested children.
        float span=(7.+4.8+5.1)*(resolution.x/resolution.y)/1.85+5.5;
        vec3 centre=vec3(mix(-span,span,u),row*2.15+(u-.5)*1.8,2.4+mod(id,4.)*.8-2.*sin(u*3.14159)-bassHit*.55*pulse);
        mat3 parent=rz(s.y)*ry(s.y*.53+id*.2)*rx(.2);
        float size=(1.05+scale*.55)*(1.+bassHit*.12*pulse);
        panel(ro,rd,centre,parent,vec3(1.30,1.08,.68)*size,id,0.,nearest,result);
        // Child is attached to the parent's front. It swings fully around its
        // local hinge while retaining all parent translations and rotations.
        mat3 child=parent*rz(s.z)*ry(.28*sin(s.w));
        vec3 childCentre=centre+parent*vec3(.43,.20,-.96-midHit*.55*pulse)*size;
        panel(ro,rd,childCentre,child,vec3(.70,.60,.40)*size,id,1.,nearest,result);
        mat3 grandchild=child*rz(-s.w)*ry(.35);
        vec3 grandCentre=childCentre+child*vec3(-.20,.14,-.58)*size;
        panel(ro,rd,grandCentre,grandchild,vec3(.30,.28,.22)*size,id,2.,nearest,result);
    }
    result*=.85+intensity*.35;
    colour=vec4(max(result,vec3(0)),1);
}

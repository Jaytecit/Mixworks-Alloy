#version 330
#define LIQUID_KIND 0
// Owned liquid identities. LIQUID_KIND: 0 spiral funnel, 1 four impacts, 2 thin film.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float warp,detail,imageMix,intensity,pulse,hue;
uniform float bassHit,midHit,trebleHit,bassImpulse,midImpulse,trebleImpulse;
out vec4 colour;
const float TAU=6.2831853;
mat2 turn(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
vec3 tint(float t){return .08+.78*(.5+.5*cos(t+vec3(0.,2.1,4.2)+hue*TAU));}
vec3 picture(vec2 uv){
    vec4 a=hasImage!=0?sampleFittedMedia(clamp(uv,0.,1.)):vec4(0);
    vec4 b=spriteMaterial(clamp(uv,0.,1.));
    return mix(tint(uv.x*2.+uv.y),mix(a.rgb,b.rgb,b.a),max(a.a,b.a)*imageMix);
}
float height(vec2 uv){return texture(input0,clamp(uv,0.,1.)).r;}
vec2 impact(int i){return vec2((i%2==0?-.478125:.478125)*resolution.x/resolution.y,(i<2?-.43:.43));}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,aspect=vec2(resolution.x/resolution.y,1.);
    vec2 p=(uv-.5)*2.*aspect,px=1./vec2(textureSize(input0,0));
    float t=travelTime,lo=bassHit*pulse,mi=midHit*pulse,hi=trebleHit*pulse;
    float h=height(uv);
    vec2 g=vec2(height(uv+vec2(px.x,0))-height(uv-vec2(px.x,0)),height(uv+vec2(0,px.y))-height(uv-vec2(0,px.y)))*.5;
    vec3 c;
#if LIQUID_KIND == 0
    // A logarithmic spiral winds down a shaded funnel into one unobstructed eye.
    float r=max(length(p),.001),a=atan(p.y,p.x),eye=.14+.045*lo;
    float depth=log(max(r,eye)/eye);
    float phase=a*3.+depth*(7.+warp*2.)-t*1.3-midImpulse*.9;
    float fold=sin(phase),slope=cos(phase);
    vec2 radial=p/r,tangent=vec2(-radial.y,radial.x);
    vec2 gradient=(radial*(.28+(.10+.06*lo)*slope*(7.+warp*2.))+tangent*slope*.38)/max(r,.18);
    vec3 n=normalize(vec3(-gradient,1.));
    vec2 chart=.5+turn(depth*(1.25+warp*.6)-t*.25-midImpulse*.20)*p/aspect*.42;
    vec3 art=picture(chart+g*.06);
    float diffuse=.30+.70*max(0.,dot(n,normalize(vec3(-.5,.75,1.))));
    float recession=.17+.83*smoothstep(eye,1.6,r);
    c=art*diffuse*recession*(.70+.22*fold);
    float satin=pow(max(0.,dot(n,normalize(vec3(-.4,.6,1.)))),22.);
    c+=tint(depth*.6)*satin*(.12+.35*hi)*smoothstep(eye,eye+.12,r);
    float lip=exp(-pow((r-eye-.025)*30.,2.));
    c+=tint(.4)*lip*(.12+.18*lo);
    c=mix(vec3(.002,.003,.006),c,smoothstep(eye-.025,eye+.015,r));
#elif LIQUID_KIND == 1
    // One continuous reflecting pool, with four separated injection sites.
    vec2 ripple=g*(12.+warp*9.);float rings=0.,cores=0.,rims=0.;
    for(int i=0;i<4;i++){
        vec2 q=p-impact(i);float d=max(length(q),.001);
        // Slow ambient rings establish direction; the retained wave field carries
        // the independently injected music pressure away from these same sites.
        float radius=mod(t*.24+float(i)*.13,.60),ring=0.;
        for(int j=0;j<3;j++){
            float front=radius-float(j)*.16;
            ring+=exp(-pow((d-front)*36.,2.))*smoothstep(0.,.08,front)*(1.-smoothstep(.42,.60,radius))*(1.-float(j)*.22);
        }
        rings+=ring;
        ripple+=q/d*cos(d*35.-t*7.+float(i))*ring*(.07+.10*lo);
        cores+=exp(-d*d*480.);
        rims+=exp(-pow((d-.065)*65.,2.));
    }
    vec3 n=normalize(vec3(-ripple,1.));
    vec3 art=picture(uv+ripple*(.025+.025*warp));
    float light=.45+.55*max(0.,dot(n,normalize(vec3(-.55,.7,1.))));
    c=art*light*.72*(1.-min(.90,cores));
    c+=tint(.8+h*4.)*(rings*(.25+.25*mi)+rims*(.27+.45*lo));
    c+=vec3(.20,.55,.65)*cores*(.12+.45*lo);
    c+=tint(.3)*pow(max(0.,dot(n,normalize(vec3(-.4,.55,1.)))),38.)*(.09+.30*hi);
#else
    // Broad thin-film thickness and lateral colour shear, without a raised basin.
    float shear=sin(p.y*2.8+t*.22+midImpulse*.6)*(.035+.09*mi);
    vec2 chart=uv+vec2(shear+.015*sin(p.x*3.-t*.17),g.y*.04);
    float thickness=p.y*1.45+p.x*.42+sin(p.x*2.3+p.y*1.2+t*.16)*.7+h*.32+lo*.12;
    vec3 film=.65+.35*cos(thickness*(3.+detail*1.5)+vec3(0.,2.1,4.2)+midImpulse*.22);
    vec3 art=picture(chart);
    // Chromatic dispersion samples the same source along the shear direction.
    float split=(.004+.020*mi)*(.4+warp);
    vec3 dispersed=vec3(picture(chart+vec2(split,0)).r,art.g,picture(chart-vec2(split,0)).b);
    c=dispersed*(.36+.48*film)+film*(.12+.08*intensity);
    float shimmer=pow(max(0.,sin(p.x*170.+p.y*81.+trebleImpulse*11.+t*.5)),18.);
    float fringe=pow(max(0.,cos(thickness*13.+t*.3)),12.);
    c+=vec3(.55,.82,1.)*hi*shimmer*fringe*.55;
#endif
    // Emboss the existing optical surface, preserving its funnel/pool/film identity.
    if(reliefDepth>0. && hasCutout!=0){
        vec2 slope=vec2(spriteRelief(uv+vec2(px.x*2.,0))-spriteRelief(uv-vec2(px.x*2.,0)),
                        spriteRelief(uv+vec2(0,px.y*2.))-spriteRelief(uv-vec2(0,px.y*2.)))/(px*4.);
        vec3 n=normalize(vec3(-slope*.18,1.));
        c*=.5+.5*n.z-.4*n.x+.3*n.y;
    }
    colour=vec4(clamp(c,0.,1.),1.);
}

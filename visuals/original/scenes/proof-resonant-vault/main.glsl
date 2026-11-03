#version 330
// Original resonant vault: one continuous folded wall, lit in world space.
// mode changes the cross-section to an open pleated canopy. All material,
// audio state and finishing passes remain identical.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float sceneTime,bass,mid,treble,bassTime,midTime,trebleTime,trebleHit;
uniform float intensity,warp,detail,pulse,scale,mode,motion;
out vec4 colour;
float travel(){return sceneTime*(.14+.22*motion);}
// Match the field to the visible depth: far opening at 18, viewer at zero.
// No clamped endpoint beyond the structure to hold a pulse onscreen.
float pressure(float z){if(z<0.||z>18.)return 0.;return min(1.,texture(input1,vec2(z/18.,.5)).g*2.4)*pulse;}
vec2 centre(float z){return vec2(.20*sin(z*.31+midTime*.24),.16*cos(z*.37-midTime*.19));}
float wall(vec3 p){
    vec2 q=p.xy-centre(p.z);
    float a=atan(q.y,q.x),t=travel(),w=pressure(p.z);
    float twist=a+.22*p.z+.26*sin(p.z*.23-t*.3);
    float folds=cos(5.*twist+t*.22)+.27*cos(10.*twist-p.z*.6-t*.43);
    float radius=1.48+(.26+.22*warp)*folds+.17*sin(p.z*1.5-t*1.4);
    radius+=.75*w*(.65+.35*sin(a*3.+p.z*.8));
    radius+=.12*bass*sin(p.z*2.1-bassTime*1.5);
    if(mode>.5){
        // Canopy has a broad sinuous slit and a rectangular, pleated profile.
        float roof=.92+.30*cos(q.x*2.8+p.z*.45+t*.4)+.18*sin(p.z*1.2-t);
        roof+=.7*w*cos(q.x*1.7);
        return (roof-abs(q.y)+.16*sin(q.x*6.+p.z*.6))*.48;
    }
    return (radius-length(q))*.48;
}
vec2 reflected(vec2 p){return 1.-abs(mod(p,2.)-1.);}
vec3 pigment(vec2 uv){
    vec3 c=texture(input0,reflected(uv)).rgb;
    return pow(max(c,vec3(0)),vec3(2.2));
}
void main(){
    vec2 screen=(2.*gl_FragCoord.xy-resolution)/resolution.y;
    float t=travel();
    vec3 ro=vec3(.10*sin(t*.31),.08*cos(t*.23),0.);
    vec3 rd=normalize(vec3(screen*(.83+.22*scale),1.42));
    float dist=0.;vec3 p=ro;bool hit=false;
    for(int i=0;i<128;i++){
        p=ro+rd*dist;float d=wall(p);
        if(d<.0025*(1.+dist*.12)){hit=true;break;}
        dist+=max(.003,d*.9);if(dist>18.)break;
    }
    // A grazing ray may exhaust its budget while converging on a fold. Shade
    // that near surface instead of punching a dark pinhole through the wall.
    if(dist<18.)hit=true;
    vec3 c=vec3(.008,.016,.025)*exp(-dot(screen,screen)*.4);
    c+=vec3(.025,.075,.085)*exp(-dot(screen,screen)*5.);
    if(hit){
        vec2 e=vec2(.006,0);
        vec3 n=normalize(vec3(wall(p+e.xyy)-wall(p-e.xyy),wall(p+e.yxy)-wall(p-e.yxy),wall(p+e.yyx)-wall(p-e.yyx)));
        vec2 q=p.xy-centre(p.z);float a=atan(q.y,q.x);
        // World projections avoid angular seams and put broad source contours
        // across connected folds. Finer inner material moves independently.
        vec2 uv=vec2(q.x*.28+p.z*.085,q.y*.27+p.z*.035)+.5;
        uv+=.045*vec2(sin(p.z*.8+midTime*.3),cos(q.x*3.-midTime*.27));
        vec3 art=pigment(uv);
        vec3 inner=pigment(uv*1.8+vec2(t*.018,-trebleTime*.015));
        float crease=pow(.5+.5*sin(p.z*8.+a*3.+sin(a*5.-t)),10.);
        art=mix(art,inner,.15+.25*crease*detail);
        vec3 light=normalize(vec3(-.6,.85,-.7));
        float diffuse=max(0.,dot(n,light));
        float fresnel=pow(1.-max(0.,dot(n,-rd)),3.);
        vec3 halfVector=normalize(light-rd);
        float spec=pow(max(0.,dot(n,halfVector)),36.);
        float fine=.5+.5*sin(p.z*42.+a*11.+sin(a*7.+trebleTime));
        float emboss=mix(.86,1.07,fine*detail);
        float recess=clamp(wall(p+n*.12)/.12,.25,1.);
        c=art*(.19+1.45*diffuse)*emboss*recess;
        c+=vec3(.62,.78,.90)*spec*.8+vec3(.08,.25,.28)*fresnel*.16;
        float ridge=pow(.5+.5*cos(a*5.+p.z*1.3-t*.6),26.);
        c+=vec3(.08,.48,.53)*ridge*(.12+.6*trebleHit)*detail;
        c+=vec3(.65,.27,.06)*pressure(p.z)*(.08+.26*crease);
        c=mix(c,vec3(.008,.025,.035),1.-exp(-dist*.035));
    }
    colour=vec4(max(c,vec3(0))*(.85+intensity*.65),1);
}

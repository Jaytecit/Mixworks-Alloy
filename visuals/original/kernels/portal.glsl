// Layered lit architecture; distinct cross-sections and depth motions per recipe.
float section(vec2 p,float z,float t){
    if(VARIANT==0)return max(abs(p.x),abs(p.y));
    if(VARIANT==1)return length(p)+.15*sin(atan(p.y,p.x)*5.+z*.4);
    if(VARIANT==2){p=abs(p);return max(p.x*.866+p.y*.5,p.y);}
    if(VARIANT==3)return max(abs(p.x)*.866+p.y*.5,-p.y);
    if(VARIANT==4)return length(p)+.08*sin(atan(p.y,p.x)*12.-t);
    return pow(pow(abs(p.x),4.)+pow(abs(p.y),4.),.25);
}
void main(){vec2 uv=domain();float t=clockTime();vec3 c=vec3(.002,.004,.012);
    float travel=t*.45+bassTime*(3.+float(VARIANT)*.25)+1.25*lowSwing();
    uv=rot(.24*midSwing())*uv;
    for(int i=21;i>=0;i--){float z=.5+mod(-travel,1.5)+float(i)*1.5;float world=z+travel;vec2 p=uv*z/2.;
        p-=vec2(.22*sin(world*.18)+.42*midSwing()*sin(world*.4),.24*highSwing()*cos(world*.5));
        p=rot(world*(.03+warp*.2)+.6*sin(midTime*1.7+world*.15)*mid+.25*lowSwing())*p;
        float aperture=1.15+.18*bass*sin(world*.8-bassTime*4.)+.11*highSwing()*sin(world);
        float d=abs(section(p,z,t)-aperture);float aa=max(fwidth(d),.002);float rib=1.-smoothstep(.055,.055+aa,d);float edge=1.-smoothstep(.009,.009+aa,abs(d-.045));float fog=exp(-z*.075);
        vec3 ink=media(p*.25+.5,palette(world*.045+t*.025));c=mix(c,ink*.035,rib*fog);c+=ink*(edge*(.8+beatPulse*pulse)+exp(-d*20.)*.07)*fog;
        float lamps=pow(max(0.,cos(atan(p.y,p.x)*(4.+float(VARIANT)*2.)-trebleTime*7.-highSwing())),28.);c+=palette(.6)*lamps*exp(-abs(d-.14)*55.)*fog*.24;
    }finish(c,uv);}

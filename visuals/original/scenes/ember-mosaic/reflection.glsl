#version 330
uniform vec2 resolution;
uniform sampler2D input0;
uniform float mirror,symmetry;
out vec4 colour;
void main(){vec2 aspect=vec2(resolution.x/resolution.y,1.);vec2 p=(gl_FragCoord.xy/resolution-.5)*2.*aspect;
    if(mirror>.5)p.x=abs(p.x);
    float sectors=floor(symmetry*6.+.5);
    if(sectors>0.){float n=sectors+2.,a=atan(p.y,p.x),r=length(p);a=abs(mod(a+3.141593/n,6.283185/n)-3.141593/n);p=vec2(cos(a),sin(a))*r*.72;}
    vec2 uv=p/(2.*aspect)+.5;uv=1.-abs(mod(uv,2.)-1.);colour=vec4(texture(input0,uv).rgb,1.);
}

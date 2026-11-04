#version 330
// A separate optical neighbourhood: broad liquid normals, not raw simulation texels.
uniform vec2 resolution;
uniform sampler2D input0;
out vec4 colour;
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    vec4 sum=vec4(0);float total=0.;
    for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
        float weight=exp(-float(x*x+y*y)*.3);
        sum+=texture(input0,clamp(uv+vec2(x,y)*px*1.7,0.,1.))*weight;total+=weight;
    }
    colour=sum/total;
}

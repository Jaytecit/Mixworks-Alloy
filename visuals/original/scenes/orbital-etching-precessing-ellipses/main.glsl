#version 330
#define FAMILY 5
uniform sampler2D input1;
uniform float intensity,detail,colourR,colourG,colourB;
uniform float generationBlend,handoffFamily;
in vec2 sourceUV,localUV;
flat in float speed;
out vec4 fragColor;
void main(){
 vec4 ink=texture(input1,sourceUV);
 float border=1.-smoothstep(1.-mix(.035,.18,detail),1.,max(abs(localUV.x),abs(localUV.y)));
 if(handoffFamily<0.)border=mix(1.,border,generationBlend);
 // Colour is captured source material. No sampled final scene or opaque logo layer.
 vec3 tint=mix(vec3(1.),vec3(colourR,colourG,colourB)*1.43,.3);
 float style=handoffFamily<0.?generationBlend:1.;
 float light=mix(1.,.65+.5*intensity,style);
 fragColor=vec4(clamp(ink.rgb*light*mix(vec3(1.),tint,style),0.,ink.a),ink.a)*border;
}

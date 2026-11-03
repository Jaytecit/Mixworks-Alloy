#version 330
uniform sampler2D input2;
uniform float intensity;
in vec2 sourceUV,localUV;
in float sheen;
flat in float group,tiny;
out vec4 colour;
void main(){
 vec4 art=texture(input2,sourceUV);
 vec3 source=art.rgb/max(art.a,.0001);
 float pigment=dot(source,vec3(.2126,.7152,.0722));
 vec3 tint=mix(vec3(.11,.66,.68),vec3(.94,.40,.27),step(4.5,group));
 tint=mix(tint,vec3(.68,.79,.94),step(6.5,group));
 float edge=1.-smoothstep(.90,1.,abs(localUV.y));
 float seam=1.-smoothstep(.995,1.,abs(localUV.x));
 float alpha=edge*seam*art.a;
 float rim=pow(abs(localUV.y),12.);
 vec3 material=mix(tint*(.38+.8*pigment),source,.55);
 material*=.85+.65*sheen;
 material+=tint*(.35*rim+.20*pow(max(0.,1.-abs(localUV.y+.25)),14.));
 material*=.8+.35*intensity;
 colour=vec4(material*alpha,alpha);
}

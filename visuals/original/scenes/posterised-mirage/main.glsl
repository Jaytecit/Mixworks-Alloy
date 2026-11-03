#version 330
// No image sampler: only pigments that have entered the feedback state can appear.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float intensity,hue,detail,generationBlend;
out vec4 fragColor;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec4 s=texture(input0,uv);
 // Lift surviving pigment, while the coverage gate still retires faint residue.
 vec3 c=s.rgb/max(.18,s.a)*smoothstep(.035,.16,s.a);float bands=4.+floor(detail*5.);
 vec3 quant=floor(c*bands)/bands;c=mix(c,quant,.55);
 vec3 shifted=mix(c,c.gbr,abs(hue-.5)*.8);
 vec3 paper=vec3(.012,.018,.035);
 fragColor=vec4(clamp(paper+shifted*(.7+.6*intensity),0.,1.),1.);fragColor=mix(vec4(s.rgb,1.),fragColor,generationBlend);}

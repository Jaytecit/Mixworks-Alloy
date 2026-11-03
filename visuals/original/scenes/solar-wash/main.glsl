#version 330
uniform float bassImpulse,midImpulse,trebleImpulse;
// All image identity and echoes come from recurrent pigment, not fresh media.
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform float intensity,hue,detail,generationBlend;
out vec4 fragColor;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec4 s=texture(input0,uv),blur=texture(input1,uv);
 vec3 hot=max(vec3(0),s.rgb-blur.rgb);
 vec3 c=s.rgb*(1.+intensity*.8)+hot*(.5+detail)+blur.rgb*.08;
 c=mix(c,c.brg,abs(hue-.5)*.6);
 fragColor=vec4(pow(clamp(c,0.,1.),vec3(.95)),1.);fragColor=mix(vec4(s.rgb,1.),fragColor,generationBlend);}

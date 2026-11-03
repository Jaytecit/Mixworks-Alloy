#version 330
uniform sampler2D input0;
uniform vec2 resolution;
out vec4 colour;
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 vec4 form=texture(input0,uv);
 vec3 background=mix(vec3(.012,.020,.036),vec3(.027,.048,.065),exp(-length((uv-.5)*vec2(1.5,1.))*2.8));
 colour=vec4(form.rgb+background*(1.-form.a),1.);
}

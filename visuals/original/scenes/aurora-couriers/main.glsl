#version 330
// Final flock composition; also the native catalogue entry point.
uniform vec2 resolution;
uniform sampler2D input0;
uniform float sceneTime,bassHit,trebleHit;
out vec4 colour;
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
    vec4 particles=texture(input0,uv);
    vec3 halo=vec3(0);
    for(int i=0;i<8;i++){
        float a=float(i)*.785398;
        halo+=texture(input0,clamp(uv+vec2(cos(a),sin(a))*px*3.,0.,1.)).rgb;
    }
    vec3 bg=vec3(.002,.004,.012)*(1.-particles.a);
    colour=vec4(clamp(bg+particles.rgb+halo*.035,0.,1.),1.);
}

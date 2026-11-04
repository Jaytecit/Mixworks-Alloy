#version 330
uniform vec2 resolution;
uniform sampler2D input0,input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float intensity;
out vec4 colour;
// PIGMENT_TRIO_COMMON
float density(vec2 uv){return texture(input0,clamp(uv,0.,1.)).a;}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    vec4 pigment=texture(input0,uv);
    vec3 c=vec3(0);
#if PIGMENT_KIND == 0
    // The retained inlet makes the moving jet and its decaying trail.
    vec2 g=vec2(density(uv+vec2(px.x,0))-density(uv-vec2(px.x,0)),
        density(uv+vec2(0,px.y))-density(uv-vec2(0,px.y)));
    c=pigment.rgb*3.2;
    float edge=length(g)*(.12+.75*trebleHit*pulse);
    c+=edge*vec3(.1,.6,1.);
#elif PIGMENT_KIND == 1
    // Two independent fronts carry complementary source halves, never two logos.
    float left=pressMask(uv,-1.),right=pressMask(uv,1.),mask=max(left,right);
    vec4 direct=pigmentArt(pressSource(uv,-1.))*left+pigmentArt(pressSource(uv,1.))*right;
    c=mix(pigment.rgb,direct.rgb,.30)*mask;
    float ridge=pow(mask,.35)*(1.-mask);
    c+=ridge*(.10+1.1*trebleHit*pulse)*vec3(.15,.65,1.)*max(pigment.a,direct.a);
    c*=smoothstep(.06,.095,abs(uv.x-.5));
#else
    // Two broad pigment banks slip across one open jagged fault.
    float distance=abs(uv.y-faultBoundary(uv));
    float mask=smoothstep(faultGap(),faultGap()+.009,distance);
    vec2 q=faultSource(uv);vec4 direct=pigmentArt(q),past=texture(input0,clamp(q,0.,1.));
    c=mix(direct.rgb,past.rgb,.28)*mask;
    float lip=exp(-pow((distance-faultGap()-.008)*150.,2.));
    c+=lip*(.08+.65*trebleHit*pulse)*vec3(.3,.65,1.)*max(direct.a,past.a);
#endif
    colour=vec4(clamp(pow(max(c,0.),vec3(.85))*(.7+.6*intensity),0.,1.),1.);
}

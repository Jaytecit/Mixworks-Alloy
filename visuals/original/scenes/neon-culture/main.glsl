#version 330
// Surface is derived solely from living concentrations, never image RGB.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float generationBlend;
uniform float hue, intensity, detail, imageMix;
out vec4 fragColor;
float h(vec2 p) {return texture(input0,p).g;}
void main() {
    // Image methods change the colony's material chart without resetting biology.
    vec2 uv=materialDomain(gl_FragCoord.xy/resolution),px=1./vec2(textureSize(input0,0));
    vec2 c=texture(input0,uv).rg;
    vec2 g=vec2(h(uv+vec2(px.x,0))-h(uv-vec2(px.x,0)),h(uv+vec2(0,px.y))-h(uv-vec2(0,px.y)));
    vec3 n=normalize(vec3(-g*(16.+detail*24.),1.));
    float population=smoothstep(.035,.34,c.y);
    // Concentration bands and slope produce saturated ridges over dark valleys.
    float band=c.y*(3.5+imageMix*2.)+length(g)*1.2+hue*.35;
    vec3 colour=pow(.5+.5*cos(6.283185*(vec3(.0,.38,.69)+band)),vec3(3.));
    colour=mix(vec3(.0003,.0005,.001),colour,population);
    float diffuse=.24+.76*max(0.,dot(n,normalize(vec3(-.55,.6,1.))));
    float spec=pow(max(0.,dot(n,normalize(vec3(-.35,.4,1.)))),36.);
    colour=colour*diffuse*1.6+spec*population*(.18+.45*intensity)*smoothstep(.01,.11,length(g));
    vec3 inherited=texture(handoffMaterial,clamp(uv+g*.15*generationBlend,0.,1.)).rgb;
    fragColor=vec4(mix(inherited,pow(clamp(colour,0.,1.),vec3(1./2.2)),generationBlend),1.);
}

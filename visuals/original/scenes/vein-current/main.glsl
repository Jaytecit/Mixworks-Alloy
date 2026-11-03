#version 330
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D input1;
uniform sampler2D userImage;
uniform int hasImage;
uniform float imageMix, warp, hue, intensity, detail,generationBlend;
out vec4 fragColor;
vec4 media(vec2 uv) {
    vec4 c=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 s=sampleCutout(uv);return vec4(mix(c.rgb,s.rgb,s.a),max(c.a,s.a));
}
float heightAt(vec2 uv) {vec4 s=texture(input0,uv);return s.w*.55+s.z*.8;}
void main() {
    vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(input0,0));
    vec4 s=texture(input0,uv);
    vec2 g=vec2(heightAt(uv+vec2(px.x,0))-heightAt(uv-vec2(px.x,0)),heightAt(uv+vec2(0,px.y))-heightAt(uv-vec2(0,px.y)));
    vec3 n=normalize(vec3(-g*(19.+detail*30.),1.));
    vec2 refractUV=mix(uv,texture(input1,uv).xy,.58)+warp*(g*.48+s.xy*px*.45);
    refractUV=mix(uv,refractUV,generationBlend);
    vec4 art=media(refractUV);
    vec3 base=mix(vec3(.001,.002,.004),vec3(.11,.035,.006),smoothstep(.12,.8,s.z));
    vec3 imageLinear=pow(max(art.rgb,vec3(0)),vec3(2.2));
    base=mix(base,imageLinear,imageMix*art.a);
    float diffuse=.08+1.45*pow(max(0.,dot(n,normalize(vec3(-.5,.65,1.)))),1.5);
    float spec=pow(max(0.,dot(n,normalize(vec3(-.35,.4,1.)))),28.);
    float vein=smoothstep(.08,.55,length(g)*5.);
    float ridge=smoothstep(.015,.12,length(g));
    vec3 colour=base*(diffuse+.3*vein)*(.38+.62*ridge)+vec3(.48,.60,.7)*spec*(.025+.16*intensity)*ridge;
    colour*=.82+.18*pow(max(0.,1.-length(uv-.5)),.6);
    fragColor=vec4(mix(art.rgb*art.a,pow(clamp(colour,0.,1.),vec3(1./2.2)),generationBlend),1.);
}

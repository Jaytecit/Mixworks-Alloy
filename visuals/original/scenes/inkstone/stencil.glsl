#version 330
uniform vec2 resolution;
uniform sampler2D userImage;
uniform sampler2D input0;
uniform float imageMix,warp,pulse,bassHit,kickImpact,trebleHit;
out vec4 colour;

// Use screen-height units and contain the actual source canvas at this aspect.
// The runtime supplies premultiplied mipmaps, including during sprite blends.
vec4 artwork(sampler2D source,vec2 p,float lod) {
    vec2 size=vec2(textureSize(source,0));
    vec2 fit=size*min(resolution.x/size.x,resolution.y/size.y)/resolution.y;
    vec2 uv=p/(fit*(.84+.30*imageZoom))+.5;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec4(0);
    return textureLod(source,uv,lod);
}

vec4 sprite(vec2 p,float lod) {
    return mix(artwork(cutoutImage,p,lod),artwork(nextCutoutImage,p,lod),cutoutBlend);
}

float boundary(vec2 p) {
    return sprite(p,3.).a;
}

void main() {
    if(hasCutout==0){colour=vec4(0);return;}
    vec2 uv=gl_FragCoord.xy/resolution;
    vec2 p=(gl_FragCoord.xy-.5*resolution)/resolution.y;
    vec4 field=texture(input0,uv);
    float swell=pulse*max(.62*bassHit,kickImpact);
    // Only the softened alpha boundary bends the chart. RGB highlights stay
    // in the lighting calculation and cannot become forces.
    vec2 edge=vec2(boundary(p+vec2(.011,0))-boundary(p-vec2(.011,0)),
                   boundary(p+vec2(0,.011))-boundary(p-vec2(0,.011)));
    vec2 tangent=vec2(-edge.y,edge.x);
    vec2 q=p+warp*(.113*field.rg+(.011+.028*swell)*tangent);
    vec4 source=sprite(q,.75);
    vec3 art=source.rgb/max(source.a,.001);
    float light=dot(art,vec3(.2126,.7152,.0722));
    float ink=smoothstep(.34,.63,field.b+.06*(field.a-.5));
    vec3 sea=mix(vec3(.028,.12,.15),vec3(.075,.64,.43),ink);
    vec3 lacquer=mix(vec3(.23,.035,.14),vec3(.96,.40,.27),ink);
    // Cool boundaries and warm pale lettering use the same flow, with their
    // own pigments. An independent source-colour mix retains printed detail.
    vec3 pigment=mix(sea,lacquer,smoothstep(.37,.73,light));
    float vein=pow(field.a,8.)*(.20+.14*pulse*trebleHit+.20*swell);
    pigment+=vec3(.70,.87,.69)*vein*smoothstep(.33,.56,field.b);
    pigment*=.73+.34*light;
    pigment=mix(pigment,art*(.82+.18*ink),imageMix);
    colour=vec4(pigment*source.a,source.a);
}

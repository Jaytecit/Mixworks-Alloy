#version 330
#define COMPOSITION 3
// Original Gray-Scott system: R nutrient U, G organism V, B image food.
// Six explicit updates per fixed tick; concentrations are never display RGB.
uniform vec2 resolution;
uniform sampler2D input0;
uniform sampler2D userImage;
uniform int hasImage;
uniform float simulationReset, stepDelta, simulationTime;
uniform float bass, mid, trebleHit, bassHit, feedback, motion, pulse, scale, imageMix;
out vec4 fragColor;
vec4 media(vec2 uv) {
    vec4 c=hasImage!=0?sampleFittedMedia(uv):vec4(0);
    vec4 s=sampleCutout(materialDomain(uv));return vec4(mix(c.rgb,s.rgb,s.a),max(c.a,s.a));
}
float food(vec2 uv) {vec4 c=media(uv);return smoothstep(.13,.8,dot(c.rgb,vec3(.2126,.7152,.0722)))*c.a;}
float hash(vec2 p) {return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main() {
    vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
    vec2 imageUV=uv;
#if COMPOSITION == 4
    vec2 pp=(uv-.5)*vec2(1.78,1.);imageUV=vec2(atan(pp.y,pp.x)/6.283+.5,length(pp));
#elif COMPOSITION == 5
    imageUV=fract(uv*vec2(3,2));
#elif COMPOSITION == 7
    imageUV.x=fract(uv.x+floor(uv.y*5.)*.17);
#endif
    float imageFood=food(imageUV)*(.35+.65*imageMix);
    vec2 sampleUV=uv;
#if COMPOSITION == 1
    sampleUV-=vec2(-(uv.y-.5),uv.x-.5)*stepDelta*.018;
#elif COMPOSITION == 2
    px*=vec2(1.8,.65);
#elif COMPOSITION == 3
    imageFood=abs(food(uv+px*2.)-food(uv-px*2.))*3.;
#elif COMPOSITION == 6
    sampleUV-=vec2(.16,sin(uv.x*8.+simulationTime*.2)*.08)*px;
#elif COMPOSITION == 7
    sampleUV-=vec2((mod(floor(uv.y*5.),2.)*2.-1.)*.12,0)*px;
#endif
    if(simulationReset>.5) {
        vec2 cell=floor(gl_FragCoord.xy/12.);
        vec2 centre=.25+.5*vec2(hash(cell+1.7),hash(cell+9.3));
        float spots=step(.72,hash(cell))*(1.-smoothstep(.16,.28,length(fract(gl_FragCoord.xy/12.)-centre)));
        // With artwork, colonies start in its material instead of a screen-wide grid.
        float seed=max(spots*((hasImage!=0||hasCutout!=0)?imageFood*.3:1.),imageFood*(.38+.38*feedback));
        fragColor=vec4(1.-.65*seed,.52*seed,imageFood,1.);return;
    }
    vec2 c=texture(input0,sampleUV).rg;
    vec2 lap=-c;
    lap+=.2*(texture(input0,sampleUV+vec2(px.x,0)).rg+texture(input0,sampleUV-vec2(px.x,0)).rg+texture(input0,sampleUV+vec2(0,px.y)).rg+texture(input0,sampleUV-vec2(0,px.y)).rg);
    lap+=.05*(texture(input0,sampleUV+px).rg+texture(input0,sampleUV-px).rg+texture(input0,sampleUV+vec2(px.x,-px.y)).rg+texture(input0,sampleUV+vec2(-px.x,px.y)).rg);
    float feed=.023+float(COMPOSITION)*.0013+.008*mid+.004*imageFood*feedback;
    float kill=.052+float(COMPOSITION%3)*.0025+.005*(1.-bass)+.002*scale;
    float reaction=c.x*c.y*c.y;
    float rate=(.65+motion*.65)*(stepDelta*60.);
    vec2 change=vec2(.20,.10)*lap+vec2(-reaction+feed*(1.-c.x),reaction-(feed+kill)*c.y);
    // Sparse replenishment lets existing contours split and erode between attacks.
    float injection=imageFood*feedback*(.00008+.0025*trebleHit*pulse);
    change+=vec2(-injection*.6,injection);
    float pores=smoothstep(.4,.8,hash(floor(gl_FragCoord.xy/7.)));
    change+=vec2(1.,-1.)*c.y*pores*bassHit*pulse*.013;
    fragColor=vec4(clamp(c+rate*change,0.,1.),imageFood,1.);
}

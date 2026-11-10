#version 330
uniform vec2 resolution;
uniform sampler2D input0,input1,input2;
uniform float feedback;
out vec4 colour;

void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec4 field=texture(input0,uv);
    vec4 live=texture(input1,uv);
    vec4 wake=texture(input2,uv)*feedback;
    float pool=smoothstep(.26,.67,field.b);
    vec3 paper=mix(vec3(.94,.86,.72),vec3(.92,.53,.34),pool);
    float tide=smoothstep(.61,.77,field.b);
    paper=mix(paper,vec3(.22,.53,.46),.65*tide);
    paper=mix(paper,vec3(.99,.92,.81),.28*pow(field.a,5.));
    vec2 p=(uv-.5)*vec2(1.42,1.);
    paper*=1.-.22*smoothstep(.25,.79,length(p));
    vec3 bed=wake.rgb+paper*(1.-wake.a);
    colour=vec4(live.rgb+bed*(1.-live.a),1.);
}

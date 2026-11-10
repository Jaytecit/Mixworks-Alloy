#version 330
uniform vec2 resolution;
uniform sampler2D input0,input1,input2;
uniform int historyValid;
uniform float deltaTime,motion,bassHit,kickImpact,pulse;
out vec4 colour;

void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    if(historyValid==0){colour=vec4(0);return;}
    if(deltaTime==0.){colour=texture(input2,uv);return;}
    float push=pulse*max(.62*bassHit,kickImpact);
    vec2 metric=vec2(resolution.x/resolution.y,1.);
    vec2 p=(uv-.5)*metric;
    vec2 drift=texture(input1,uv).rg;
    float angle=deltaTime*(.13+.18*push);
    mat2 spin=mat2(cos(angle),sin(angle),-sin(angle),cos(angle));
    vec2 from=.5+(spin*p*exp(-deltaTime*(.16+.25*push))
        +deltaTime*(drift*(.19+.18*motion)+vec2(.018,-.014)))/metric;
    vec4 previous=vec4(0);
    if(all(greaterThanEqual(from,vec2(0)))&&all(lessThanEqual(from,vec2(1))))
        previous=texture(input2,from)*exp(-deltaTime*1.25);
    // Deposit premultiplied pigment over the advected reservoir. The paper is
    // drawn later, so its brightness never accumulates in the feedback loop.
    vec4 fresh=texture(input0,uv)*(1.-exp(-deltaTime*(2.4+4.2*push)));
    colour=fresh+previous*(1.-fresh.a);
}

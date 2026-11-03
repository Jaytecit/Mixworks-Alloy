"""Owned GPU state handoffs. Colour is never reinterpreted as velocity/chemistry.

Snapshots belong to the incoming clip, not the outgoing scene's GL resources.
Compatible state is copied exactly; different simulations use named conversions.
"""
import math
from OpenGL import GL as gl
from original_visual_gl import Target, program

PARTICLE='particle-position-velocity-v1'
ATLAS='premultiplied-source-atlas-v1'
FLOW='velocity-pigment-relief-v1'
REACTION='gray-scott-uv-v1'
UV='advected-material-uv-v1'
PIGMENTS={'posterised-mirage-premultiplied-pigment-v1','solar-wash-premultiplied-pigment-v1','solar-wash-delayed-pigment-v1'}

def adapter(semantic, available):
    if semantic in available:return 'exact'
    if semantic==PARTICLE:return 'image-to-particles'
    if semantic==FLOW:return 'image-to-flow'
    if semantic==REACTION:return 'image-to-growth'
    if semantic in PIGMENTS or semantic==ATLAS:return 'image-to-pigment'
    # Material coordinates are not colour or velocity; keep their native initial UV.
    return None

def transition_phase(progress,pivot):
    if progress<=0.:return 0.
    if progress>=1.:return 1.
    return .5*progress/max(pivot,.001) if progress<pivot else .5+.5*(progress-pivot)/max(1.-pivot,.001)

SEED='''#version 330
uniform sampler2D sourceImage, velocityImage;
uniform vec2 resolution;
uniform int conversion, velocityKind;
out vec4 colour;
float lum(vec2 uv){return dot(texture(sourceImage,clamp(uv,0.,1.)).rgb,vec3(.2126,.7152,.0722));}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,px=1./vec2(textureSize(sourceImage,0));
 vec4 c=texture(sourceImage,uv);
 vec2 g=vec2(lum(uv+vec2(px.x,0))-lum(uv-vec2(px.x,0)),lum(uv+vec2(0,px.y))-lum(uv-vec2(0,px.y)));
 vec2 velocity=vec2(-g.y,g.x)*.025;
 if(velocityKind==1)velocity=texture(velocityImage,uv).xy/vec2(textureSize(velocityImage,0));
 // Particle velocity is UV/sec, indexed by original material cell; not a fluid grid.
 if(velocityKind==2)velocity=texture(velocityImage,uv).zw;
 float l=clamp(lum(uv),0.,1.);
 if(conversion==1){colour=vec4(uv,clamp(velocity,vec2(-.5),vec2(.5)));return;}
 if(conversion==2){colour=vec4(clamp(velocity*resolution,vec2(-80),vec2(80)),l,l*.7);return;}
 if(conversion==3){float seed=smoothstep(.035,.65,l);colour=vec4(1.-.65*seed,.52*seed,l,1.);return;}
 colour=vec4(clamp(c.rgb,0.,1.),clamp(c.a,0.,1.));
}
'''

TRANSPORT='''#version 330
uniform sampler2D sourceImage,velocityImage;
uniform vec2 resolution;
uniform float dt,energy,treble;
uniform int velocityKind;
out vec4 colour;
float l(vec2 uv){return dot(texture(sourceImage,clamp(uv,0.,1.)).rgb,vec3(.2126,.7152,.0722));}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,px=1./resolution;
 vec2 grad=vec2(l(uv+vec2(px.x,0))-l(uv-vec2(px.x,0)),l(uv+vec2(0,px.y))-l(uv-vec2(0,px.y)));
 vec2 v=vec2(-grad.y,grad.x)*(.05+.12*treble);
 if(velocityKind==1)v+=texture(velocityImage,uv).xy/vec2(textureSize(velocityImage,0))*.45;
 if(velocityKind==2)v+=texture(velocityImage,uv).zw*.25;
 v=clamp(v,vec2(-.2),vec2(.2));
 vec2 back=uv-v*dt*(.25+energy);
 // Bounded material transport, with no fresh framebuffer accumulation or blur.
 colour=texture(sourceImage,clamp(back,px*.5,1.-px*.5));
}
'''

ASSEMBLE='''#version 330
uniform sampler2D sourceImage,incomingImage,velocityImage;
uniform vec2 resolution;
uniform float progress;
uniform int velocityKind,choreography;
out vec4 colour;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 if(progress<=0.){colour=texture(sourceImage,uv);return;}
 if(progress>=1.){colour=texture(incomingImage,uv);return;}
 vec2 cell=floor(uv*vec2(64,36));float seed=hash(cell);
 float arc=sin(progress*3.14159265);
 vec2 v=vec2(0);
 if(velocityKind==1)v=texture(velocityImage,uv).xy/vec2(textureSize(velocityImage,0));
 if(velocityKind==2)v=texture(velocityImage,uv).zw;
 // Fragments carry outgoing material along its inherited motion, then assemble.
 vec2 direction=normalize(uv-.5+vec2(.001));
 vec2 displacement=arc*(clamp(v,vec2(-.3),vec2(.3))*.35+direction*(seed-.5)*.065);
 vec2 oldUV=clamp(uv-displacement,0.,1.),newUV=clamp(uv+displacement*(1.-progress),0.,1.);
 // Local material replacement (small edge antialias), not a whole-picture dissolve.
 float region=seed;
 if(choreography==1){
  region=clamp(uv.x*.75+uv.y*.25+.09*sin(uv.y*15.),0.,1.);
  oldUV=clamp(uv-vec2(arc*.12*sin(uv.y*9.),arc*.035),0.,1.);
  newUV=clamp(uv+vec2(arc*.08*sin(uv.y*9.),-arc*.035),0.,1.);
 }else if(choreography==2){
  vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
  region=clamp(length(p)/length(vec2(resolution.x/resolution.y,1.)*.5),0.,1.);
  vec2 tangent=vec2(-direction.y,direction.x)*arc*.13;
  oldUV=clamp(oldUV-tangent,0.,1.);newUV=clamp(newUV+tangent*(1.-progress),0.,1.);
 }else if(choreography==3){
  float angle=atan(uv.y-.5,uv.x-.5);
  region=clamp(length(uv-.5)*1.2+.18*sin(angle*6.+length(uv-.5)*9.)+.12,0.,1.);
  oldUV=clamp(uv-direction*arc*.12,0.,1.);newUV=clamp(uv+direction*arc*.08,0.,1.);
 }
 float takeover=smoothstep(region-.045,region+.045,progress*1.14-.07);
 colour=mix(texture(sourceImage,oldUV),texture(incomingImage,newUV),takeover);
}
'''

class GenerativeHandoff:
    def __init__(self,outgoing,pair,clock):
        self.pair,self.clock,self.progress=pair,clock,0.
        self.age,self.accumulator,self.tail=0.,0.,0.
        self.source_family=outgoing.particle_family
        self.source_hierarchy_mode=getattr(outgoing,'hierarchy_mode',-1)
        self.source_controls=dict(outgoing.last_controls)
        # Stable scene-pair choreography; clip UUIDs and Python hash randomisation
        # must not change a saved join's appearance on reopen.
        self.choreography=sum(ord(c) for identity in pair[1] for c in identity[1]) % 4
        self.snapshots={};self.targets=[];self.programs=[];self.seeded=[]
        self.material=self.appearance=None
        try:
            # Retain the final outgoing appearance as material, and typed state separately.
            self.appearance=self.copy(outgoing.final,format='rgba16f')
            for entry in outgoing.passes:
                if entry.get('state'):
                    semantic=entry['semantic']
                    if semantic in self.snapshots:self.snapshots[semantic].close();self.targets.remove(self.snapshots[semantic])
                    self.snapshots[semantic]=self.copy(entry['target'],format=entry['format'])
            self.material=Target(512,288,buffers=2,format='rgba16f');self.targets.append(self.material)
            self.blit(self.appearance,self.material)
            self.velocity=self.snapshots.get(FLOW) or self.snapshots.get(PARTICLE)
            self.velocity_kind=1 if FLOW in self.snapshots else 2 if PARTICLE in self.snapshots else 0
            self.seed_program=program(SEED);self.programs.append(self.seed_program)
            self.transport_program=program(TRANSPORT);self.programs.append(self.transport_program)
            self.assemble_program=program(ASSEMBLE);self.programs.append(self.assemble_program)
        except Exception:self.close();raise

    @staticmethod
    def blit(source,destination):
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER,source.fbos[source.front])
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER,destination.fbos[destination.front])
        gl.glBlitFramebuffer(0,0,source.width,source.height,0,0,destination.width,destination.height,gl.GL_COLOR_BUFFER_BIT,gl.GL_NEAREST)

    def copy(self,source,format='rgba32f',size=None):
        target=Target(*(size or (source.width,source.height)),format=format,filter='nearest' if format=='rgba32f' else 'linear')
        self.targets.append(target);self.blit(source,target);return target

    @property
    def byte_size(self):return sum(t.byte_size for t in self.targets)

    @property
    def strength(self):
        return max(.35,1.-self.progress)*math.exp(-self.tail*.55)

    def setup(self,shader,target,bindings,values=None,ints=None):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front]);gl.glViewport(0,0,target.width,target.height)
        gl.glUseProgram(shader);gl.glUniform2f(gl.glGetUniformLocation(shader,'resolution'),target.width,target.height)
        for unit,(name,texture) in enumerate(bindings):
            gl.glActiveTexture(gl.GL_TEXTURE0+unit);gl.glBindTexture(gl.GL_TEXTURE_2D,texture)
            gl.glUniform1i(gl.glGetUniformLocation(shader,name),unit)
        for name,value in (values or {}).items():gl.glUniform1f(gl.glGetUniformLocation(shader,name),value)
        for name,value in (ints or {}).items():gl.glUniform1i(gl.glGetUniformLocation(shader,name),value)
        gl.glDrawArrays(gl.GL_TRIANGLES,0,3)

    def seed(self,scene):
        for entry in scene.passes:
            if not entry.get('state'):continue
            semantic=entry['semantic'];route=adapter(semantic,self.snapshots)
            if route is None:continue
            target=entry['target']
            if route=='exact':self.blit(self.snapshots[semantic],target)
            else:
                # Particle capture uses a compatible source atlas when one exists;
                # all other cross-family mappings start from screen-space material.
                source=self.appearance
                if semantic==ATLAS and ATLAS in self.snapshots:source=self.snapshots[ATLAS]
                conversion={'image-to-particles':1,'image-to-flow':2,'image-to-growth':3,'image-to-pigment':4}[route]
                self.setup(self.seed_program,target,[('sourceImage',source.textures[source.front]),
                    ('velocityImage',self.velocity.textures[0] if self.velocity else 0)],ints={'conversion':conversion,'velocityKind':self.velocity_kind})
            self.seeded.append({'pass':entry['id'],'semantic':semantic,'route':route})

    def advance(self,clock,progress,levels):
        dt=max(0.,min(.3,clock-self.clock));self.clock=clock
        self.progress=progress;self.tail+=dt if progress>=1. else 0.
        self.accumulator+=dt
        steps=min(18,int((self.accumulator+1e-9)*60))
        for _ in range(steps):
            old=self.material.textures[self.material.front];self.material.front=1-self.material.front
            self.setup(self.transport_program,self.material,[('sourceImage',old),('velocityImage',self.velocity.textures[0] if self.velocity else 0)],
                values={'dt':1/60,'energy':levels['energy'],'treble':levels['treble']},ints={'velocityKind':self.velocity_kind})
            self.accumulator=max(0.,self.accumulator-1/60);self.age+=1/60

    def render(self,incoming,target,progress,readback):
        self.setup(self.assemble_program,target,[('sourceImage',self.appearance.textures[self.appearance.front]),
            ('incomingImage',incoming.final.textures[incoming.final.front]),('velocityImage',self.velocity.textures[0] if self.velocity else 0)],
            values={'progress':progress},ints={'velocityKind':self.velocity_kind,'choreography':self.choreography})
        return bytes(gl.glReadPixels(0,0,target.width,target.height,gl.GL_BGRA,gl.GL_UNSIGNED_BYTE)) if readback else b''

    def close(self):
        for target in self.targets:target.close()
        for shader in self.programs:gl.glDeleteProgram(shader)
        self.targets=[];self.programs=[];self.snapshots={}

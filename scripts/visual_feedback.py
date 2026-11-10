"""Owned recurrent transparent artwork; shared by original and MilkDrop scenes."""
from OpenGL import GL as gl
from original_visual_gl import Target, program

SPRITE_FEEDBACK = '''#version 330
uniform sampler2D previous, preset, sprite, nextSprite;
uniform vec2 resolution, outputSize, spriteSize, nextSpriteSize;
uniform float seconds, beat, dt, bass, energy, blend, sourceScale;
uniform int hasSprite, initialise;
out vec4 colour;
vec4 artwork(sampler2D image,vec2 size,vec2 p) {
    // Fit both dimensions, retaining transparent borders and wide artwork.
    vec2 uv=p*outputSize/(size*min(outputSize.x/size.x,outputSize.y/size.y)*sourceScale)+.5;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec4(0);
    // Sources are premultiplied BEFORE texture filtering, including hidden RGB
    // at transparent edges. Black sprites retain their actual alpha shape too.
    return texture(image,uv);
}
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 field=texture(preset,uv).rgb;
    vec2 pixel=1./outputSize;
    float x=length(texture(preset,uv+vec2(pixel.x*3.,0)).rgb)-length(texture(preset,uv-vec2(pixel.x*3.,0)).rgb);
    float y=length(texture(preset,uv+vec2(0,pixel.y*3.)).rgb)-length(texture(preset,uv-vec2(0,pixel.y*3.)).rgb);
    vec2 p=(uv-.5)*vec2(outputSize.x/outputSize.y,1.);
    float angle=dt*(.12+.36*bass);
    p=mat2(cos(angle),-sin(angle),sin(angle),cos(angle))*p*(1.-dt*(.16+.17*energy));
    vec2 oldUV=.5+p/vec2(outputSize.x/outputSize.y,1.)+dt*(vec2(y,-x)*.18+(field.rg-field.gb)*.035);
    vec4 old=vec4(0);
    if(all(greaterThanEqual(oldUV,vec2(0)))&&all(lessThanEqual(oldUV,vec2(1))))old=texture(previous,oldUV);
    old*=exp(-dt*.42);
    vec4 fresh=vec4(0);
    if(hasSprite!=0) {
        vec2 q=uv-.5-vec2(.09*sin(seconds*.27),.07*cos(seconds*.31));
        float a=.13*sin(seconds*.21);
        q=mat2(cos(a),-sin(a),sin(a),cos(a))*q/(.86+.22*bass);
        fresh=mix(artwork(sprite,spriteSize,q),artwork(nextSprite,nextSpriteSize,q),blend);
        float pulse=pow(max(0.,cos(beat*6.283185)),6.);
        fresh*=initialise!=0 ? 1. : 1.-exp(-dt*(.45+2.6*energy+2.*pulse));
    }
    colour=clamp(fresh+old*(1.-fresh.a),0.,1.);
}
'''

SPRITE_DISPLAY = '''#version 330
uniform sampler2D preset, pigment;
uniform vec2 resolution;
out vec4 colour;
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    vec4 ink=texture(pigment,uv);
    vec3 base=texture(preset,uv).rgb;
    // Pigment is already premultiplied: one source-over operation preserves
    // sprite colour/coverage without another opacity or lighting multiplier.
    colour=vec4(clamp(ink.rgb+base*(1.-ink.a),0.,1.),1.);
}
'''

SPRITE_MATERIAL = '''#version 330
uniform sampler2D pigment;
uniform vec2 resolution;
out vec4 colour;
void main(){
    vec4 ink=texture(pigment,gl_FragCoord.xy/resolution);
    colour=vec4(ink.rgb/max(ink.a,.0001),ink.a);
}
'''


class SpriteFeedback:
    """Bounded Alloy-owned pigment, advected by projectM's rendered field.

    Images enter recurrent state, never the projectM preset or its internal
    buffers. Removal stops feeding but preserves evolving trails until decay.
    """
    def __init__(self, width, height, vao):
        self.vao, self.state, self.base = vao, None, None
        self.feed, self.display = 0, 0
        self.art,self.art_program=None,0
        self.frames=0
        try:
            scale=min(1.,512/width,288/height)
            self.state=Target(max(1,round(width*scale)),max(1,round(height*scale)),buffers=2,hdr=True)
            self.base=Target(width,height)
            self.feed=program(SPRITE_FEEDBACK)
            self.display=program(SPRITE_DISPLAY)
            self.locations={p:{name:gl.glGetUniformLocation(p,name) for name in
                ('previous','preset','sprite','nextSprite','pigment','resolution','outputSize',
                 'spriteSize','nextSpriteSize','seconds','beat','dt','bass','energy','blend','sourceScale','hasSprite','initialise')}
                for p in (self.feed,self.display)}
        except BaseException:
            self.close()
            raise

    @property
    def byte_size(self):
        return self.base.byte_size+self.state.byte_size+(self.art.byte_size if self.art else 0)

    def render(self, target, media, seconds, beat, dt, levels, *, material=False):
        if material and dt<=0. and self.art is not None:
            return self.art.textures[0],(self.art.width,self.art.height)
        texture,size,next_texture,next_size,blend=media
        gl.glBindVertexArray(self.vao)
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER,target.fbos[0])
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER,self.base.fbos[0])
        gl.glBlitFramebuffer(0,0,target.width,target.height,0,0,target.width,target.height,gl.GL_COLOR_BUFFER_BIT,gl.GL_NEAREST)
        front=self.state.front; back=1-front
        def draw(p, output, inputs):
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,output)
            gl.glUseProgram(p)
            for unit,(name,tex) in enumerate(inputs):
                gl.glActiveTexture(gl.GL_TEXTURE0+unit)
                gl.glBindTexture(gl.GL_TEXTURE_2D,tex)
                gl.glUniform1i(self.locations[p][name],unit)
        draw(self.feed,self.state.fbos[back],(('previous',self.state.textures[front]),
            ('preset',self.base.textures[0]),('sprite',texture),('nextSprite',next_texture or texture)))
        gl.glViewport(0,0,self.state.width,self.state.height)
        loc=self.locations[self.feed]
        for name,value in dict(seconds=seconds,beat=beat,dt=dt,bass=levels['bass'],energy=levels['energy'],blend=blend,sourceScale=1. if material else .68).items():
            gl.glUniform1f(loc[name],value)
        for name,value in dict(resolution=(self.state.width,self.state.height),outputSize=(target.width,target.height),
                               spriteSize=size,nextSpriteSize=next_size if next_texture else size).items():
            gl.glUniform2f(loc[name],*value)
        gl.glUniform1i(loc['hasSprite'],int(bool(texture)))
        gl.glUniform1i(loc['initialise'],int(self.frames==0))
        gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
        self.frames+=1
        self.state.front=back
        if material:
            if self.art is None:
                self.art=Target(self.state.width,self.state.height)
                self.art_program=program(SPRITE_MATERIAL)
                self.locations[self.art_program]={name:gl.glGetUniformLocation(self.art_program,name) for name in ('pigment','resolution')}
            draw(self.art_program,self.art.fbos[0],(('pigment',self.state.textures[back]),))
            gl.glViewport(0,0,self.art.width,self.art.height)
            gl.glUniform2f(self.locations[self.art_program]['resolution'],self.art.width,self.art.height)
            gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
            gl.glActiveTexture(gl.GL_TEXTURE0)
            return self.art.textures[0],(self.art.width,self.art.height)
        draw(self.display,target.fbos[0],(('preset',self.base.textures[0]),('pigment',self.state.textures[back])))
        gl.glViewport(0,0,target.width,target.height)
        gl.glUniform2f(self.locations[self.display]['resolution'],target.width,target.height)
        gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
        gl.glActiveTexture(gl.GL_TEXTURE0)

    def close(self):
        for target in (self.state,self.base,self.art):
            if target:target.close()
        for p in (self.feed,self.display,self.art_program):
            if p:gl.glDeleteProgram(p)
        self.state,self.base,self.feed,self.display=None,None,0,0
        self.art,self.art_program=None,0


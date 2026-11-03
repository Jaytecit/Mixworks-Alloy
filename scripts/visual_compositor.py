"""Alloy-owned GPU scene transitions; borrows final textures from live pipelines."""
from OpenGL import GL as gl
from OpenGL.GL.shaders import compileProgram, compileShader

TRANSITIONS = ('dissolve', 'zoom', 'radial', 'sweep', 'cut', 'sweep-reverse')

FRAGMENT = '''#version 330
uniform sampler2D outgoingImage, incomingImage;
uniform vec2 resolution, outgoingSize, incomingSize;
uniform float progress, pivot;
uniform int effect;
out vec4 colour;
vec4 fitted(sampler2D source, vec2 size, vec2 uv) {
    float scale=min(resolution.x/size.x,resolution.y/size.y);
    vec2 p=(uv-.5)*resolution/(size*scale)+.5;
    if(any(lessThan(p,vec2(0))) || any(greaterThan(p,vec2(1)))) return vec4(0,0,0,1);
    return vec4(texture(source,p).rgb,1.);
}
void main() {
    vec2 uv=gl_FragCoord.xy/resolution;
    if(progress<=0.) { colour=fitted(outgoingImage,outgoingSize,uv); return; }
    if(progress>=1.) { colour=fitted(incomingImage,incomingSize,uv); return; }
    float p=progress<pivot ? .5*progress/max(pivot,.001) : .5+.5*(progress-pivot)/max(1.-pivot,.001);
    float q=p*p*(3.-2.*p);
    vec2 a=uv,b=uv; float weight=q;
    if(effect==1) { a=(uv-.5)*(1.+.32*q)+.5; b=(uv-.5)/(1.+.32*(1.-q))+.5; }
    if(effect==2) {
        vec2 aspect=vec2(resolution.x/resolution.y,1.);
        float radius=length((uv-.5)*aspect)/length(.5*aspect);
        weight=smoothstep(radius-.04,radius+.04,q*1.08-.04);
    }
    if(effect==3 || effect==5) {
        float edge=effect==3 ? uv.x : 1.-uv.x;
        weight=smoothstep(edge-.035,edge+.035,q*1.07-.035);
    }
    if(effect==4) weight=step(pivot,progress);
    colour=mix(fitted(outgoingImage,outgoingSize,a),fitted(incomingImage,incomingSize,b),weight);
}
'''


class VisualCompositor:
    def __init__(self, renderer, width, height):
        if hasattr(renderer, 'target_class'):
            Target, VERTEX = renderer.target_class, renderer.vertex_shader
        else:
            from renderer import Target, VERTEX
        self.renderer, self.width, self.height = renderer, width, height
        self.program, self.target = 0, None
        shaders = []
        try:
            shaders.append(compileShader(VERTEX, gl.GL_VERTEX_SHADER))
            shaders.append(compileShader(FRAGMENT, gl.GL_FRAGMENT_SHADER))
            self.program = compileProgram(*shaders, validate=False)
            self.locations = {name: gl.glGetUniformLocation(self.program, name) for name in
                              ('outgoingImage', 'incomingImage', 'resolution', 'outgoingSize', 'incomingSize', 'progress', 'pivot', 'effect')}
            self.target = Target(width, height, buffers=1)
        except Exception:
            self.close()
            raise
        finally:
            for shader in shaders:
                gl.glDeleteShader(shader)

    def render(self, outgoing, incoming, progress, transition, pivot, readback=True):
        gl.glBindVertexArray(self.renderer.vao)
        gl.glUseProgram(self.program)
        for unit, (name, pipeline) in enumerate((('outgoing', outgoing), ('incoming', incoming))):
            gl.glActiveTexture(gl.GL_TEXTURE0 + unit)
            gl.glBindTexture(gl.GL_TEXTURE_2D, pipeline.final.textures[pipeline.final.front])
            gl.glUniform1i(self.locations[name+'Image'], unit)
            gl.glUniform2f(self.locations[name+'Size'], pipeline.width, pipeline.height)
        gl.glUniform2f(self.locations['resolution'], self.width, self.height)
        gl.glUniform1f(self.locations['progress'], progress)
        gl.glUniform1f(self.locations['pivot'], pivot)
        gl.glUniform1i(self.locations['effect'], TRANSITIONS.index(transition))
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.target.fbos[0])
        gl.glViewport(0, 0, self.width, self.height)
        gl.glDrawArrays(gl.GL_TRIANGLES, 0, 3)
        return bytes(gl.glReadPixels(0, 0, self.width, self.height, gl.GL_BGRA, gl.GL_UNSIGNED_BYTE)) if readback else b''

    def close(self):
        if self.target:
            self.target.close()
            self.target = None
        if self.program:
            gl.glDeleteProgram(self.program)
            self.program = 0

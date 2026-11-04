"""Small original GL resource layer; no compatibility runtime imports."""
from pathlib import Path
import json
import math
import re
from visual_catalogue import catalogue, CONTROL_DEFAULTS
import glfw
from OpenGL import GL as gl
from OpenGL.GL.shaders import compileProgram, compileShader

ASSET_ROOT = Path(__file__).resolve().parent
if not (ASSET_ROOT/'original-scenes.json').is_file(): ASSET_ROOT = ASSET_ROOT.parent/'visuals/original'
SCENE_KEYS = tuple(catalogue(ASSET_ROOT))

MEDIA_GLSL = '''
uniform sampler2D nextImage;
uniform float imageBlend, imageZoom, imageRotation;
uniform float travelTime;
uniform float materialMotion, materialMode;
uniform vec2 mediaAspect, nextMediaAspect;
uniform sampler2D cutoutImage, nextCutoutImage;
uniform int hasCutout;
uniform float cutoutBlend;
uniform vec2 cutoutAspect, nextCutoutAspect;
uniform sampler2D handoffMaterial;
uniform float handoffStrength;
vec2 materialDomain(vec2 uv);
vec4 inheritedMaterial(vec2 uv,vec4 nativeColour) {
    if(handoffStrength<=0.)return nativeColour;
    vec4 c=texture(handoffMaterial,clamp(uv,0.,1.));
    c.rgb/=max(c.a,.0001);
    return mix(nativeColour,c,handoffStrength);
}
vec4 inheritedTextureLod(sampler2D tex,vec2 uv,float lod) {
    return inheritedMaterial(uv,textureLod(tex,materialDomain(uv),lod));
}
vec4 cutoutSample(sampler2D tex,vec2 uv,vec2 aspect) {
    uv=(uv-.5)*aspect+.5;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec4(0);
    return texture(tex,uv);
}
vec4 sampleCutout(vec2 uv) {
    if(hasCutout==0)return vec4(0);
    vec4 a=cutoutSample(cutoutImage,uv,cutoutAspect),b=cutoutSample(nextCutoutImage,uv,nextCutoutAspect);
    vec4 c=mix(vec4(a.rgb*a.a,a.a),vec4(b.rgb*b.a,b.a),cutoutBlend);
    return vec4(c.rgb/max(c.a,.0001),c.a*(1.-handoffStrength));
}
vec2 materialDomain(vec2 uv) {
    float t=travelTime*(.12+.55*materialMotion);
    int method=int(floor(materialMode*3.+.5));
    if(method==1) uv+=.045*sin(uv.yx*12.+vec2(t,-t*.73));
    if(method==2) {
        vec2 cell=floor(uv*4.), p=fract(uv*4.)-.5;
        float a=t*.35+sin(dot(cell,vec2(1.7,2.1)))*.5;
        uv=(cell+mat2(cos(a),-sin(a),sin(a),cos(a))*p+.5)/4.;
    }
    if(method==3) {
        float band=floor(uv.y*9.);
        uv.x+=.12*sin(t*.6+band*.72); uv.y+=.025*sin(uv.x*14.-t);
    }
    return uv;
}
vec4 sampleMedia(vec2 uv) {
    uv=materialDomain(uv);
    vec2 materialUV=uv;
    float a=(imageRotation-.5)*6.28318;
    uv=mat2(cos(a),-sin(a),sin(a),cos(a))*(uv-.5)/(.65+imageZoom);
    vec2 aUV=uv*mediaAspect+.5, bUV=uv*nextMediaAspect+.5;
    vec4 aC=texture(userImage,clamp(aUV,0.,1.));
    vec4 bC=texture(nextImage,clamp(bUV,0.,1.));
    return inheritedMaterial(materialUV,mix(aC,bC,imageBlend));
}
// Image-led scenes use an explicit fitted projection, not material UVs.
vec4 sampleFittedMedia(vec2 uv) {
    uv=materialDomain(uv);
    vec2 p=(uv-.5)/(.90+.08*imageZoom);
    vec2 aUV=p*(mediaAspect/min(mediaAspect.x,mediaAspect.y))+.5;
    vec2 bUV=p*(nextMediaAspect/min(nextMediaAspect.x,nextMediaAspect.y))+.5;
    vec4 a=vec4(0),b=vec4(0);
    if(all(greaterThanEqual(aUV,vec2(0)))&&all(lessThanEqual(aUV,vec2(1))))a=texture(userImage,aUV);
    if(all(greaterThanEqual(bUV,vec2(0)))&&all(lessThanEqual(bUV,vec2(1))))b=texture(nextImage,bUV);
    vec4 c=mix(vec4(a.rgb*a.a,a.a),vec4(b.rgb*b.a,b.a),imageBlend);
    return inheritedMaterial(uv,vec4(c.rgb/max(c.a,.0001),c.a));
}
'''

def scene_source(text):
    # Inject only into the main shader, after its own sampler declaration.
    text=text.replace('texture(userImage,','sampleMedia(')
    text=text.replace('textureLod(userImage,','inheritedTextureLod(userImage,')
    text=text.replace('textureLod(nextImage,','inheritedTextureLod(nextImage,')
    text=text.replace('vec4 pmSample(sampler2D tex,vec2 uv) {',
                      'vec4 pmSample(sampler2D tex,vec2 uv) {\n    uv=materialDomain(uv);')
    text=text.replace('vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){',
                      'vec4 alphaArt(sampler2D tex,vec2 uv,vec2 aspect){uv=materialDomain(uv);')
    if 'vec4 cutoutArt(' in text:
        text=text.replace('return vec4(c.rgb/max(c.a,.00001),c.a);',
                          'return vec4(c.rgb/max(c.a,.00001),c.a*(1.-handoffStrength));')
    text=text.replace('sceneTime*(.19+.26*motion)','travelTime*.48')
    text=text.replace('sceneTime*(.65+motion*.9)','travelTime*1.7')
    text=text.replace('sceneTime*(.45+motion*.8)','travelTime*1.3')
    text=text.replace('sceneTime*(.13+.2*motion)','travelTime*.35')
    marker='uniform sampler2D userImage;'
    return text.replace(marker,marker+'\n'+MEDIA_GLSL)

VERTEX = '''#version 330
void main() {
    vec2 point = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(point * 2.0 - 1.0, 0.0, 1.0);
}
'''

# A common display stage makes every method available to every scene, including
# simulations. It never feeds back into state or moves a structural camera.
LOOK_GLSL = '''#version 330
uniform sampler2D input0;
uniform vec2 resolution;
uniform float viewPattern,finishGlow,paletteStyle,paletteAmount,hue;
uniform float travelTime,bassImpulse,midImpulse,trebleImpulse,bassHit,midHit,trebleHit,intensity;
out vec4 colour;
vec2 viewUV(vec2 uv){
    int mode=int(floor(viewPattern*4.+.5));
    if(mode==1) return vec2(abs(uv.x-.5)*2.,uv.y);
    if(mode==2) return abs(uv-.5)*2.;
    if(mode>=3){
        vec2 p=(uv-.5)*vec2(resolution.x/resolution.y,1.);
        float n=mode==3?6.:10., wedge=6.283185/n;
        float a=abs(mod(atan(p.y,p.x)+wedge*.5,wedge)-wedge*.5);
        p=length(p)*vec2(cos(a),sin(a));
        return clamp(.5+p*vec2(resolution.y/resolution.x,1.)*.88,0.,1.);
    }
    return uv;
}
vec3 reflectedScene(vec2 uv){
    int mode=int(floor(viewPattern*4.+.5));
    if(mode<3)return texture(input0,uv).rgb;
    // Keep the original angular scale. Gather source sectors with smooth
    // highlight weighting, so asymmetric forms survive without radial smearing.
    vec2 aspect=vec2(resolution.x/resolution.y,1.);
    vec2 p=(uv-.5)*aspect;float n=mode==3?6.:10.;
    vec3 sum=vec3(0);float weight=0.;
    for(int j=0;j<10;j++)if(float(j)<n){
        float a=float(j)*6.283185/n;mat2 r=mat2(cos(a),-sin(a),sin(a),cos(a));
        for(int side=0;side<2;side++){
            vec2 q=r*(p*vec2(1.,side==0?1.:-1.));
            vec2 sampleUV=clamp(.5+q/aspect,0.,1.);
            vec3 c=texture(input0,sampleUV).rgb;
            float w=.005+pow(max(c.r,max(c.g,c.b)),5.);
            sum+=c*w;weight+=w;
        }
    }
    return sum/max(weight,.00001);
}
vec3 authoredPalette(vec3 source,int style){
    float value=max(source.r,max(source.g,source.b));
    float low=min(source.r,min(source.g,source.b));
    float chroma=value-low;
    float phase=chroma<.001?0.:(source.r==value?(source.g-source.b)/chroma:
        source.g==value?2.+(source.b-source.r)/chroma:4.+(source.r-source.g)/chroma)/6.;
    float x=fract(phase+hue*.32+value*.48+midImpulse*.012);
    vec3 a,b,c;
    if(style==1){a=vec3(.01,.95,1.);b=vec3(1.,.005,.42);c=vec3(.38,.005,1.);}
    else if(style==2){a=vec3(.52,1.,.005);b=vec3(.3,.005,1.);c=vec3(.005,.36,1.);}
    else if(style==3){a=vec3(1.,.015,.005);b=vec3(1.,.24,.005);c=vec3(1.,.93,.01);}
    else if(style==4){a=vec3(.005,.76,.24);b=vec3(.8,.005,.08);c=vec3(.015,.08,1.);}
    else if(style==5){a=vec3(1.,0.,.01);b=vec3(0.,1.,.015);c=vec3(.005,.015,1.);}
    else if(style==6){
        vec3 accent=.5+.5*cos(6.283185*(hue+vec3(0.,.33,.67)));
        return mix(vec3(value),accent*value,smoothstep(.24,.55,chroma));
    }
    else {return mix(vec3(dot(source,vec3(.2126,.7152,.0722))),source,.48)*.72+value*.24;}
    vec3 ink=x<.5?mix(a,b,smoothstep(0.,.5,x)):mix(b,c,smoothstep(.5,1.,x));
    // Retain a dark floor and local contrast; neon is colour, not white bloom.
    return ink*min(.98,pow(max(value,0.),.82)*1.22);
}
void main(){
    vec2 uv=viewUV(gl_FragCoord.xy/resolution),px=1./resolution;
    vec3 c=reflectedScene(uv);
    vec3 halo=vec3(0);
    for(int i=0;i<8;i++){
        float a=float(i)*.785398;
        vec3 s=texture(input0,clamp(uv+vec2(cos(a),sin(a))*px*5.,0.,1.)).rgb;
        halo+=max(s-.55,0.);
    }
    c+=halo*(finishGlow*.12)*(1.-c);
    // Recolour the actual scene and its imagery, preserving spatial detail.
    // Integrated band phases cycle continuously; attacks add immediate changes.
    float angle=travelTime*.24+bassImpulse*.23+midImpulse*.41+trebleImpulse*.19
        +bassHit*.85+midHit*.55+trebleHit*.35;
    vec3 axis=normalize(vec3(1.));
    vec3 rotated=c*cos(angle)+cross(axis,c)*sin(angle)+axis*dot(axis,c)*(1.-cos(angle));
    float light=dot(c,vec3(.2126,.7152,.0722));
    vec3 tint=.5+.5*cos(angle+vec3(0.,2.094,4.189));
    rotated=mix(rotated,light*(.45+1.5*tint),.18+.22*bassHit);
    int style=int(floor(paletteStyle*7.+.5));
    if(style==0)c=mix(c,rotated,clamp(.5+.45*intensity,0.,1.));
    else c=mix(c,authoredPalette(c,style),paletteAmount);
    colour=vec4(clamp(c,0.,1.),1.);
}
'''


def program(fragment, vertex=VERTEX):
    shaders = []
    try:
        shaders.append(compileShader(vertex, gl.GL_VERTEX_SHADER))
        shaders.append(compileShader(fragment, gl.GL_FRAGMENT_SHADER))
        return compileProgram(*shaders, validate=False)
    finally:
        for shader in shaders:
            gl.glDeleteShader(shader)


class Target:
    def __init__(self, width, height, buffers=1, hdr=False, format=None, filter='linear'):
        if buffers not in (1,2) or not 1 <= width <= 1920 or not 1 <= height <= 1080:
            raise ValueError('Unsupported original visual target')
        self.width, self.height = width, height
        format = format or ('rgba16f' if hdr else 'rgba8')
        formats = {'rgba8':(gl.GL_RGBA8,4),'rgba16f':(gl.GL_RGBA16F,8),'rgba32f':(gl.GL_RGBA32F,16)}
        if format not in formats or filter not in ('linear','nearest'): raise ValueError('Invalid target storage')
        self.byte_size = width * height * formats[format][1] * buffers
        self.textures, self.fbos, self.front = [], [], 0
        try:
            for _ in range(buffers):
                self.textures.append(int(gl.glGenTextures(1)))
                gl.glBindTexture(gl.GL_TEXTURE_2D, self.textures[-1])
                gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, formats[format][0],
                               width, height, 0, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE if format=='rgba8' else gl.GL_FLOAT, None)
                for name in (gl.GL_TEXTURE_MIN_FILTER, gl.GL_TEXTURE_MAG_FILTER):
                    gl.glTexParameteri(gl.GL_TEXTURE_2D, name, gl.GL_LINEAR if filter=='linear' else gl.GL_NEAREST)
                for name in (gl.GL_TEXTURE_WRAP_S, gl.GL_TEXTURE_WRAP_T):
                    gl.glTexParameteri(gl.GL_TEXTURE_2D, name, gl.GL_CLAMP_TO_EDGE)
                self.fbos.append(int(gl.glGenFramebuffers(1)))
                gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.fbos[-1])
                gl.glFramebufferTexture2D(gl.GL_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0, gl.GL_TEXTURE_2D, self.textures[-1], 0)
                if gl.glCheckFramebufferStatus(gl.GL_FRAMEBUFFER) != gl.GL_FRAMEBUFFER_COMPLETE:
                    raise RuntimeError('Original scene framebuffer is incomplete')
            self.clear()
        except Exception:
            self.close()
            raise

    def clear(self):
        for fbo in self.fbos:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, fbo)
            gl.glClearColor(0,0,0,0)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)
        self.front = 0

    def close(self):
        if self.fbos: gl.glDeleteFramebuffers(len(self.fbos), self.fbos)
        if self.textures: gl.glDeleteTextures(self.textures)
        self.fbos, self.textures = [], []


class Renderer:
    target_class, vertex_shader = Target, VERTEX

    def __init__(self):
        self.window, self.vao = None, 0
        if not glfw.init(): raise RuntimeError('OpenGL window library could not start')
        try:
            glfw.window_hint(glfw.VISIBLE, False)
            glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
            glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
            glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
            self.window = glfw.create_window(64, 64, 'Mixworks original visual context', None, None)
            if not self.window: raise RuntimeError('Original visuals require OpenGL 3.3')
            glfw.make_context_current(self.window)
            glfw.swap_interval(0)
            self.vao = int(gl.glGenVertexArrays(1))
            gl.glBindVertexArray(self.vao)
            self.gpu = gl.glGetString(gl.GL_RENDERER).decode(errors='replace')
        except Exception:
            self.close()
            raise

    def close(self):
        if self.window:
            glfw.make_context_current(self.window)
            if self.vao: gl.glDeleteVertexArrays(1, [self.vao])
            glfw.destroy_window(self.window)
        self.window, self.vao = None, 0
        glfw.terminate()


def pass_descriptors(root):
    """Ordered current DAG and explicit previous edges, local to one Scene.

    Schema 2 state passes run at 60 Hz; previous means the start of that tick.
    Display reads the completed state. No implicit transfer between scene types.
    """
    descriptor = json.loads((root/'scene.json').read_text(encoding='utf-8'))
    passes = descriptor.get('passes')
    schema = descriptor.get('passSchema')
    if type(schema) is not int or schema not in (1,2) or not isinstance(passes,list) or not 1 <= len(passes) <= (4 if schema==1 else 12):
        raise ValueError('Invalid original pass descriptor')
    seen = set()
    for item in passes:
        allowed={'id','shader','scale','format','inputs','history'}
        if schema==2: allowed.update(('size','filter','state','semantic','vertex','instances'))
        if not isinstance(item,dict) or set(item)-allowed:
            raise ValueError('Invalid original pass fields')
        name, shader = item.get('id'), item.get('shader')
        if not isinstance(name,str) or not re.fullmatch('[a-z][a-z0-9]{0,31}',name) or name in seen:
            raise ValueError('Invalid original pass identity')
        if not isinstance(shader,str) or not re.fullmatch(r'[a-z][a-z0-9-]{0,63}\.glsl',shader):
            raise ValueError('Invalid original shader path')
        if (root/shader).resolve().parent != root.resolve():
            raise ValueError('Original shader escapes scene directory')
        if 'vertex' in item or 'instances' in item:
            vertex=item.get('vertex')
            if (not isinstance(vertex,str) or not re.fullmatch(r'[a-z][a-z0-9-]{0,63}\.glsl',vertex)
                    or (root/vertex).resolve().parent != root.resolve()
                    or type(item.get('instances')) is not int or not 1<=item['instances']<=4096
                    or item.get('state') or item.get('history')):
                raise ValueError('Invalid bounded particle display')
        if 'size' in item:
            size=item['size']
            if 'scale' in item or not isinstance(size,list) or len(size)!=2 or any(type(n) is not int or not 1<=n<=512 for n in size):
                raise ValueError('Invalid state target size')
        elif type(item.get('scale')) not in (int,float) or item['scale'] not in (.25,.5,.75,1):
            raise ValueError('Invalid original pass scale')
        if item.get('format') not in (('rgba8','rgba16f') if schema==1 else ('rgba8','rgba16f','rgba32f')) or type(item.get('history',False)) is not bool:
            raise ValueError('Invalid original pass target')
        if type(item.get('state',False)) is not bool or item.get('filter','linear') not in ('linear','nearest'):
            raise ValueError('Invalid state sampling')
        if item.get('state') and (item['format']=='rgba8' or not isinstance(item.get('semantic'),str) or not item['semantic']):
            raise ValueError('Simulation state needs signed storage and a semantic identity')
        inputs=item.get('inputs',[])
        if not isinstance(inputs,list) or len(inputs)>4: raise ValueError('Invalid pass inputs')
        for edge in inputs:
            if schema==1:
                if not isinstance(edge,str) or edge not in seen: raise ValueError('Original pass inputs must precede their consumer')
            elif not isinstance(edge,dict) or set(edge)!={'pass','frame'} or not isinstance(edge['pass'],str) or edge['frame'] not in ('current','previous'):
                raise ValueError('Explicit current/previous input required')
            elif edge['frame']=='current' and edge['pass'] not in seen:
                raise ValueError('Current-frame cycle or forward input')
        seen.add(name)
    by_id={p['id']:p for p in passes}
    display_seen=False
    for item in passes:
        if item.get('state'):
            if display_seen: raise ValueError('State must precede display')
        else: display_seen=True
        for edge in item.get('inputs',[]):
            if isinstance(edge,str): continue
            producer=by_id.get(edge['pass'])
            if producer is None or (edge['frame']=='previous' and not producer.get('history')):
                raise ValueError('Previous input requires a retained target')
            if item.get('state') and not producer.get('state'): raise ValueError('State cannot consume display history')
            if not item.get('state') and producer.get('state') and edge['frame']=='previous':
                raise ValueError('Display consumes completed current simulation state')
    if passes[-1].get('scale') != 1 or passes[-1]['format'] != 'rgba8' or passes[-1].get('state'):
        raise ValueError('Original scene must finish in full-size rgba8')
    return passes


class AudioController:
    """Transport-time envelopes. A seek starts at the supplied block, never old audio.

    History intentionally depends on contiguous playback; seek/revision/generation
    changes reset it. We do not claim to reconstruct past envelopes after a seek.
    """
    def __init__(self):
        self.levels = None
        self.onset = 0.
        self.previous = 0.
        self.motion_phase = 0.
        self.bass_hit = self.bass_phase = self.previous_bass = 0.
        self.band_hits = dict(mid=0.,treble=0.)
        self.band_times = dict(mid=0.,treble=0.)
        self.band_previous = dict(mid=0.,treble=0.)
        self.impulse_times = dict(bass=0.,mid=0.,treble=0.)

    def update(self, levels, dt, reset, momentum=.5):
        if reset or self.levels is None:
            self.levels = dict(levels)
            self.onset = 0.
            self.motion_phase = 0.
            self.bass_hit = self.bass_phase = 0.
            self.band_hits = dict(mid=0.,treble=0.)
            self.band_times = dict(mid=0.,treble=0.)
            self.impulse_times = dict(bass=0.,mid=0.,treble=0.)
        elif dt > 0:
            for name,value in levels.items():
                tau = .025 if value > self.levels[name] else (.28 if name == 'bass' else .18)*(.4+1.2*momentum)
                self.levels[name] += (value-self.levels[name]) * (1-math.exp(-dt/tau))
            novelty = max(0.,levels['energy']-self.previous)/(self.previous+.08)
            self.onset = max(self.onset*math.exp(-dt/.14), min(1.,novelty*1.2))
            self.motion_phase += dt*(self.levels['treble']*.7+self.onset*2.)
            novelty_bass=max(0.,levels['bass']-self.previous_bass)/(self.previous_bass+.06)
            release=.12*(.4+1.2*momentum)
            self.bass_hit=max(self.bass_hit*math.exp(-dt/release),min(1.,novelty_bass*3.5) if levels['bass']>.015 else 0.)
            self.bass_phase+=dt*self.levels['bass']
            for band in self.band_hits:
                novelty=max(0.,levels[band]-self.band_previous[band])/(self.band_previous[band]+.06)
                self.band_hits[band]=max(self.band_hits[band]*math.exp(-dt/release),min(1.,novelty*3.5) if levels[band]>.015 else 0.)
                self.band_times[band]+=dt*self.levels[band]
            for band,hit in dict(bass=self.bass_hit,**self.band_hits).items():
                self.impulse_times[band]+=dt*hit
        if reset or dt > 0:
            self.previous = levels['energy']
            self.previous_bass = levels['bass']
            self.band_previous = {band:levels[band] for band in self.band_hits}
        return dict(self.levels,onset=self.onset,bassHit=self.bass_hit,bassTime=self.bass_phase,
                    midHit=self.band_hits['mid'],trebleHit=self.band_hits['treble'],
                    midTime=self.band_times['mid'],trebleTime=self.band_times['treble'],
                    bassImpulse=self.impulse_times['bass'],midImpulse=self.impulse_times['mid'],trebleImpulse=self.impulse_times['treble'])


class Scene:
    def __init__(self, root, width, height, key='original/prism-bloom'):
        if key not in SCENE_KEYS: raise ValueError('Unknown original scene')
        self.key = key
        self.width, self.height = width, height
        self.passes, self.final, self.frames = [], None, 0
        self.clock, self.identity = None, None
        self.simulation_time, self.accumulator = 0., 0.
        self.audio = AudioController()
        self.handoff = None
        descriptor=json.loads((Path(root)/'scenes'/key.split('/')[1]/'scene.json').read_text())
        self.particle_family=descriptor.get('variant',-1) if descriptor.get('family')=='sprite-particles' else -1
        self.hierarchy_mode=descriptor.get('hierarchyMode',-1)
        self.last_controls={}
        scene_root = Path(root)/'scenes'/key.split('/')[1]
        descriptors = pass_descriptors(scene_root)
        descriptors=[*descriptors,dict(id='sharedlook',shader='shared-look.glsl',scale=1,format='rgba8',inputs=[descriptors[-1]['id']])]
        def dimensions(d):
            return d.get('size',[max(1,int(width*d.get('scale',1))),max(1,int(height*d.get('scale',1)))])
        budget=sum(math.prod(dimensions(d))*{'rgba8':4,'rgba16f':8,'rgba32f':16}[d['format']]
                   *(2 if d.get('history') else 1) for d in descriptors)
        if budget > 128*1024*1024: raise ValueError('Scene target budget exceeded')
        try:
            for descriptor in descriptors:
                entry = dict(descriptor, target=None, program=0)
                self.passes.append(entry)
                entry['target'] = Target(*dimensions(entry), buffers=2 if entry.get('history') else 1,
                                         format=entry['format'], filter=entry.get('filter','linear'))
                source=LOOK_GLSL if entry['id']=='sharedlook' else (scene_root/entry['shader']).read_text(encoding='utf-8')
                # Re-capture material atlases on an explicit image-method edit.
                # Image-fed physical/population fields keep their accumulated state.
                entry['materialSource']=bool(entry.get('state') and entry.get('semantic')=='premultiplied-source-atlas-v1')
                vertex=(scene_root/entry['vertex']).read_text(encoding='utf-8') if 'vertex' in entry else VERTEX
                entry['program'] = program(scene_source(source) if 'uniform sampler2D userImage;' in source else source,vertex)
                names = ('resolution','sceneTime','projectBeat','beatPulse','energy','bass','mid','treble','onset','bassHit','bassTime','midHit','trebleHit','midTime','trebleTime','bassImpulse','midImpulse','trebleImpulse',
                         'simulationTime','simulationReset','stepDelta','audioMotion','intensity','motion','imageMix','userImage','imageSize','hasImage',
                         'input0','input1','input2','input3','historyImage','historyValid',
                         'nextImage','imageBlend','mediaAspect','nextMediaAspect','travelTime','deltaTime',
                         'cutoutImage','nextCutoutImage','hasCutout','cutoutBlend','cutoutAspect','nextCutoutAspect',
                         'handoffMaterial','handoffAppearance','handoffStrength','generationBlend','handoffFamily','handoffMode','handoffScale','handoffHierarchy') + tuple(CONTROL_DEFAULTS)
                entry['locations'] = {name:gl.glGetUniformLocation(entry['program'],name) for name in names}
            self.final = self.passes[-1]['target']
        except Exception:
            self.close()
            raise

    @property
    def byte_size(self):
        return sum(p['target'].byte_size for p in self.passes)+(self.handoff.byte_size if self.handoff else 0)

    def render(self, actor, beat, levels, texture, image_size, generation=0, next_texture=0, next_size=(1,1), image_blend=0.,cutout=(0,(1,1),0,(1,1),0.)):
        dt = 0. if self.clock is None else actor['time']-self.clock
        identity = (actor['revision'],generation)
        reset = self.clock is None or identity != self.identity or dt < 0 or dt > .3
        if reset:
            for entry in self.passes:
                if entry.get('history'): entry['target'].clear()
        if reset: self.simulation_time, self.accumulator = 0., 0.
        controls=actor['controls']
        driven=dict(levels)
        for band in ('bass','mid','treble'):
            driven[band]=min(1.,levels[band]*2.*controls.get(band+'Drive',.5))
        driven['energy']=min(1.,levels['energy']*(sum(controls.get(b+'Drive',.5) for b in ('bass','mid','treble'))/1.5))
        controlled = self.audio.update(driven,dt,reset,controls.get('momentum',.5))
        values = dict(sceneTime=actor['time'],projectBeat=beat,
                      deltaTime=0. if reset else max(0.,min(.1,dt)),
                      travelTime=actor.get('motionTime',actor['time']*(.25+.75*actor['controls'].get('motion',.5))),
                      audioMotion=self.audio.motion_phase,
                      imageBlend=image_blend,
                      cutoutBlend=cutout[4],
                      beatPulse=math.exp(-6*(beat%1))*controlled['bass'], **controlled, **actor['controls'])
        values.setdefault('imageZoom',.35); values.setdefault('imageRotation',.5)
        for name in ('materialMotion','materialMode','viewPattern','finishGlow','paletteStyle','paletteAmount'):
            values.setdefault(name,CONTROL_DEFAULTS[name])
        values['handoffStrength']=self.handoff.strength if self.handoff else 0.
        progress=self.handoff.progress if self.handoff else 1.
        values.update(generationBlend=progress*progress*(3.-2.*progress),
                      handoffHierarchy=float(self.handoff.source_hierarchy_mode if self.handoff else -1),
                      handoffFamily=float(self.handoff.source_family if self.handoff else -1),
                      handoffMode=self.handoff.source_controls.get('mode',0.) if self.handoff else 0.,
                      handoffScale=self.handoff.source_controls.get('scale',.48) if self.handoff else .48)
        if self.handoff and self.particle_family>=0 and self.handoff.source_family>=0:
            for name in ('intensity','detail','colourR','colourG','colourB'):
                old=self.handoff.source_controls.get(name,values[name])
                values[name]=old+(values[name]-old)*values['generationBlend']
        rendered = {e['id']:e['target'].textures[e['target'].front] for e in self.passes}
        def draw(entry, previous, initialise=False):
            target, locations = entry['target'], entry['locations']
            output = 1-target.front if entry.get('history') else 0
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[output])
            gl.glViewport(0,0,target.width,target.height)
            gl.glUseProgram(entry['program'])
            gl.glUniform2f(locations['resolution'],target.width,target.height)
            for name,value in values.items():
                if name in locations: gl.glUniform1f(locations[name],value)
            bindings = [('userImage',texture),('nextImage',next_texture or texture),('cutoutImage',cutout[0]),('nextCutoutImage',cutout[2] or cutout[0])]
            for i,edge in enumerate(entry.get('inputs',[])):
                tex=rendered[edge] if isinstance(edge,str) else (previous if edge['frame']=='previous' else rendered)[edge['pass']]
                bindings.append(('input'+str(i),tex))
            bindings.append(('historyImage',target.textures[target.front] if entry.get('history') else 0))
            if self.handoff:
                bindings.append(('handoffMaterial',self.handoff.material.textures[self.handoff.material.front]))
                bindings.append(('handoffAppearance',self.handoff.appearance.textures[self.handoff.appearance.front]))
            for unit,(name,tex) in enumerate(bindings):
                gl.glActiveTexture(gl.GL_TEXTURE0+unit)
                gl.glBindTexture(gl.GL_TEXTURE_2D,tex)
                gl.glUniform1i(locations[name],unit)
            gl.glUniform1i(locations['hasImage'],int(bool(texture) or bool(self.handoff)))
            gl.glUniform1i(locations['hasCutout'],int(bool(cutout[0])))
            gl.glUniform1i(locations['historyValid'],int(not initialise and bool(entry.get('history'))))
            gl.glUniform2f(locations['imageSize'],*image_size)
            def aspect(size):
                ratio=(self.width/self.height)/(size[0]/size[1])
                return (min(1.,ratio),min(1.,1./ratio))
            gl.glUniform2f(locations['mediaAspect'],*aspect(image_size))
            gl.glUniform2f(locations['nextMediaAspect'],*aspect(next_size if next_texture else image_size))
            def cutout_aspect(size):
                ratio=size[0]/size[1];return max(1.,1/ratio),max(1.,ratio)
            gl.glUniform2f(locations['cutoutAspect'],*cutout_aspect(cutout[1]))
            gl.glUniform2f(locations['nextCutoutAspect'],*cutout_aspect(cutout[3] if cutout[2] else cutout[1]))
            if 'instances' in entry:
                # The fragment shader emits premultiplied colour. Clear each draw:
                # particle persistence is in position/velocity, never framebuffer fog.
                gl.glClearColor(0,0,0,0)
                gl.glClear(gl.GL_COLOR_BUFFER_BIT)
                gl.glEnable(gl.GL_BLEND)
                gl.glBlendFunc(gl.GL_ONE,gl.GL_ONE_MINUS_SRC_ALPHA)
                try: gl.glDrawArraysInstanced(gl.GL_TRIANGLES,0,6,entry['instances'])
                finally: gl.glDisable(gl.GL_BLEND)
            else: gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
            target.front = output
            rendered[entry['id']] = target.textures[output]
        state=[e for e in self.passes if e.get('state')]
        material_changed=bool(self.last_controls) and controls.get('materialMode',0.)!=self.last_controls.get('materialMode',0.)
        if material_changed and not reset:
            # An explicit material-method edit must update paused captured imagery
            # without advancing/restarting particle positions or pressure fields.
            values.update(stepDelta=0.,simulationTime=self.simulation_time,simulationReset=1.)
            previous={e['id']:e['target'].textures[e['target'].front] for e in self.passes}
            for entry in state:
                if entry['materialSource']:draw(entry,previous,True)
        if state:
            self.accumulator += 0. if reset else max(0.,dt)
            steps=1 if reset else min(18,int((self.accumulator+1e-9)*60))
            for _ in range(steps):
                values.update(stepDelta=0. if reset else 1./60.,simulationTime=self.simulation_time,simulationReset=float(reset))
                previous={e['id']:e['target'].textures[e['target'].front] for e in self.passes}
                for entry in state: draw(entry,previous,reset)
                if reset and self.handoff:
                    self.handoff.seed(self)
                    rendered.update({e['id']:e['target'].textures[e['target'].front] for e in state})
                if not reset:
                    self.accumulator=max(0.,self.accumulator-1./60.)
                    self.simulation_time+=1./60.
        previous={e['id']:e['target'].textures[e['target'].front] for e in self.passes}
        for entry in self.passes:
            if not entry.get('state'): draw(entry,previous,reset)
        self.clock, self.identity = actor['time'], identity
        self.last_controls=dict(actor['controls'])
        self.frames += 1

    def close(self):
        if self.handoff:self.handoff.close();self.handoff=None
        for entry in self.passes:
            if entry['target']: entry['target'].close()
            if entry['program']: gl.glDeleteProgram(entry['program'])
        self.passes, self.final = [], None

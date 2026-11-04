"""Owned scene protocol implementation, with optional LGPL libprojectM support.

Music comes from Alloy's bounded mixed-PCM feed, never a capture device.
No scene scripts, extracted host helpers, Synesthesia paths or prototype imports.
One attributed CC0 shader adaptation accompanies the owned scene collection.
"""
import base64
import math
from pathlib import Path
import json
import time
import warnings
import numpy as np
from PIL import Image
from OpenGL import GL as gl
from original_visual_gl import Renderer, Scene, SCENE_KEYS
from visual_catalogue import catalogue, CONTROL_DEFAULTS
from visual_media import image_role, content_identity
from visual_compositor import VisualCompositor, TRANSITIONS
from visual_handoff import GenerativeHandoff, transition_phase

DEFAULTS = dict(intensity=.65, motion=.45, imageMix=.35)
IMAGE_SUFFIXES = {'.png','.jpg','.jpeg','.bmp','.webp','.tif','.tiff'}


def number(value, low, high, label):
    if isinstance(value, bool) or not isinstance(value, (int,float)) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError('Invalid original visual '+label)
    return float(value)


def actor(raw, entries=None):
    if not isinstance(raw, dict) or raw.get('scene') not in (entries if entries is not None else SCENE_KEYS):
        raise ValueError('Unknown original scene')
    if any(not isinstance(raw.get(k),str) or not 1 <= len(raw[k]) <= 128 for k in ('clip','revision')):
        raise ValueError('Invalid original scene identity')
    controls = raw.get('controls', {})
    schema=entries[raw['scene']]['controls'] if entries is not None else {k:{'default':v} for k,v in CONTROL_DEFAULTS.items()}
    if not isinstance(controls,dict) or set(controls)-set(schema):
        raise ValueError('Unknown original scene control')
    values = {k:v['default'] for k,v in schema.items()}
    for key,value in controls.items(): values[key] = number(value, 0, 1, key)
    return dict(clip=raw['clip'],scene=raw['scene'],revision=raw['revision'],
                time=number(raw.get('time'),0,1e9,'scene clock'),
                motionTime=number(raw.get('motionTime',raw.get('time',0)*(.25+.75*values.get('motion',.5))),0,1e9,'motion clock'),controls=values)


def audio_levels(pcm, rate):
    if not len(pcm): return dict(energy=0.,bass=0.,mid=0.,treble=0.), 0.
    rms = float(np.sqrt(np.mean(np.square(pcm.astype(np.float64)))))
    samples = np.zeros(2048, dtype=np.float64)
    samples[-len(pcm):] = pcm
    spectrum = np.abs(np.fft.rfft(samples*np.hanning(2048))) / 512.
    frequencies = np.fft.rfftfreq(2048,1/rate)
    def band(low, high):
        value=float(np.sqrt(np.sum(np.square(spectrum[(frequencies>=low)&(frequencies<high)]))))*3.
        return value/(1.+value)
    # Soft knees preserve changes in mastered/loud music instead of pinning at 1.
    return dict(energy=rms*4/(1+rms*4),bass=band(30,220),mid=band(220,3000),treble=band(3000,16000)), rms


class OriginalScenePlayer:
    def __init__(self, renderer_root, install, width=1280, height=720, owner_pid=0, *, milkdrop_library=None):
        if type(width) is not int or type(height) is not int or not 1 <= width <= 1920 or not 1 <= height <= 1080:
            raise ValueError('Invalid original visual dimensions')
        self.root, self.width, self.height = Path(install), width, height
        if not (self.root/'original-scenes.json').is_file():
            raise ValueError('Original scene collection is missing')
        self.entries=catalogue(self.root)
        from milkdrop_catalogue import catalogue as imported_catalogue
        self.entries.update(imported_catalogue(milkdrop_library))
        self.milkdrop_library = None
        self.renderer_root = Path(renderer_root)
        self.pool_textures={}; self.cutout_textures={}; self.pool_errors={}; self.pool=[]; self.cutouts=[]
        config_path=self.root/'media-library.json'
        config=json.loads(config_path.read_text(encoding='utf-8')) if config_path.is_file() else {}
        folders=config.get('backgroundFolders',['Backgrounds'])
        sprite_folders=config.get('spriteFolders',['Sprites'])
        if not isinstance(folders,list) or not isinstance(sprite_folders,list) or len(folders)+len(sprite_folders)>8 or any(not isinstance(p,str) for p in folders+sprite_folders):
            raise ValueError('Invalid sprite folders')
        # Explicit folder configuration; originals are read only and never copied/deleted here.
        found=[]
        for folder,force_background in [(p,True) for p in folders]+[(p,False) for p in sprite_folders]:
            path=Path(folder)
            if not path.is_absolute(): path=self.root/path
            if path.is_dir():found.extend((str(p.resolve()),force_background) for p in sorted(path.iterdir(),key=lambda p:p.name.casefold()) if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES)
        identities=set();self.duplicate_count=0
        for path,force_background in found[:1024]:
            try:
                if len(identities)>=512:break
                identity=content_identity(path)
                if identity in identities:self.duplicate_count+=1;continue
                role=image_role(path)
                (self.cutouts if role=='cutout' and not force_background else self.pool).append(path)
                identities.add(identity)
            except Exception as error:self.pool_errors[path]=str(error)
        self.phrase_beats=number(config.get('phraseBeats',32),8,256,'media phrase')
        self.blend_beats=number(config.get('blendBeats',4),.25,8,'media blend')
        self.instances, self.signature, self.encoded, self.sequence = {}, None, b'', 0
        self.transport_identity=None
        self.texture, self.image_size = 0, (1,1)
        self.image, self.requested_image, self.media_error = '', '', None
        self.explicit_role='background'
        self.renderer, self.compositor, self.presenter = None, None, None
        try:
            self.renderer = Renderer()
            self.compositor = VisualCompositor(self.renderer,width,height)
            if owner_pid:
                from visual_presenter import Presenter
                self.presenter = Presenter(self.renderer,owner_pid)
        except Exception:
            self.close()
            raise

    def set_image(self, path):
        replacement, size = 0, (1,1)
        try:
            if path:
                if Path(path).suffix.lower() not in IMAGE_SUFFIXES:
                    raise ValueError('Choose a still image for original visuals')
                with warnings.catch_warnings():
                    warnings.simplefilter('error',Image.DecompressionBombWarning)
                    with Image.open(path) as source:
                        if source.width*source.height > 16_777_216:
                            raise ValueError('Scene image exceeds 16 megapixels')
                        source.thumbnail((2048,2048))
                        image=source.convert('RGBA').transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                size=image.size
                replacement=int(gl.glGenTextures(1))
                gl.glBindTexture(gl.GL_TEXTURE_2D,replacement)
                gl.glTexImage2D(gl.GL_TEXTURE_2D,0,gl.GL_RGBA8,*size,0,gl.GL_RGBA,gl.GL_UNSIGNED_BYTE,image.tobytes())
                for name in (gl.GL_TEXTURE_MIN_FILTER,gl.GL_TEXTURE_MAG_FILTER): gl.glTexParameteri(gl.GL_TEXTURE_2D,name,gl.GL_LINEAR)
                for name in (gl.GL_TEXTURE_WRAP_S,gl.GL_TEXTURE_WRAP_T): gl.glTexParameteri(gl.GL_TEXTURE_2D,name,gl.GL_CLAMP_TO_EDGE)
        except Exception as error:
            if replacement: gl.glDeleteTextures([replacement])
            self.media_error=str(error)
        else:
            if self.texture: gl.glDeleteTextures([self.texture])
            self.texture,self.image_size,self.image = replacement,size,path
            self.media_error=None
            self.signature=None
        self.requested_image=path

    def render(self, request):
        began=time.perf_counter()
        if request.get('protocol') != 6: raise ValueError('Original visual protocol mismatch')
        raw=request.get('scenes')
        if not isinstance(raw,list) or len(raw)>2: raise ValueError('At most two original scenes are supported')
        actors=[actor(a,self.entries) for a in raw]
        if len({a['clip'] for a in actors}) != len(actors): raise ValueError('Duplicate original scene identity')
        if request.get('preload') is not None: actor(request['preload'],self.entries)
        position=number(request.get('position'),0,1e9,'position')
        beat=number(request.get('beat'),-1e9,1e9,'beat')
        number(request.get('bpm'),1,1000,'tempo')
        rate=number(request.get('rate'),8000,384000,'sample rate')
        mix=number(request.get('mix'),0,1,'blend')
        pivot=number(request.get('pivot',.5),0,1,'transition pivot')
        transition=request.get('transition','dissolve')
        if transition not in TRANSITIONS: raise ValueError('Unknown original visual transition')
        # Long track overlaps must keep both complete live scenes visible.
        # Unknown duration is conservative; native requests supply exact overlap.
        duration=number(request.get('transitionSeconds',1e9),0,1e9,'transition duration')
        if duration>4. and transition not in ('dissolve','zoom','cut'): transition='dissolve'
        generative=transition not in ('dissolve','zoom','cut')
        if generative and any(a['scene'].startswith('milkdrop/') for a in actors):
            # Imported engines have no Alloy simulation state to inherit.
            transition, generative = 'dissolve', False
        generation=request.get('generation')
        if type(generation) is not int or generation<0: raise ValueError('Invalid original visual generation')
        capture=request.get('capture',self.presenter is None)
        if type(capture) is not bool: raise ValueError('Invalid original visual capture')
        image=request.get('image','')
        if not isinstance(image,str): raise ValueError('Invalid original image path')
        encoded=request.get('pcm','')
        if not isinstance(encoded,str) or len(encoded)>10924: raise ValueError('Oversized original visual PCM')
        raw=base64.b64decode(encoded,validate=True)
        if len(raw)>8192 or len(raw)%4: raise ValueError('Invalid original visual PCM')
        pcm=np.frombuffer(raw,dtype='<f4')
        if not np.isfinite(pcm).all() or np.any(np.abs(pcm)>64): raise ValueError('Invalid original visual sample')
        levels,rms=audio_levels(pcm,rate)
        transport_reset=(self.transport_identity is not None and
            (generation!=self.transport_identity[0] or position<self.transport_identity[1]
             or position-self.transport_identity[1]>.3))
        if transport_reset:
            # A discontinuity must not retain a handoff from a previous route.
            for instance in self.instances.values():instance.close()
            self.instances.clear();self.signature=None
        self.transport_identity=(generation,position)
        if self.presenter: self.presenter.configure(request.get('views',[]))
        if image != self.requested_image:
            self.set_image(image)
            if self.image==image:self.explicit_role=image_role(image) if image else 'background'
        pool_media={}; cutout_media={}; required=set()
        if (not image or self.explicit_role=='cutout') and self.pool:
            phrase=max(0.,beat)/self.phrase_beats
            blend=max(0.,min(1.,(phrase%1-(1-self.blend_beats/self.phrase_beats))/(self.blend_beats/self.phrase_beats)))
            blend=blend*blend*(3-2*blend)
            for a in actors:
                if a['scene'].startswith('milkdrop/'): continue
                offset=int(a['controls'].get('imageIndex',0)*(len(self.pool)-1))
                i=(int(phrase)+offset)%len(self.pool)
                paths=(self.pool[i],self.pool[(i+1)%len(self.pool)])
                required.update(paths);pool_media[a['clip']]=(paths,blend)
            # Up to two images per actor, evicted BEFORE replacement allocations.
            for path in list(self.pool_textures):
                if path not in required: gl.glDeleteTextures([self.pool_textures.pop(path)[0]])
            for path in sorted(required):
                if path in self.pool_textures or path in self.pool_errors: continue
                old=(self.texture,self.image_size,self.image,self.requested_image,self.media_error)
                self.texture=0
                self.set_image(path)
                if self.texture: self.pool_textures[path]=(self.texture,self.image_size)
                else: self.pool_errors[path]=self.media_error
                self.texture,self.image_size,self.image,self.requested_image,self.media_error=old
        else:
            for tex,size in self.pool_textures.values(): gl.glDeleteTextures([tex])
            self.pool_textures.clear()
        required_cutouts=set()
        if self.cutouts and not (image and self.explicit_role=='cutout'):
            phrase=max(0.,beat)/self.phrase_beats
            blend=max(0.,min(1.,(phrase%1-(1-self.blend_beats/self.phrase_beats))/(self.blend_beats/self.phrase_beats)))
            blend=blend*blend*(3-2*blend)
            for a in actors:
                if not self.entries[a['scene']].get('cutoutReady',False):continue
                offset=int(a['controls'].get('imageIndex',0)*(len(self.cutouts)-1))
                i=(int(phrase)+offset)%len(self.cutouts)
                paths=(self.cutouts[i],self.cutouts[(i+1)%len(self.cutouts)])
                required_cutouts.update(paths);cutout_media[a['clip']]=(paths,blend)
        for path in list(self.cutout_textures):
            if path not in required_cutouts:gl.glDeleteTextures([self.cutout_textures.pop(path)[0]])
        for path in sorted(required_cutouts):
            if path in self.cutout_textures or path in self.pool_errors:continue
            old=(self.texture,self.image_size,self.image,self.requested_image,self.media_error);self.texture=0
            self.set_image(path)
            if self.texture:self.cutout_textures[path]=(self.texture,self.image_size)
            else:self.pool_errors[path]=self.media_error
            self.texture,self.image_size,self.image,self.requested_image,self.media_error=old
        keep={a['clip'] for a in actors}
        for key in list(self.instances):
            if key not in keep: self.instances.pop(key).close()
        signature=(tuple((a['clip'],a['scene'],a['revision'],a['time'],a['motionTime'],tuple(a['controls'].items())) for a in actors),
                   beat,mix,pivot,transition,generation,self.texture,tuple(pool_media.items()),tuple(cutout_media.items()),capture)
        if signature != self.signature:
            gl.glBindVertexArray(self.renderer.vao)
            for index,a in enumerate(actors):
                if a['clip'] in self.instances and self.instances[a['clip']].key != a['scene']:
                    self.instances.pop(a['clip']).close()
                if a['clip'] not in self.instances:
                    if a['scene'].startswith('milkdrop/'):
                        from milkdrop_scene import Library, MilkDropScene
                        if self.milkdrop_library is None: self.milkdrop_library = Library(self.renderer_root)
                        self.instances[a['clip']]=MilkDropScene(self.milkdrop_library,self.entries[a['scene']],
                            self.width,self.height,a['scene'],self.renderer)
                    else:
                        self.instances[a['clip']]=Scene(self.root,self.width,self.height,a['scene'])
                scene=self.instances[a['clip']]
                if a['scene'].startswith('milkdrop/'):
                    scene.render_pcm(a,pcm)
                    gl.glBindVertexArray(self.renderer.vao)
                    continue
                if index==1 and not generative and scene.handoff:
                    scene.handoff.close();scene.handoff=None;scene.clock=None
                if index==1 and generative:
                    pair=(generation,tuple((b['clip'],b['scene'],b['revision']) for b in actors))
                    if scene.handoff is None or scene.handoff.pair!=pair:
                        if scene.handoff:scene.handoff.close()
                        scene.handoff=GenerativeHandoff(self.instances[actors[0]['clip']],pair,a['time'])
                        scene.clock=None
                    # Outgoing has rendered this very frame. Keep its visible
                    # appearance live throughout overlap; typed seed snapshots
                    # remain owned by the incoming simulation.
                    scene.handoff.blit(self.instances[actors[0]['clip']].final,scene.handoff.appearance)
                elif scene.handoff and scene.identity is not None and scene.identity!=(a['revision'],generation):
                    scene.handoff.close();scene.handoff=None
                if scene.handoff:
                    progress=transition_phase(mix,pivot) if index==1 and len(actors)>1 else 1.
                    scene.handoff.advance(a['time'],progress,levels)
                    if scene.handoff.tail>12.:scene.handoff.close();scene.handoff=None
                tex,size,next_tex,next_size,blend=self.texture,self.image_size,0,(1,1),0.
                if image and self.explicit_role=='cutout':tex,size=0,(1,1)
                if a['clip'] in pool_media:
                    paths,blend=pool_media[a['clip']]
                    tex,size=self.pool_textures.get(paths[0],(0,(1,1)))
                    next_tex,next_size=self.pool_textures.get(paths[1],(tex,size))
                    if not tex: tex,size=next_tex,next_size
                cutout=(0,(1,1),0,(1,1),0.)
                if image and self.explicit_role=='cutout' and self.entries[a['scene']].get('cutoutReady',False):
                    cutout=(self.texture,self.image_size,self.texture,self.image_size,0.)
                if a['clip'] in cutout_media:
                    paths,amount=cutout_media[a['clip']]
                    ct,cs=self.cutout_textures.get(paths[0],(0,(1,1)))
                    nt,ns=self.cutout_textures.get(paths[1],(ct,cs))
                    if not ct:ct,cs=nt,ns
                    cutout=(ct,cs,nt,ns,amount)
                self.instances[a['clip']].render(a,beat,levels,tex,size,generation,next_tex,next_size,blend,cutout)
            if len(actors)==2 and generative and 0.<mix<1.:
                incoming=self.instances[actors[1]['clip']]
                progress=transition_phase(mix,pivot)
                progress=progress*progress*(3.-2.*progress)
                if incoming.handoff.seeded:
                    # These scenes morph their own geometry/material shading from
                    # inherited state and optional full-resolution appearance.
                    self.encoded=self.compositor.render(incoming,incoming,0.,'dissolve',.5,readback=capture)
                else:
                    self.encoded=incoming.handoff.render(incoming,self.compositor.target,progress,capture)
            elif actors:
                self.encoded=self.compositor.render(self.instances[actors[0]['clip']],self.instances[actors[-1]['clip']],
                    mix if len(actors)>1 else 0.,transition,pivot,readback=capture)
            else: self.encoded=b''
            self.signature=signature
            self.sequence+=1
        before_present=time.perf_counter()
        presentation=self.presenter.present(self.compositor.target.textures[0] if actors else 0,
            self.width,self.height,generation,bool(request.get('playing'))) if self.presenter else {}
        presentation['pixelTransferBytes']=len(self.encoded)
        facts=[dict(**a,frames=self.instances[a['clip']].frames,rms=rms) for a in actors]
        for fact in facts:
            if fact['scene'].startswith('milkdrop/'):
                fact.update(renderer='projectM-4.1.8',audioSource='mixed-pcm-mono',clock='live-wall-clock',
                            seekBehaviour='restart-feedback',upstreamTargetBytesIncluded=False)
        handoffs=[dict(clip=a['clip'],routes=self.instances[a['clip']].handoff.seeded,
                      materialStrength=self.instances[a['clip']].handoff.strength)
                  for a in actors if self.instances[a['clip']].handoff]
        return dict(ok=True,empty=not actors,scenes=facts,position=position,generation=generation,mix=mix,
                    transition=transition,generativeHandoffs=handoffs,image=self.image,mediaError=self.media_error,sequence=self.sequence,
                    spriteCount=len(self.pool)+len(self.cutouts),backgroundCount=len(self.pool),cutoutCount=len(self.cutouts),
                    duplicateImagesIgnored=self.duplicate_count,
                    spriteSelection=pool_media,cutoutSelection=cutout_media,spriteErrors=dict(self.pool_errors),videoEnabled=False,
                    mediaTextureBytes=sum(s[0]*s[1]*4 for t,s in list(self.pool_textures.values())+list(self.cutout_textures.values())),
                    pixelFormat='bgra8-bottom-up',width=self.width,height=self.height,gpu=self.renderer.gpu,
                    targetBytes=sum(s.byte_size for s in self.instances.values())+self.compositor.target.byte_size,original=True,
                    renderMs=(before_present-began)*1000,presentMs=(time.perf_counter()-before_present)*1000,
                    **presentation),self.encoded

    def reset(self):
        if self.presenter: self.presenter.present(0,self.width,self.height,-1,False)
        for instance in self.instances.values(): instance.close()
        self.instances.clear()
        self.signature,self.encoded=None,b''
        self.transport_identity=None

    def close(self):
        if self.presenter: self.presenter.close(); self.presenter=None
        for instance in self.instances.values(): instance.close()
        self.instances.clear()
        if self.texture: gl.glDeleteTextures([self.texture]); self.texture=0
        for tex,size in self.pool_textures.values(): gl.glDeleteTextures([tex])
        self.pool_textures.clear()
        for tex,size in self.cutout_textures.values():gl.glDeleteTextures([tex])
        self.cutout_textures.clear()
        if self.compositor: self.compositor.close(); self.compositor=None
        if self.renderer: self.renderer.close(); self.renderer=None

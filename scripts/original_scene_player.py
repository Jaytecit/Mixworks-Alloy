"""Owned scene protocol implementation, with optional LGPL libprojectM support.

Music comes from Alloy's bounded mixed-PCM feed, never a capture device.
No scene scripts, extracted host helpers, Synesthesia paths or prototype imports.
One attributed CC0 shader adaptation accompanies the owned scene collection.
"""
import base64
from copy import deepcopy
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
from visual_media import ALPHA_NOISE_MAX, image_role, content_identity, sprite_pixels
from visual_compositor import VisualCompositor, TRANSITIONS
from visual_handoff import GenerativeHandoff, transition_phase, TAIL_SECONDS

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


def transport_reset_required(previous, generation, position):
    # Engine.seek/stop/open publish an explicit generation. A slow forward
    # frame is not a seek: reloading here can trap costly presets in startup.
    return previous is not None and (generation!=previous[0] or position<previous[1])


def kick_impact(position, events):
    """Evaluate recent confirmed audio events at the current transport time."""
    if not isinstance(events,list) or len(events)>3:
        raise ValueError('Invalid visual kick events')
    impact=0.
    for event in events:
        if not isinstance(event,list) or len(event)!=2:
            raise ValueError('Invalid visual kick event')
        stamp=number(event[0],0,1e9,'kick time')
        strength=number(event[1],0,1,'kick strength')
        # The detector confirms after five 10 ms windows. Leave most of the
        # eased rise visible after confirmation, without restarting it on a late frame.
        age=position-stamp-.042
        phase=min(1.,age/.045,(.270-age)/.225)
        if phase>0.:
            impact=max(impact,strength*phase*phase*(3.-2.*phase))
    return impact


class OriginalScenePlayer:
    def __init__(self, renderer_root, install, width=1280, height=720, owner_pid=0, *, milkdrop_library=None, sprite_folder=None):
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
        self.pool_textures={}; self.cutout_textures={}
        self.pool_errors={}; self.pool=[]; self.cutouts=[]
        config_path=self.root/'media-library.json'
        config=json.loads(config_path.read_text(encoding='utf-8')) if config_path.is_file() else {}
        folders=config.get('backgroundFolders',['Backgrounds'])
        sprite_folders=config.get('spriteFolders',['Sprites'])
        if not isinstance(folders,list) or not isinstance(sprite_folders,list) or len(folders)+len(sprite_folders)>8 or any(not isinstance(p,str) for p in folders+sprite_folders):
            raise ValueError('Invalid sprite folders')
        custom=Path(sprite_folder) if sprite_folder else None
        if custom and custom.is_absolute() and custom.is_dir():
            sprite_folders=[str(custom)]
        # Explicit folder configuration; originals are read only and never copied/deleted here.
        found=[]
        for folder,force_background in [(p,True) for p in folders]+[(p,False) for p in sprite_folders]:
            path=Path(folder)
            if not path.is_absolute(): path=self.root/path
            try:
                if path.is_dir():
                    found.extend((str(p.resolve()),force_background,path==custom) for p in sorted(path.iterdir(),key=lambda p:p.name.casefold())
                                 if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES)
            except OSError as error:
                self.pool_errors[str(path)]=str(error)
        identities=set();self.duplicate_count=0
        for path,force_background,cutouts_only in found[:1024]:
            try:
                if len(identities)>=512:break
                identity=content_identity(path)
                if identity in identities:self.duplicate_count+=1;continue
                role=image_role(path)
                if cutouts_only and role!='cutout':continue
                (self.cutouts if role=='cutout' and not force_background else self.pool).append(path)
                identities.add(identity)
            except Exception as error:self.pool_errors[path]=str(error)
        self.phrase_beats=number(config.get('phraseBeats',32),8,256,'media phrase')
        self.sprite_phrase_beats=number(config.get('spritePhraseBeats',256),8,1024,'sprite phrase')
        self.blend_beats=number(config.get('blendBeats',4),.25,8,'media blend')
        self.instances, self.signature, self.encoded, self.sequence = {}, None, b'', 0
        self.transport_identity=None
        self.last_kick_impact=0.
        self.prepared=None
        self.prepare_attempt=None
        self.morph_identity=None
        self.last_actors=[]
        self.direct_morph=None
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

    def set_image(self, path, *, premultiply=False):
        replacement, size = 0, (1,1)
        try:
            if path:
                if Path(path).suffix.lower() not in IMAGE_SUFFIXES:
                    raise ValueError('Choose a still image for original visuals')
                if premultiply is None:premultiply=image_role(path)=='cutout'
                with warnings.catch_warnings():
                    warnings.simplefilter('error',Image.DecompressionBombWarning)
                    with Image.open(path) as source:
                        if source.width*source.height > 16_777_216:
                            raise ValueError('Scene image exceeds 16 megapixels')
                        if premultiply and max(source.size)>2048:
                            source=source.convert('RGBA')
                            source.putalpha(source.getchannel('A').point(
                                lambda alpha: alpha if alpha>ALPHA_NOISE_MAX else 0))
                        source.thumbnail((2048,2048))
                        image=source.convert('RGBA').transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                size=image.size
                pixels=np.array(image)
                if premultiply:
                    pixels=sprite_pixels(pixels)
                replacement=int(gl.glGenTextures(1))
                gl.glBindTexture(gl.GL_TEXTURE_2D,replacement)
                gl.glTexImage2D(gl.GL_TEXTURE_2D,0,gl.GL_RGBA8,*size,0,gl.GL_RGBA,gl.GL_UNSIGNED_BYTE,pixels.tobytes())
                gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
                gl.glTexParameteri(gl.GL_TEXTURE_2D,gl.GL_TEXTURE_MIN_FILTER,gl.GL_LINEAR_MIPMAP_LINEAR)
                gl.glTexParameteri(gl.GL_TEXTURE_2D,gl.GL_TEXTURE_MAG_FILTER,gl.GL_LINEAR)
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

    def create_scene(self,key):
        if key.startswith('milkdrop/'):
            from milkdrop_scene import Library, MilkDropScene
            if self.milkdrop_library is None:
                self.milkdrop_library=Library(self.renderer_root)
            return MilkDropScene(self.milkdrop_library,self.entries[key],self.width,self.height,key,self.renderer)
        return Scene(self.root,self.width,self.height,key,field_programs=self.renderer.field_programs)

    def prepare(self,upcoming):
        identity=(upcoming['clip'],upcoming['scene']) if upcoming else None
        if self.prepared and self.prepared[0]!=identity:
            self.prepared[1].close()
            self.prepared=None
        if identity is None:
            self.prepare_attempt=None
            return
        if identity==self.prepare_attempt or identity[0] in self.instances:
            return
        self.prepare_attempt=identity
        try:
            self.prepared=(identity,self.create_scene(identity[1]))
        except Exception:
            # A failed speculative load belongs to the upcoming scene. Report it
            # through the normal renderer only if that scene is actually requested.
            return

    def scene_for(self,actor):
        key=actor['clip']
        scene=self.instances.get(key)
        if scene and scene.key!=actor['scene']:
            scene.close()
            self.instances.pop(key)
            scene=None
        if scene is None:
            if self.prepared and self.prepared[0]==(key,actor['scene']):
                _,scene=self.prepared
                self.prepared=None
            else:
                scene=self.create_scene(actor['scene'])
            self.instances[key]=scene
        return scene

    def cutout_for(self,actor,selection):
        if self.image and self.explicit_role=='cutout' and self.supports_cutout(actor):
            return self.texture,self.image_size,self.texture,self.image_size,0.
        if actor['clip'] in selection:
            paths,amount=selection[actor['clip']]
            ct,cs=self.cutout_textures.get(paths[0],(0,(1,1)))
            nt,ns=self.cutout_textures.get(paths[1],(ct,cs))
            if not ct:ct,cs=nt,ns
            return ct,cs,nt,ns,amount
        return 0,(1,1),0,(1,1),0.

    def supports_cutout(self,actor):
        if actor['scene'].startswith('milkdrop/'): return True
        entry=self.entries[actor['scene']]
        return bool(entry.get('cutoutReady') or entry.get('spriteGeometry'))

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
        number(request.get('transitionSeconds',1e9),0,1e9,'transition duration')
        # Most pairs use a material transfer. Shared geometry uses a field morph.
        transition='feedback'
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
        previous_position=self.transport_identity[1] if self.transport_identity else position
        transport_reset=transport_reset_required(self.transport_identity,generation,position)
        impact=kick_impact(position,request.get('kicks',[]))
        if transport_reset:
            # A discontinuity must not retain a handoff from a previous route.
            self.clear_scenes()
            impact=0.
        elif not request.get('playing') and self.transport_identity==(generation,position):
            impact=self.last_kick_impact
        self.last_kick_impact=impact
        self.transport_identity=(generation,position)
        retained_frame=bool(self.instances)
        if self.presenter: self.presenter.configure(request.get('views',[]))
        if image != self.requested_image:
            self.set_image(image,premultiply=None)
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
            phrase=max(0.,beat)/self.sprite_phrase_beats
            blend=max(0.,min(1.,(phrase%1-(1-self.blend_beats/self.sprite_phrase_beats))/(self.blend_beats/self.sprite_phrase_beats)))
            blend=blend*blend*(3-2*blend)
            for a in actors:
                if not self.supports_cutout(a):continue
                i=int(phrase)%len(self.cutouts)
                paths=(self.cutouts[i],self.cutouts[(i+1)%len(self.cutouts)])
                required_cutouts.update(paths);cutout_media[a['clip']]=(paths,blend)
        for path in list(self.cutout_textures):
            if path not in required_cutouts:gl.glDeleteTextures([self.cutout_textures.pop(path)[0]])
        for path in sorted(required_cutouts):
            if path in self.cutout_textures or path in self.pool_errors:continue
            old=(self.texture,self.image_size,self.image,self.requested_image,self.media_error);self.texture=0
            self.set_image(path,premultiply=True)
            if self.texture:self.cutout_textures[path]=(self.texture,self.image_size)
            else:self.pool_errors[path]=self.media_error
            self.texture,self.image_size,self.image,self.requested_image,self.media_error=old
        # A cutout is shared across compatible actors. Each scene samples it in
        # its own material/geometry coordinates.
        morph_source,morph_progress=None,1.
        destination=(actors[-1]['clip'],actors[-1]['scene'],actors[-1]['revision']) if actors else None
        active_destination=self.instances.get(destination[0]) if destination else None
        changed_destination=destination and (not active_destination or active_destination.key!=destination[1])
        if len(actors)==2:
            self.direct_morph=None
            morph_source,morph_progress=actors[0],transition_phase(mix,pivot)
        elif self.direct_morph and self.direct_morph[0]==destination:
            _,morph_source,start=self.direct_morph
            elapsed=max(0.,actors[0]['time']-start)
            morph_source=dict(morph_source,time=morph_source['time']+elapsed,
                              motionTime=morph_source['motionTime']+elapsed*(.25+.75*morph_source['controls'].get('motion',.5)))
            morph_progress=min(1.,elapsed/2.)
        elif len(actors)==1 and retained_frame and changed_destination and self.last_actors:
            previous=self.last_actors[-1]
            elapsed=max(0.,position-previous_position)
            morph_source=dict(previous,time=previous['time']+elapsed,
                              motionTime=previous['motionTime']+elapsed*(.25+.75*previous['controls'].get('motion',.5)))
            morph_progress=0.
        source_entry=self.entries[morph_source['scene']] if morph_source else {}
        composition=source_entry.get('composition',-1)
        morphing=composition>=0 and source_entry['morphFamily']==self.entries[actors[-1]['scene']].get('morphFamily')
        if morphing and len(actors)==1 and changed_destination:
            self.direct_morph=(destination,morph_source,actors[0]['time'])
        if not morphing or morph_progress>=1.:
            self.direct_morph=None
        if not morphing:
            self.morph_identity=None
        morph_clock=self.instances.get(morph_source['clip']) if morphing else None
        direct_handoff=None
        if len(actors)==1 and retained_frame and changed_destination and not morphing:
            outgoing=next(reversed(self.instances.values()))
            pair=(generation,((str(outgoing.key),outgoing.key,'snapshot'),
                              (actors[0]['clip'],actors[0]['scene'],actors[0]['revision'])))
            direct_handoff=GenerativeHandoff(outgoing,pair,actors[0]['time'],appearance=self.compositor.target,direct=True)
        keep={a['clip'] for a in actors}
        for key in list(self.instances):
            if key not in keep: self.instances.pop(key).close()
        signature=(tuple((a['clip'],a['scene'],a['revision'],a['time'],a['motionTime'],tuple(a['controls'].items())) for a in actors),
                   beat,mix,pivot,transition,generation,self.texture,tuple(pool_media.items()),tuple(cutout_media.items()),
                   impact,capture)
        if signature != self.signature:
            gl.glBindVertexArray(self.renderer.vao)
            for index,a in enumerate(actors):
                scene=self.scene_for(a)
                if morphing and len(actors)==2 and index==0:
                    continue
                if direct_handoff:
                    scene.handoff=direct_handoff;direct_handoff=None;scene.clock=None
                if morphing:
                    if scene.handoff:
                        scene.handoff.close();scene.handoff=None
                    identity=(morph_source['clip'],a['clip'],a['scene'],a['revision'])
                    if (self.morph_identity!=identity and morph_clock and morph_clock.clock is not None
                            and morph_clock.key==morph_source['scene']):
                        scene.audio=deepcopy(morph_clock.audio)
                        scene.clock=a['time']-max(0.,morph_source['time']-morph_clock.clock)
                        scene.identity=(a['revision'],generation)
                    self.morph_identity=identity
                elif index==1:
                    pair=(generation,tuple((b['clip'],b['scene'],b['revision']) for b in actors))
                    if scene.handoff is None or scene.handoff.pair!=pair:
                        if scene.handoff:scene.handoff.close()
                        scene.handoff=GenerativeHandoff(self.instances[actors[0]['clip']],pair,a['time'])
                        scene.clock=None
                elif scene.handoff and scene.identity is not None and scene.identity!=(a['revision'],generation):
                    # projectM's frame identity is revision/time; transport resets
                    # above own its feedback lifetime, just as for native scenes.
                    if not a['scene'].startswith('milkdrop/'):
                        scene.handoff.close();scene.handoff=None
                if scene.handoff:
                    progress=(max(0.,min(1.,(a['time']-scene.handoff.started)/2.)) if scene.handoff.direct
                              else transition_phase(mix,pivot) if index==1 else 1.)
                    scene.handoff.advance(a['time'],progress,levels)
                    if scene.handoff.tail>TAIL_SECONDS:scene.handoff.close();scene.handoff=None
                cutout=self.cutout_for(a,cutout_media)
                if a['scene'].startswith('milkdrop/'):
                    scene.render_pcm(a,pcm,beat=beat,levels=levels,cutout=cutout)
                    if scene.handoff and not scene.handoff.seeded:
                        scene.handoff.seeded.append(dict(passId='host-feedback',semantic='rendered-colour',route='image-to-feedback'))
                    gl.glBindVertexArray(self.renderer.vao)
                    continue
                tex,size,next_tex,next_size,blend=self.texture,self.image_size,0,(1,1),0.
                if image and self.explicit_role=='cutout':tex,size=0,(1,1)
                if a['clip'] in pool_media:
                    paths,blend=pool_media[a['clip']]
                    tex,size=self.pool_textures.get(paths[0],(0,(1,1)))
                    next_tex,next_size=self.pool_textures.get(paths[1],(tex,size))
                    if not tex: tex,size=next_tex,next_size
                morph=(-1,1.)
                render_actor=a
                if morphing:
                    weight=morph_progress*morph_progress*(3.-2.*morph_progress)
                    def interpolate(source,target):return source+(target-source)*weight
                    controls={k:interpolate(morph_source['controls'].get(k,v),v) for k,v in a['controls'].items()}
                    render_actor=dict(a,controls=controls,motionTime=interpolate(morph_source['motionTime'],a['motionTime']))
                    morph=(composition,weight)
                scene.render(render_actor,beat,levels,tex,size,generation,next_tex,next_size,blend,cutout,
                             morph=morph,kick_impact=impact,
                             media_identity=self.texture if self.texture in (tex,cutout[0]) else 0)
            if actors:
                incoming=self.instances[actors[-1]['clip']]
                if incoming.handoff:
                    self.encoded=incoming.handoff.render(incoming,self.compositor.target,incoming.handoff.progress,capture,clock=actors[-1]['time'])
                else:
                    self.encoded=self.compositor.render(incoming,incoming,0.,'dissolve',.5,readback=capture)
            else: self.encoded=b''
            self.signature=signature
            self.sequence+=1
            self.last_actors=actors
        before_present=time.perf_counter()
        presentation=self.presenter.present(self.compositor.target.textures[0] if actors else 0,
            self.width,self.height,generation,bool(request.get('playing'))) if self.presenter else {}
        presentation['pixelTransferBytes']=len(self.encoded)
        facts=[dict(**a,frames=self.instances[a['clip']].frames,rms=rms) for a in actors]
        for fact in facts:
            if fact['scene'].startswith('milkdrop/'):
                fact.update(renderer='projectM-4.1.8',audioSource='mixed-pcm-mono',clock='live-wall-clock',
                            seekBehaviour='restart-feedback',upstreamTargetBytesIncluded=False,
                            spriteFeedback=self.instances[fact['clip']].sprite_feedback)
        handoffs=[dict(clip=a['clip'],routes=self.instances[a['clip']].handoff.seeded,
                      materialStrength=self.instances[a['clip']].handoff.strength)
                  for a in actors if self.instances[a['clip']].handoff]
        if self.texture and image and self.explicit_role=='cutout':
            cutout_media={a['clip']:((self.image,self.image),0.) for a in actors
                          if self.supports_cutout(a)}
        milkdrop_media={fact['clip']:cutout_media[fact['clip']] for fact in facts
                        if fact.get('spriteFeedback') and fact['clip'] in cutout_media}
        cutout_media={key:value for key,value in cutout_media.items()
                      if not self.instances[key].key.startswith('milkdrop/')}
        return dict(ok=True,empty=not actors,scenes=facts,position=position,generation=generation,mix=mix,
                    transition='morph' if morphing else transition,generativeHandoffs=handoffs,image=self.image,mediaError=self.media_error,sequence=self.sequence,
                    kickImpact=impact,
                    spriteCount=len(self.pool)+len(self.cutouts),backgroundCount=len(self.pool),cutoutCount=len(self.cutouts),
                    duplicateImagesIgnored=self.duplicate_count,
                    spriteSelection=pool_media,cutoutSelection=cutout_media,milkdropSpriteSelection=milkdrop_media,
                    milkdropSpriteCount=len(milkdrop_media),spriteErrors=dict(self.pool_errors),videoEnabled=False,
                    mediaTextureBytes=sum(s[0]*s[1]*4 for t,s in list(self.pool_textures.values())+list(self.cutout_textures.values())),
                    pixelFormat='bgra8-bottom-up',width=self.width,height=self.height,gpu=self.renderer.gpu,
                    targetBytes=sum(s.byte_size for s in self.instances.values())+self.compositor.target.byte_size
                        +(self.prepared[1].byte_size if self.prepared else 0),original=True,
                    renderMs=(before_present-began)*1000,presentMs=(time.perf_counter()-before_present)*1000,
                    **presentation),self.encoded

    def clear_scenes(self):
        for instance in self.instances.values(): instance.close()
        self.instances.clear()
        if self.prepared:
            self.prepared[1].close()
            self.prepared=None
        self.prepare_attempt=self.morph_identity=self.direct_morph=None
        self.last_actors=[]
        self.signature=None

    def reset(self):
        if self.presenter: self.presenter.present(0,self.width,self.height,-1,False)
        self.clear_scenes()
        self.signature,self.encoded=None,b''
        self.transport_identity=None
        self.last_kick_impact=0.

    def close(self):
        if self.presenter: self.presenter.close(); self.presenter=None
        self.clear_scenes()
        if self.milkdrop_library:self.milkdrop_library.close();self.milkdrop_library=None
        if self.texture: gl.glDeleteTextures([self.texture]); self.texture=0
        for tex,size in self.pool_textures.values(): gl.glDeleteTextures([tex])
        self.pool_textures.clear()
        for tex,size in self.cutout_textures.values():gl.glDeleteTextures([tex])
        self.cutout_textures.clear()
        if self.compositor: self.compositor.close(); self.compositor=None
        if self.renderer: self.renderer.close(); self.renderer=None

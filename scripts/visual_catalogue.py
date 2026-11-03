"""Bounded owned catalogue and scalar representations of scene control types."""
import json
import math
import re
from pathlib import Path

CONTROL_DEFAULTS = dict(intensity=.7, motion=.5, imageMix=.5, scale=.5,
    warp=.35, detail=.5, hue=.5, symmetry=0., orbit=.35, focusX=.5, focusY=.5,
    colourR=.7, colourG=.7, colourB=.7, pulse=.5, mode=0., mirror=0.,
    imageZoom=.35, imageRotation=.5, imageIndex=0., feedback=.65,
    bassDrive=.5, midDrive=.5, trebleDrive=.5, momentum=.5,
    materialMotion=.35, materialMode=0., viewPattern=0., finishGlow=.2)
DISCRETE = {'mode': 4, 'mirror': 2, 'symmetry': 7}

def catalogue(root):
    root=Path(root)
    data=json.loads((root/'original-scenes.json').read_text(encoding='utf-8'))
    if data.get('schema') != 1 or not isinstance(data.get('scenes'),list) or not 1<=len(data['scenes'])<=160:
        raise ValueError('Invalid original catalogue')
    result={}
    for entry in data['scenes']:
        key=entry.get('key','')
        if not re.fullmatch(r'original/[a-z][a-z0-9-]{0,63}',key) or key in result:
            raise ValueError('Invalid or duplicate original scene key')
        scene=root/'scenes'/key.split('/')[1]
        if scene.resolve().parent != (root/'scenes').resolve(): raise ValueError('Scene escapes collection')
        descriptor=json.loads((scene/'scene.json').read_text(encoding='utf-8'))
        controls=descriptor.get('controls',{})
        if not controls or set(controls)-set(CONTROL_DEFAULTS): raise ValueError('Unknown original control schema')
        for name,c in controls.items():
            if c.get('min')!=0 or c.get('max')!=1 or isinstance(c.get('default'),bool) or not isinstance(c.get('default'),(float,int)) or not math.isfinite(c['default']) or not 0<=c['default']<=1:
                raise ValueError('Invalid original control range')
        presets=descriptor.get('presets',[])
        seen=set()
        if not isinstance(presets,list) or len(presets)>16: raise ValueError('Invalid scene presets')
        for preset in presets:
            if not isinstance(preset,dict) or set(preset)!={'id','title','controls'}:
                raise ValueError('Invalid scene preset fields')
            identity=preset['id']
            if not isinstance(identity,str) or not re.fullmatch('[a-z][a-z0-9-]{0,47}',identity) or identity in seen:
                raise ValueError('Invalid scene preset identity')
            seen.add(identity)
            if not isinstance(preset['title'],str) or not 1<=len(preset['title'])<=64:
                raise ValueError('Invalid scene preset title')
            values=preset['controls']
            if not isinstance(values,dict) or not values or set(values)-set(controls):
                raise ValueError('Unknown scene preset control')
            if any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) or not 0<=v<=1 for v in values.values()):
                raise ValueError('Invalid scene preset value')
        result[key]=dict(entry,controls=controls,presets=presets)
    return result

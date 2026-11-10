"""Bounded actual-GL Studio seek/lookahead checks. Run through verification.run_checks.

Arguments: bundle-directory report-file [--source]. No audio device or user data.
"""
from pathlib import Path
import sys, base64, json, time
import numpy as np

root=Path.cwd(); bundle=Path(sys.argv[1]); report=Path(sys.argv[2])
runtime=bundle/'original-visualiser-runtime'
sys.path.insert(0,str(runtime))
if '--source' in sys.argv: sys.path.insert(0,str(root/'scripts'))
import studio_scene_player as module
from visual_scene_worker import decode_frame
results=[]; facts=[]
def check(ok,label): results.append(('PASS' if ok else 'FAIL',label))
def actor(clip,scene): return dict(clip=clip,scene=scene,revision='0/0',controls={},time=1.,motionTime=1.)
player=module.StudioScenePlayer(runtime,(160,90),None)
created=[]; loads=[]; original=module.Shader
def shader(*args,**kwargs):
 created.append(1); return original(*args,**kwargs)
module.Shader=shader
load_asset=player.create_asset
def load(layer,actor):
 loads.append((layer,actor['clip'])); return load_asset(layer,actor)
player.create_asset=load
pcm=base64.b64encode(np.sin(np.arange(2048,dtype=np.float32)*.1).astype('<f4').tobytes()).decode()
def render(layers,t=1.,generation=0):
 request=dict(studioLayers=layers,position=t,generation=generation,pcm=pcm,rate=44100,beat=t*2,bpm=120,playing=True,sequence=len(facts)+1)
 started=time.perf_counter();metadata,pixels=player.render(request)
 facts.append(dict(**metadata,renderMs=(time.perf_counter()-started)*1000))
 check(metadata['ok'] and len(pixels)==160*90*4,'actual Studio pixels delivered at '+str(t))
 if pixels: decode_frame(pixels,160,90).save(report.parent/('scene-'+str(len(facts))+'.png'))
 return pixels
try:
 first={'shader':actor('first','studio/fault-ledger')}
 second={'shader':actor('second','studio/tidal-culture')}
 render(first)
 count=len(created);player.prepare(second)
 check(len(created)==count+1,'next shader is compiled before the boundary')
 count=len(created);render(second,1.1)
 check(len(created)==count,'prepared boundary performs no shader compilation')
 count=len(created);render(second,7.,1)
 check(len(created)==count,'seek resets feedback while retaining compiled current shader')
 second['shader']['controls']['milkMix']=.71
 count=len(created);render(second,7.02,1)
 check(len(created)==count,'parameter edits retain the compiled actor')
 # Failed speculative assets cannot evict the current working layer.
 bad={'shader':actor('bad','studio/does-not-exist')}
 try: player.prepare(bad)
 except Exception: check(False,'speculative missing scene remains isolated')
 render(second,7.03,1)
 check(player.assets['shader'][0][0]=='second','failed lookahead keeps the current scene')
 # A timeline gap must release inactive assets while retaining its prepared successor.
 player.prepare(first); count=len(created)
 metadata,pixels=player.render(dict(studioLayers={},position=8.,generation=1))
 check(metadata.get('empty') and not pixels,'timeline gap stays empty')
 render(first,9.,1)
 check(len(created)==count,'prepared successor survives an intentional timeline gap')
 # Failed preparation is attempted once until its identity changes.
 attempts=len(loads);player.prepare(bad);player.prepare(bad)
 check(len(loads)==attempts+1,'failed speculative identity is attempted once')
 check(len(getattr(player,'prepared',{}))<=4,'lookahead holds at most one prepared actor per layer')
 all_layers={'background':actor('bg','studio-bg/copper'),'sprite':actor('sprite','studio-sprite/circuit'),
             'milk':actor('milk','studio-milk/prism'),'shader':actor('third','studio/tunnel')}
 for _ in range(4):
  count=len(loads);player.prepare(all_layers)
  check(len(loads)-count<=1,'one speculative resource load per delivered frame')
 check(len(player.prepared)==4,'all four future layer resources are ready')
 count=len(loads);render(all_layers,9.03,1)
 check(len(loads)==count,'four-layer boundary uses prepared resources without loading')
 render(all_layers,12.,2)
 check(len(loads)==count,'four-layer seek retains resources and resets feedback')
finally: player.close()
failures=sum(s=='FAIL' for s,_ in results)
report.write_text(''.join(f'{s} {label}\n' for s,label in results)+f'{len(results)} checks; {failures} failures\n')
(report.parent/'frames.json').write_text(json.dumps(facts,indent=2))
sys.exit(bool(failures))

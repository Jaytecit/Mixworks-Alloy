"""Serial bounded GPU coverage of all scenes, named looks and method pairings.

No audio device, generated audio files, stress or capacity benchmark. Pixel
differences establish exercised mechanisms, not artistic or musical acceptance.
"""
import argparse
import base64
import ctypes
import hashlib
import json
from pathlib import Path
import sys
import time

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--image',type=Path,required=True)
    ap.add_argument('--scene',action='append',help='Only these scene slugs for a scoped correction')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    p=OriginalScenePlayer(args.runtime,args.runtime,320,180)
    selected={'original/'+slug for slug in args.scene} if args.scene else set(p.entries)
    if selected-set(p.entries):raise ValueError('Unknown scene selection')
    checks=[];records=[]
    def check(ok,label):
        checks.append(('PASS ' if ok else 'FAIL ')+label)
    def render(key,controls,t,revision,frame):
        x=np.arange(2048)/44100
        pcm=(.15*np.sin(x*2*np.pi*80)*(frame%6<2)+.08*np.sin(x*2*np.pi*900)*(frame%5<3)+.06*np.sin(x*2*np.pi*7000)*(frame%4<2)).astype('<f4')
        q=dict(protocol=6,position=t,beat=t*2,bpm=120,rate=44100,generation=1,mix=0.,
            image=str(args.image.resolve()),capture=True,pcm=base64.b64encode(pcm.tobytes()).decode(),
            scenes=[dict(clip='probe',scene=key,revision=revision,time=t,controls=controls)])
        facts,raw=p.render(q)
        if not facts['ok'] or facts.get('mediaError'):raise AssertionError(facts)
        return q,raw
    try:
        for index,(key,entry) in enumerate(p.entries.items()):
            if key not in selected:continue
            p.reset();unique=set();matrix=[];finite=True;nonblank=True
            try:
                for preset in entry['presets']:
                    for frame in range(12):q,raw=render(key,preset['controls'],frame/30,preset['id'],frame)
                    unique.add(hashlib.sha256(raw).hexdigest())
                    rgb=np.frombuffer(raw,np.uint8).reshape(-1,4)[:,:3]
                    nonblank &= bool(np.ptp(rgb)>12 and np.mean(rgb)>1.)
                    for entryPass in p.instances['probe'].passes:
                        target=entryPass['target'];gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
                        pixels=np.frombuffer(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,gl.GL_FLOAT),np.float32)
                        finite &= bool(np.isfinite(pixels).all())
                check(finite and nonblank,f'{key}: all six presets produce finite, nonblank targets')
                check(len(unique)>=5,f'{key}: named presets exercise distinct rendered outcomes')
                native=entry['presets'][0]['controls']
                for view in range(5):
                    for material in range(4):
                        controls=dict(native,viewPattern=view/4,materialMode=material/3)
                        q,raw=render(key,controls,.4,'methods',12)
                        matrix.append(hashlib.sha256(raw).hexdigest())
                check(len(set(matrix))>=8,f'{key}: all 20 view/image pairings render with distinct methods')
                check(p.render(q)[1]==raw,f'{key}: pause preserves exact pixels')
                records.append(dict(key=key,presets=len(unique),methodOutcomes=len(set(matrix)),finite=finite,nonblank=nonblank))
            except Exception as error:
                check(False,f'{key}: {str(error)[:1800]}')
                records.append(dict(key=key,error=str(error)[:1800]))
            if (index+1)%10==0:print(f'{index+1}/{len(p.entries)} scenes checked',flush=True)
            time.sleep(.02)
    finally:p.close()
    failures=[c for c in checks if c.startswith('FAIL')]
    (args.out/'checks.txt').write_text('\n'.join(checks+[f'{len(checks)} checks; {len(failures)} failures'])+'\n')
    (args.out/'report.json').write_text(json.dumps(dict(scenes=records,failures=failures,
        scope='Bounded GPU mechanisms; not artistic acceptance, listening or native delivery',
        runtimeHashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in args.runtime.glob('*.py')}),indent=2))
    print(f'{len(checks)} checks; {len(failures)} failures',flush=True)
    return int(bool(failures))

if __name__=='__main__':raise SystemExit(main())

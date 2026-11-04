"""Bounded GPU checks for image generation, palettes and continuous particle resolve."""
import argparse
import base64
import ctypes
import json
from pathlib import Path
import sys
import time

SCENES = ('neon-culture','chromatic-shoal','terrain-prism','liquid-memory','signal-fracture')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    w,h=320,180
    fixture=Image.new('RGB',(256,256),'#07101b');draw=ImageDraw.Draw(fixture)
    draw.ellipse((18,18,238,238),fill='#ef6b10');draw.rectangle((60,80,196,175),fill='#15b4ed')
    path=args.out.resolve()/'source.png';fixture.save(path)
    other=args.out.resolve()/'other.png';Image.fromarray(255-np.asarray(fixture)).save(other)
    cut=args.out.resolve()/'cutout.png';rgba=fixture.convert('RGBA');rgba.putalpha(Image.fromarray((np.asarray(fixture)[:,:,0]>20).astype('uint8')*255));rgba.save(cut)
    player=OriginalScenePlayer(args.runtime,args.runtime,w,h);player.pool=[];player.cutouts=[]
    checks=[];metrics={}

    def check(ok,label):
        checks.append(dict(ok=bool(ok),label=label))
        if not ok:raise AssertionError(label)
    def request(slug,t,controls=None,image=path,loud=True,beat=None):
        x=np.arange(2048)/44100
        pcm=((.2*np.sin(2*np.pi*80*x)+.12*np.sin(2*np.pi*900*x)+.07*np.sin(2*np.pi*7000*x))*(int(t*30)%10<4) if loud else np.zeros(2048)).astype('<f4')
        return dict(protocol=6,position=t,beat=t*2 if beat is None else beat,bpm=120,rate=44100,generation=1,
            mix=0.,capture=True,image=str(image),pcm=base64.b64encode(pcm.tobytes()).decode(),
            scenes=[dict(clip='probe',scene='original/'+slug,revision='image-proof',time=t,controls=controls or {})])
    def state():
        result={}
        for item in player.instances['probe'].passes:
            if item.get('state'):
                target=item['target'];gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
                result[item['id']]=np.frombuffer(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,gl.GL_FLOAT),np.float32).copy()
        return result
    def sequence(slug,image=path,loud=True):
        player.reset()
        for frame in range(45):q=request(slug,frame/30,image=image,loud=loud);facts,raw=player.render(q)
        check(facts['ok'] and not facts.get('mediaError'),slug+' renders')
        return q,raw,state()
    try:
        for slug in SCENES:
            q,raw,before=sequence(slug)
            check(all(np.isfinite(a).all() for a in before.values()),slug+' finite state')
            check(player.render(q)[1]==raw,slug+' exact pause')
            variants=[]
            for palette in range(1,8):
                q['scenes'][0]['controls']=dict(paletteStyle=palette/7,paletteAmount=1.)
                _,colour=player.render(q);variants.append(colour)
                after=state()
                check(all(np.array_equal(before[k],after[k]) for k in before),slug+f' palette {palette} preserves paused simulation')
            check(len(set(variants))==7,slug+' seven distinct selectable colour treatments')
            if slug in ('neon-culture','liquid-memory'):
                q['scenes'][0]['controls']['materialMode']=2/3
                player.render(q);after=state()
                check(all(np.array_equal(before[k],after[k]) for k in before),slug+' material edit preserves accumulated physical state')
            _,other_raw,other_state=sequence(slug,image=other)
            difference=float(np.mean(abs(np.frombuffer(raw,np.uint8).astype(float)-np.frombuffer(other_raw,np.uint8))))
            metrics[slug+'ImagePixels']=difference
            check(difference>.5,slug+' source image changes generated output')
            if slug in ('neon-culture','chromatic-shoal','liquid-memory'):
                # These generators must ingest source information into real state.
                difference=max(float(np.mean(abs(before[k]-other_state[k]))) for k in before)
                metrics[slug+'ImageState']=difference
                check(difference>.0001,slug+' source image changes evolving state')
            _,quiet,quiet_state=sequence(slug,loud=False)
            if before:
                difference=max(float(np.mean(abs(before[k]-quiet_state[k]))) for k in before)
            else:difference=float(np.mean(abs(np.frombuffer(raw,np.uint8).astype(float)-np.frombuffer(quiet,np.uint8))))
            metrics[slug+'MusicalChange']=difference
            check(difference>.00001,slug+' music changes generation')
            _,transparent,transparent_state=sequence(slug,image=cut)
            check(np.std(np.frombuffer(transparent,np.uint8).reshape(-1,4)[:,:3])>8,slug+' accepts transparent source artwork')
            time.sleep(.01)

        # Render passage with clock advancement through the former two-beat hold.
        for slug in ('chromatic-shoal','light-loom','firefly-assembly-edge-peel'):
            player.reset();frames=[];positions=[]
            for frame in range(100):
                t=frame/30;beat=9.+t*2.
                q=request(slug,t,loud=False,beat=beat)
                _,raw=player.render(q)
                if 11.8<=beat<=14.2:
                    frames.append(raw)
                    s=state()['particles'].reshape(-1,4)
                    width=next(p['target'].width for p in player.instances['probe'].passes if p['id']=='particles')
                    height=len(s)//width
                    home=np.stack(np.meshgrid((np.arange(width)+.5)/width,(np.arange(height)+.5)/height),-1).reshape(-1,2)
                    passage=np.sin(((beat-12)%32)*np.pi/16)
                    positions.append(home+(s[:,:2]-home)*passage)
            check(len(set(frames))==len(frames),slug+' no repeated frame during image passage')
            speed=[float(np.mean(np.linalg.norm(b-a,axis=1))) for a,b in zip(positions,positions[1:])]
            metrics[slug+'MinimumPassageStep']=min(speed)
            check(min(speed)>.00001,slug+' fragments continue moving through resolve and former hold')
            target=player.instances['probe'].final
            # Ordinary transport pause must remain exact after continuous playback.
            check(player.render(q)[1]==raw,slug+' still pauses exactly with transport')

        # Shared display code changed; compile one small frame of every active scene.
        for key in player.entries:
            player.reset();facts,raw=player.render(request(key.split('/')[1],0.))
            check(facts['ok'] and not facts.get('mediaError'),key+' compiles with palette display')
            time.sleep(.002)
    finally:
        player.close()
        (args.out/'report.json').write_text(json.dumps(dict(checks=checks,metrics=metrics,
            scope='Bounded shader, causality, pause and passage checks; no stress/full regression or artistic acceptance'),indent=2))
    print(f'{len(checks)} checks; 0 failures')


if __name__=='__main__':main()

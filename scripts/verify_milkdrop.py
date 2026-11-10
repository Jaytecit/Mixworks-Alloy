"""Bounded GPU proof of imported presets and mixed-engine composition (no audio device)."""
import argparse
import base64
import json
from pathlib import Path
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--library',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from OpenGL import GL as gl
    from PIL import Image, ImageDraw
    from milkdrop_catalogue import catalogue
    from milkdrop_scene import Library, MilkDropScene
    from original_visual_gl import Renderer
    from original_scene_player import OriginalScenePlayer
    from visual_scene_worker import decode_frame
    entries=catalogue(args.library)
    if not entries:raise ValueError('Supply at least one explicit local import')
    key=next(iter(entries))
    args.out.mkdir(parents=True,exist_ok=False)
    rows=[]
    def check(ok,label):
        rows.append(('PASS ' if ok else 'FAIL ')+label)
        (args.out/'checks.txt').write_text('\n'.join(rows)+'\n')
        if not ok:raise AssertionError(label)
    width,height=640,360
    def read(scene):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,scene.final.fbos[0])
        return bytes(gl.glReadPixels(0,0,width,height,gl.GL_BGRA,gl.GL_UNSIGNED_BYTE))
    renderer=Renderer(); quiet=loud=None
    try:
        library=Library(args.runtime)
        quiet=MilkDropScene(library,entries[key],width,height,key,renderer)
        loud=MilkDropScene(library,entries[key],width,height,key,renderer)
        zero=np.zeros(2048,dtype='<f4')
        for frame in range(90):
            # Two mixed sources, distinct low/high components, through one input.
            sample=np.arange(2048)+frame*1470
            pcm=(.3*np.sin(sample*2*np.pi*80/44100)+.2*np.sin(sample*2*np.pi*330/44100)).astype('<f4')
            actor=dict(revision='0',time=frame/30,controls={'intensity':.5})
            quiet.render_pcm(actor,zero);loud.render_pcm(actor,pcm)
        a,b=read(quiet),read(loud)
        difference=float(np.abs(np.frombuffer(a,np.uint8).astype(float)-np.frombuffer(b,np.uint8)).mean())
        check(difference>1.,'actual imported preset pixels respond to PCM against simultaneous silence')
        check(loud.frames==90 and quiet.frames==90,'both projectM instances rendered every controlled frame')
        before=read(loud);loud.render_pcm(actor,zero)
        check(read(loud)==before and loud.frames==90,'same paused scene clock holds exact pixels despite changed PCM')
        sheet=Image.new('RGB',(width*2,height+36))
        sheet.paste(decode_frame(a,width,height),(0,36));sheet.paste(decode_frame(b,width,height),(width,36))
        draw=ImageDraw.Draw(sheet);draw.text((12,12),'Same preset / silence',fill='white')
        draw.text((width+12,12),'Same preset / mixed 80 Hz + 330 Hz PCM',fill='white')
        sheet.save(args.out/'audio-response.png')
        (args.out/'measurements.json').write_text(json.dumps(dict(scene=key,pixelMeanAbsoluteDifference=difference,
            gpu=renderer.gpu,comparison='simultaneous projectM instances; upstream random/time may differ slightly'),indent=2))
    finally:
        if quiet:quiet.close()
        if loud:loud.close()
        renderer.close()
    player=OriginalScenePlayer(args.runtime,args.runtime,width,height,milkdrop_library=args.library)
    try:
        def request(time,mix=0,transition='dissolve',generation=0,both=False):
            actors=[dict(clip='imported',scene=key,revision='0',time=time,controls={})]
            if both:actors.insert(0,dict(clip='owned',scene='original/prism-bloom',revision='0',time=time,controls={}))
            return dict(protocol=6,scenes=actors,position=time,beat=time*2,bpm=120,rate=44100,mix=mix,
                        generation=generation,transition=transition,playing=True,pcm=base64.b64encode(pcm).decode())
        first,pixels=player.render(request(0))
        paused,held=player.render(request(0))
        check(first['sequence']==paused['sequence'] and pixels==held,'player holds imported frame exactly when paused')
        for i,amount in enumerate((0.,.5,1.)):
            facts,pixels=player.render(request((i+1)/30,amount,both=True))
            check(len(pixels)==width*height*4 and len(facts['scenes'])==2,'owned/imported feedback renders at progress '+str(amount))
            decode_frame(pixels,width,height).save(args.out/f'feedback-{i}.png')
        facts,pixels=player.render(request(.15,.5,'radial',both=True))
        check(facts['transition']=='feedback' and bool(facts['generativeHandoffs']),'imported handoff retains generative feedback')
        reset,pixels=player.render(request(.05,generation=1))
        check(reset['scenes'][0]['frames']==1,'seek generation restarts projectM feedback')
        # A missing optional DLL must never be required by the original path.
        request_owned=request(.2);request_owned['scenes']=[dict(clip='owned',scene='original/prism-bloom',revision='0',time=.2,controls={})]
        facts,pixels=player.render(request_owned)
        check(facts['scenes'][0]['scene']=='original/prism-bloom' and len(pixels)==width*height*4,'original pipeline renders after projectM teardown')
    finally:player.close()
    (args.out/'checks.txt').write_text('\n'.join(rows)+f'\n{len(rows)} checks; 0 failures\n')
    print(f'{len(rows)} checks; 0 failures')


if __name__=='__main__':main()

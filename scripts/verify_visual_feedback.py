"""Focused GPU proof of sprite material and every original/projectM handoff.

Authored disposable images/preset/PCM only; no audio device or production session.
"""
import argparse
import base64
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--scripts',type=Path,default=Path(__file__).parent)
    ap.add_argument('--assets',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=False)
    if os.name=='nt':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.scripts.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    from original_visual_gl import Target,program,scene_source
    checks=[];metrics={};player=None
    def check(ok,label):
        checks.append(dict(ok=bool(ok),label=label))
        if not ok:raise AssertionError(label)
    def read(target,kind=gl.GL_UNSIGNED_BYTE):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
        return bytes(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,kind))
    def picture(raw):
        return Image.frombytes('RGBA',(w,h),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
    w,h=384,216
    assets=args.out/'assets';assets.mkdir()
    shutil.copy2(args.assets/'original-scenes.json',assets/'original-scenes.json')
    shutil.copytree(args.assets/'scenes',assets/'scenes',copy_function=os.link)
    sprites=assets/'Sprites';sprites.mkdir()
    alpha=Image.new('RGBA',(320,160),(255,0,0,0));d=ImageDraw.Draw(alpha)
    d.ellipse((8,12,150,148),fill=(0,230,255,255));d.polygon([(160,145),(230,8),(312,145)],fill=(245,180,0,255))
    alpha.save(sprites/'alpha.png')
    Image.new('RGB',(160,160),(230,45,60)).save(sprites/'opaque.png')
    config=dict(backgroundFolders=[],spriteFolders=['Sprites'],phraseBeats=8,blendBeats=2)
    (assets/'media-library.json').write_text(json.dumps(config))
    library=args.out/'library';library.mkdir()
    preset=b'[preset00]\nfDecay=.965\nzoom=1.012\nrot=.012\nwave_mode=7\nwave_a=.8\nwave_r=.2\nwave_g=.7\nwave_b=1\n'
    identity=hashlib.sha256(preset).hexdigest();folder=library/identity;folder.mkdir()
    (folder/'preset.milk').write_bytes(preset)
    (folder/'import.json').write_text(json.dumps(dict(schema=1,sha256=identity,title='Authored feedback probe',texturePaths=[])))
    milk='milkdrop/'+identity
    pcm=(.28*np.sin(np.arange(2048)*2*np.pi*80/44100)+.12*np.sin(np.arange(2048)*2*np.pi*900/44100)).astype('<f4')
    encoded=base64.b64encode(pcm).decode()
    def actor(key,t,clip):return dict(clip=clip,scene=key,revision='probe',time=t,controls={})
    def request(t,actors,**kwargs):
        return dict(protocol=6,position=t,beat=t*2,bpm=120,rate=44100,generation=0,scenes=actors,pcm=encoded,
                    **dict(dict(mix=0.,transition='feedback',transitionSeconds=16.,capture=True),**kwargs))
    try:
        player=OriginalScenePlayer(args.runtime,assets,w,h,milkdrop_library=library)
        # Independent pixel oracle: source-over once, including soft edges.
        # Exercise the same source adapter used by native scene shader passes.
        alpha_shader='''#version 330
uniform sampler2D userImage;
out vec4 colour;
void main(){
 float opacity=(gl_FragCoord.x-.5)/4.;
 vec4 a=vec4(.1,.2,.3,step(1.,gl_FragCoord.y)),b=vec4(.8,.4,.2,opacity);
 vec3 base=vec3(0);
 colour=vec4(mix(base,mix(a.rgb,b.rgb,b.a),max(a.a,b.a)),1.);
}
'''
        target=Target(5,2);shader=0
        try:
            shader=program(scene_source(alpha_shader))
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[0]);gl.glViewport(0,0,5,2)
            gl.glUseProgram(shader);gl.glBindVertexArray(player.renderer.vao);gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
            actual=np.frombuffer(read(target),np.uint8).reshape(2,5,4)[:,:,:3].astype(float)
            opacity=np.linspace(0,1,5)[:,None]
            foreground=np.array([.8,.4,.2]);background=np.array([.1,.2,.3])
            expected=np.stack((foreground*opacity,foreground*opacity+background*(1-opacity)))*255
            check(np.max(np.abs(actual-expected))<=1.1,'transparent sprite alpha is applied once at 0/25/50/75/100 percent; opaque background colours are preserved')
        finally:
            target.close()
            if shader:gl.glDeleteProgram(shader)
        check(len(player.cutouts)==1,'transparent artwork is indexed separately from opaque backgrounds')
        cutouts=player.cutouts;player.cutouts=[]
        for f in range(90):facts,raw=player.render(request(f/30,[actor(milk,f/30,'milk')]))
        check(not facts['scenes'][0]['spriteFeedback'] and not facts['milkdropSpriteSelection'],'MilkDrop preserves native output with no sprite selected')
        check(player.render(request(89/30,[actor(milk,89/30,'milk')]))[1]==raw,'sprite-free MilkDrop holds exact paused pixels')
        picture(raw).save(args.out/'milkdrop-no-overlay.png')
        player.cutouts=cutouts
        player.reset();player.pool=[]
        for f in range(20):facts,raw=player.render(request(f/30,[actor('original/molten-contours',f/30,'original')]))
        check('original' in facts['cutoutSelection'],'relief scene receives its own nested material source')
        check(player.render(request(19/30,[actor('original/molten-contours',19/30,'original')]))[1]==raw,'scene-local relief material pauses exactly')
        player.render(request(19/30,[actor('original/molten-contours',19/30,'original')],capture=False))
        check(player.render(request(19/30,[actor('original/molten-contours',19/30,'original')]))[1]==raw,'switching capture while paused does not advance nested geometry')
        picture(raw).save(args.out/'original-nested-relief.png')
        player.reset();player.cutouts=[]
        facts,raw=player.render(request(0,[actor('original/solar-wash',0,'opaque')],image=str((sprites/'opaque.png').resolve())))
        check(not facts['cutoutSelection'],'opaque background remains a scene input without a sprite layer')
        # All four engine directions, including long overlaps and retained tail.
        pairs=[('original/prism-bloom','original/solar-wash'),('original/solar-wash',milk),
               (milk,'original/solar-wash'),(milk,milk)]
        for index,(outgoing,incoming) in enumerate(pairs):
            player.reset();player.cutouts=[]
            for f in range(5):facts,last=player.render(request(f/30,[actor(outgoing,f/30,'a')]))
            facts,first=player.render(request(5/30,[actor(outgoing,5/30,'a'),actor(incoming,0,'b')]))
            check(first==last,f'engine pair {index} starts from exact last displayed frame')
            snapshots=[]
            for f in range(1,91):
                t=5/30+f/30;progress=f/90
                q=request(t,[actor(outgoing,t,'a'),actor(incoming,f/30,'b')],mix=progress)
                facts,raw=player.render(q)
                if f in (1,30,60,90):snapshots.append(picture(raw))
                if f==45:
                    a=np.frombuffer(read(player.instances['a'].final),np.uint8).reshape(-1,4).astype(float)
                    b=np.frombuffer(read(player.instances['b'].final),np.uint8).reshape(-1,4).astype(float)
                    result=np.frombuffer(raw,np.uint8).reshape(-1,4)[:,[2,1,0,3]]
                    difference=float(np.abs(result-(a+b)/2).mean());metrics[f'pair-{index}-dissolve-difference']=difference
                    check(difference>.5,f'engine pair {index} generates distinct recurrent output instead of a whole-frame dissolve')
                    check(player.render(q)[1]==raw,f'engine pair {index} pauses exactly mid-transition')
            check(facts['transition']=='feedback' and len(facts['generativeHandoffs'])==1,f'engine pair {index} retains feedback for a long overlap')
            q=request(5/30+91/30,[actor(incoming,91/30,'b')]);facts,tail=player.render(q)
            check(len(facts['generativeHandoffs'])==1,f'engine pair {index} preserves inherited state after outgoing teardown')
            sheet=Image.new('RGB',(w*2,h*2))
            for n,img in enumerate(snapshots):sheet.paste(img,((n%2)*w,(n//2)*h))
            sheet.save(args.out/f'pair-{index}.jpg')
        # Direct joins have no overlap clock; inherit the actual displayed output.
        player.reset()
        for f in range(10):facts,last=player.render(request(f/30,[actor('original/prism-bloom',f/30,'a')]))
        facts,raw=player.render(request(10/30,[actor(milk,0,'b')]))
        check(raw==last and len(facts['generativeHandoffs'])==1,'join without overlap starts from the exact last frame')
        for f in range(1,75):facts,raw=player.render(request((10+f)/30,[actor(milk,f/30,'b')]))
        check(raw!=last,'direct join evolves into the incoming scene')
        q=request(84/30,[actor(milk,74/30,'b')]);q['generation']=1
        facts,raw=player.render(q)
        check(not facts['generativeHandoffs'],'seek generation clears inherited scene state')
        check(gl.glGetError()==gl.GL_NO_ERROR,'all shader paths finish without GL errors')
        metrics['targetBytes']=facts['targetBytes'];metrics['gpu']=facts['gpu']
        player.close();player=None
        player=OriginalScenePlayer(args.runtime,assets,640,180,milkdrop_library=library)
        for f in range(60):
            actors=[actor(milk,f/30,'wide')]
            if f>=30:actors.append(actor('original/solar-wash',(f-30)/30,'incoming'))
            facts,raw=player.render(request(f/30,actors,mix=max(0.,(f-30)/30)))
        check(len(raw)==640*180*4 and np.std(np.frombuffer(raw,np.uint8).reshape(-1,4)[:,:3])>3.
              and len(facts['generativeHandoffs'])==1 and gl.glGetError()==gl.GL_NO_ERROR,
              '32:9 sprite feedback and mixed-engine handoff render finite nonblank output')
        Image.frombytes('RGBA',(640,180),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB').save(args.out/'wide-feedback.png')
    finally:
        if player:player.close()
        (args.out/'report.json').write_text(json.dumps(dict(checks=checks,metrics=metrics),indent=2))
        (args.out/'checks.txt').write_text('\n'.join(('PASS ' if c['ok'] else 'FAIL ')+c['label'] for c in checks)+f'\n{len(checks)} checks; {sum(not c["ok"] for c in checks)} failures\n')
    print(f'{len(checks)} checks; 0 failures')


if __name__=='__main__':main()

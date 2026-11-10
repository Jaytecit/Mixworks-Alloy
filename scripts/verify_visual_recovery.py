"""Small GPU regression for smooth feedback retirement; no app/audio device."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import sys
from types import SimpleNamespace


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--scripts',type=Path,default=Path(__file__).parent)
    ap.add_argument('--assets',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if os.name=='nt':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.scripts.resolve()))
    import numpy as np
    from PIL import Image
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    from original_visual_gl import Target,program
    from visual_handoff import GenerativeHandoff, TAIL_SECONDS
    checks=[];metrics={};w,h=320,180
    def check(ok,label):checks.append(dict(ok=bool(ok),label=label))
    def read(target):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
        return np.frombuffer(gl.glReadPixels(0,0,w,h,gl.GL_RGBA,gl.GL_UNSIGNED_BYTE),np.uint8).reshape(h,w,4).copy()
    def picture(pixels,path):Image.fromarray(pixels).transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB').save(path)
    preset=b'MILKDROP_PRESET_VERSION=201\n[preset00]\nfDecay=.965\nzoom=1.012\nrot=.012\nwave_mode=7\nwave_a=.8\nwave_r=.2\nwave_g=.7\nwave_b=1\n'
    identity=hashlib.sha256(preset).hexdigest();milk='milkdrop/'+identity
    library=args.out/'library';folder=library/identity;folder.mkdir(parents=True)
    (folder/'preset.milk').write_bytes(preset)
    (folder/'import.json').write_text(json.dumps(dict(schema=1,sha256=identity,title='Recovery probe',texturePaths=[])))
    player=OriginalScenePlayer(args.runtime,args.assets,w,h,milkdrop_library=library)
    targets=[];shaders=[];handoff=None
    try:
        gl.glBindVertexArray(player.renderer.vao)
        # Moving, high-contrast detail reveals temporal softening independently
        # of any particular scene or projectM's nondeterministic wall clock.
        source=Target(w,h);incoming=Target(w,h);output=Target(w,h)
        targets.extend((source,incoming,output))
        shader=program('''#version 330
uniform vec2 resolution;uniform float phase;out vec4 colour;
void main(){vec2 uv=gl_FragCoord.xy/resolution;
float v=step(.5,fract(uv.x*24.+phase));
colour=vec4(v,.2+.6*uv.y,1.-v,1.);}''');shaders.append(shader)
        def draw(target,t):
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front]);gl.glViewport(0,0,w,h)
            gl.glUseProgram(shader);gl.glUniform2f(gl.glGetUniformLocation(shader,'resolution'),w,h)
            gl.glUniform1f(gl.glGetUniformLocation(shader,'phase'),t*1.7)
            gl.glDrawArrays(gl.GL_TRIANGLES,0,3)
        draw(source,0)
        handoff=GenerativeHandoff(SimpleNamespace(final=source),('probe',(('a','source'),('b','incoming'))),0)
        levels=dict(energy=.5,treble=.4)
        handoff.advance(0,0,levels);draw(incoming,0)
        handoff.render(SimpleNamespace(final=incoming),output,0,False,clock=0)
        check(np.array_equal(read(source),read(output)),'join retains exact initial image')
        errors=[];frames=[]
        endpoint=round(TAIL_SECONDS*60)
        for f in range(1,endpoint+2):
            t=f/60;draw(incoming,t);handoff.advance(t,1,levels)
            handoff.render(SimpleNamespace(final=incoming),output,1,False,clock=t)
            if f in (1,endpoint//2,endpoint-1,endpoint,endpoint+1):
                actual,native=read(output),read(incoming)
                errors.append(dict(t=t,error=float(np.abs(actual[:,:,:3].astype(float)-native[:,:,:3]).mean())))
                if f<=endpoint:frames.append((actual,native,t))
            if f==endpoint-1:check(errors[-1]['error']<.1,'last pre-retirement frame converges to sharp live detail')
            if f==endpoint:
                check(handoff.strength==0,'inherited shader material reaches zero before retirement')
                check(np.array_equal(read(output),read(incoming)),'retirement output exactly matches native pixels')
                old=read(output);handoff.render(SimpleNamespace(final=incoming),output,1,False,clock=t)
                check(np.array_equal(old,read(output)),'recovery holds exact paused pixels')
        metrics['syntheticLiveError']=errors
        sheet=Image.new('RGB',(w*2,h*len(frames)))
        for i,(actual,native,t) in enumerate(frames):
            for j,pixels in enumerate((actual,native)):
                sheet.paste(Image.fromarray(pixels).transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB'),(j*w,i*h))
        sheet.save(args.out/'recovery-vs-live.png')
        handoff.close();handoff=None
        # Real player lifetime, incoming typed material, direct and overlap joins.
        player.pool=[];player.cutouts=[];player.sprite_pool=[]
        def actor(key,t,clip):return dict(clip=clip,scene=key,revision='recovery',time=t,controls={})
        def req(t,actors,mix=0):return dict(protocol=6,position=t,beat=t*2,bpm=120,rate=44100,generation=0,
            scenes=actors,pcm='',mix=mix,transition='feedback',transitionSeconds=2,capture=True)
        original_a='original/prism-bloom';original_b='original/solar-wash'
        pairs=[(original_a,original_b,False),(original_a,milk,False),
               (milk,original_b,False),(milk,milk,False),
               (original_a,original_b,True),(original_a,milk,True)]
        for pair_index,(a,b,direct) in enumerate(pairs):
            player.reset()
            label=f'pair {pair_index} ({direct=})';saw_tail=False
            player.render(req(0,[actor(a,0,'a')]))
            for f in range(1,73):
                t=f/20;bt=t-.05
                actors=[actor(b,bt,'b')]
                if not direct and bt<=2:actors.insert(0,actor(a,t,'a'))
                facts,raw=player.render(req(t,actors,min(1,bt/2)))
                if f==1:check(len(facts['generativeHandoffs'])==1,f'{label}: captures join')
                scene=player.instances['b']
                if not saw_tail and scene.handoff and scene.handoff.tail>=TAIL_SECONDS-.051:
                    saw_tail=True
                    actual=np.frombuffer(raw,np.uint8).reshape(h,w,4)[:,:,[2,1,0,3]]
                    error=float(np.abs(actual[:,:,:3].astype(float)-read(scene.final)[:,:,:3]).mean())
                    metrics[f'pair-{pair_index}-lastLiveError']=error
                    check(error<1.,f'{label}: real scene is sharp before resource retirement')
                    picture(actual,args.out/f'pair-{pair_index}-before-retirement.png')
            check(saw_tail,f'{label}: exercised late recovery')
            check(not facts['generativeHandoffs'],f'{label}: bounded resources retire')
            check(player.render(req(t,actors,1))[1]==raw,f'{label}: post-retirement pause holds pixels')
        check(gl.glGetError()==gl.GL_NO_ERROR,'no GL errors')
    finally:
        if handoff:handoff.close()
        for target in targets:target.close()
        for shader in shaders:gl.glDeleteProgram(shader)
        player.close()
        (args.out/'report.json').write_text(json.dumps(dict(checks=checks,metrics=metrics),indent=2))
    failures=sum(not c['ok'] for c in checks)
    print(f'{len(checks)} checks; {failures} failures')
    for c in checks:
        if not c['ok']:print('FAIL',c['label'])
    return int(bool(failures))


if __name__=='__main__':raise SystemExit(main())

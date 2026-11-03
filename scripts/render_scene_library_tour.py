"""Bounded soundtrack tour of catalogue scenes or one scene's named looks."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--music',type=Path,required=True)
    ap.add_argument('--first',type=int,default=0);ap.add_argument('--count',type=int,default=20)
    ap.add_argument('--preset-scene',help='Scene slug; captures all six looks instead of a catalogue range')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    flags=0x08000000|0x4000 if sys.platform=='win32' else 0
    w,h,fps=960,540,30;p=OriginalScenePlayer(args.runtime,args.runtime,w,h)
    if args.preset_scene:
        key='original/'+args.preset_scene;e=p.entries[key]
        scenes=[(key,e['title']+' / '+s['title'],s['controls']) for s in e['presets']]
    else:scenes=[(k,e['title'],{}) for k,e in list(p.entries.items())[args.first:args.first+args.count]]
    if not 1<=len(scenes)<=20:raise ValueError('Tours contain 1–20 entries')
    spacing,overlap=3.,.8;duration=(len(scenes)-1)*spacing+4.
    pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss','112','-i',str(args.music),'-t',str(duration+.1),'-ac','1','-ar','44100','-f','f32le','-'],creationflags=flags),dtype='<f4')
    if len(pcm)<int(duration*44100):raise ValueError('Soundtrack excerpt is too short')
    enc=subprocess.Popen([shutil.which('ffmpeg'),'-v','error','-threads','1','-filter_threads','1','-f','rawvideo','-pixel_format','rgb24','-video_size',f'{w}x{h}','-framerate',str(fps),'-i','-','-ss','112','-i',str(args.music),'-t',str(duration),'-map','0:v','-map','1:a','-c:v','libx264','-threads','1','-preset','veryfast','-crf','20','-pix_fmt','yuv420p','-c:a','aac','-b:a','160k','-movflags','+faststart',str(args.out/'tour.mp4')],stdin=subprocess.PIPE,creationflags=flags)
    checks=[];cost=[];strips={};samples={}
    def pixels(target):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front]);return bytes(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,gl.GL_UNSIGNED_BYTE))
    try:
        for frame in range(round(duration*fps)):
            t=frame/fps;i=min(len(scenes)-1,int(t//spacing));indices=[i]
            if i and t<i*spacing+overlap:indices=[i-1,i]
            actors=[dict(clip=str(j),scene=scenes[j][0],revision='library-tour',time=t-j*spacing,controls=scenes[j][2]) for j in indices]
            end=round(t*44100);block=pcm[max(0,end-2048):end]
            q=dict(protocol=6,position=t,beat=(112+t-.200405)*121.98/60,bpm=121.98,rate=44100,
                mix=(t-i*spacing)/overlap if len(actors)==2 else 0.,generation=1,capture=True,
                pcm=base64.b64encode(block.tobytes()).decode(),image='',scenes=actors,transition='dissolve',transitionSeconds=overlap)
            facts,raw=p.render(q);assert facts['ok'] and not facts.get('mediaError'),facts;cost.append(facts['renderMs'])
            if len(actors)==2 and frame%6==0:
                # Dissolve directly mixes both live final scenes. Check against
                # their actual GPU pixels instead of expecting a fragment handoff.
                a=np.frombuffer(pixels(p.instances[str(i-1)].final),np.uint8).astype(float).reshape(-1,4)
                b=np.frombuffer(pixels(p.instances[str(i)].final),np.uint8).astype(float).reshape(-1,4)
                actual=np.frombuffer(raw,np.uint8).reshape(-1,4)[:,[2,1,0,3]]
                amount=q['mix'];weight=amount*amount*(3.-2.*amount)
                assert np.max(abs(actual-(a*(1.-weight)+b*weight)))<=1.1,'Live dissolve differs from current scene pixels'
                checks.append(t)
            im=Image.frombytes('RGBA',(w,h),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
            draw=ImageDraw.Draw(im);draw.rectangle((0,h-28,w,h),fill='#10151d');draw.text((12,h-21),' -> '.join(scenes[j][1] for j in indices),fill='white')
            enc.stdin.write(im.tobytes())
            local=frame-round(i*spacing*fps)
            if local in (30,55,80):
                strips.setdefault(i,[]).append(im.resize((320,180)))
                samples.setdefault(i,[]).append(hashlib.sha256(raw).hexdigest())
                if local==55:im.save(args.out/f'{i+1:02}-{scenes[i][0].split("/")[1]}.jpg',quality=92)
            if frame%300==0:print(f'{t:.0f}/{duration:.0f}s captured',flush=True)
            time.sleep(.003)
        assert p.render(q)[1]==raw,'Paused output changed'
    finally:
        enc.stdin.close();result=enc.wait();p.close()
    assert result==0
    for page in range((len(scenes)+5)//6):
        sheet=Image.new('RGB',(960,6*180),'#11151a')
        for row in range(6):
            for col,im in enumerate(strips.get(page*6+row,[])):sheet.paste(im,(col*320,row*180))
        sheet.save(args.out/f'motion-{page+1:02}.jpg',quality=92)
    report=dict(scenes=[dict(key=k,title=title,controls=controls) for k,title,controls in scenes],duration=duration,
        liveOutgoingChecks=checks,pausedExact=True,movingSamples={str(i):len(set(s)) for i,s in samples.items()},
        renderMsMedian=float(np.median(cost)),renderMsP95=float(np.percentile(cost,95)),resolution=[w,h],fps=fps,
        sourceMusicSeconds=[112,112+duration],musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(),
        scope='Authored scene timings, unmodified soundtrack; moving GPU review, not native delivery or listening acceptance')
    (args.out/'report.json').write_text(json.dumps(report,indent=2));print('Tour complete',flush=True)

if __name__=='__main__':main()

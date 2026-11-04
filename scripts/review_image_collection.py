"""Bounded, serial GPU review and palette references for the original Image Instruments."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--collection',type=Path,required=True);ap.add_argument('--image',type=Path,required=True)
    ap.add_argument('--music',type=Path,required=True);ap.add_argument('--scene',action='append')
    ap.add_argument('--frames',type=int,default=75)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if not 30<=args.frames<=120:raise ValueError('Use 30–120 frames per bounded review')
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    flags=0x08000000|0x4000 if sys.platform=='win32' else 0
    pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss','112','-i',str(args.music),'-t','4.1','-ac','1','-ar','44100','-f','f32le','-'],creationflags=flags),dtype='<f4')
    selected=json.loads(args.collection.read_text())['scenes']
    if args.scene:selected=[s for s in selected if s['key'].split('/')[1] in args.scene]
    if not selected:raise ValueError('No collection scenes selected')
    player=OriginalScenePlayer(args.runtime,args.runtime,480,270);player.pool=[];player.cutouts=[]
    checks=[];records=[];sheets={}
    def check(ok,label):checks.append(dict(ok=bool(ok),label=label));return bool(ok)
    def state():
        result={}
        for p in player.instances['review'].passes:
            if p.get('state'):
                target=p['target'];gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
                result[p['id']]=np.frombuffer(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,gl.GL_FLOAT),np.float32).copy()
        return result
    def picture(raw):return Image.frombytes('RGBA',(480,270),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
    try:
        for index,record in enumerate(selected):
            key=record['key'];slug=key.split('/')[1];player.reset();cost=[];samples=[]
            try:
                for frame in range(args.frames):
                    t=frame/30;end=round(t*44100)
                    q=dict(protocol=6,position=t,beat=(112+t-.200405)*121.98/60,bpm=121.98,rate=44100,generation=1,mix=0.,
                        capture=True,image=str(args.image.resolve()),pcm=base64.b64encode(pcm[max(0,end-2048):end].tobytes()).decode(),
                        scenes=[dict(clip='review',scene=key,revision='image-collection',time=t,controls={})])
                    facts,raw=player.render(q)
                    if not facts['ok'] or facts.get('mediaError'):raise AssertionError(facts)
                    cost.append(facts['renderMs'])
                    if frame in (args.frames//5,args.frames//2,args.frames-1):samples.append(picture(raw))
                    time.sleep(.003)
                check(player.render(q)[1]==raw,key+' exact pause')
                states=state();check(all(np.isfinite(s).all() for s in states.values()),key+' finite physical state')
                rgb=np.asarray(samples[-1]);check(float(rgb.mean())>.65 and float(rgb.std())>4.,key+' visible structured output')
                check(len({hashlib.sha256(s.tobytes()).hexdigest() for s in samples})==3,key+' three distinct moving samples')
                before={n:s.copy() for n,s in states.items()};palette_hashes=set()
                entry=player.entries[key]
                for preset in entry['presets']:
                    q['scenes'][0]['controls']=preset['controls'];_,praw=player.render(q)
                    palette_hashes.add(hashlib.sha256(praw).hexdigest())
                    filename=slug+('' if preset['id']=='signature' else '--'+preset['id'])+'.jpg'
                    picture(praw).resize((640,360)).save(args.out/filename,quality=91)
                after=state();check(all(np.array_equal(s,after[n]) for n,s in before.items()),key+' palette changes preserve simulation')
                check(len(palette_hashes)>=5,key+' selectable looks produce distinct pixels')
                family=record['family'];sheet=sheets.setdefault(family,Image.new('RGB',(960,8*204),'#0b1018'))
                row=record['composition']
                for col,sample in enumerate(samples):sheet.paste(sample.resize((320,180)),(col*320,row*204+24))
                ImageDraw.Draw(sheet).text((8,row*204+5),record['title']+' / '+record['signature'],fill='white')
                records.append(dict(**record,frames=args.frames,finite=True,lookOutcomes=len(palette_hashes),
                    renderMsMedian=float(np.median(cost)),renderMsP95=float(np.percentile(cost,95)),targetBytes=facts['targetBytes']))
                print(record['title'],'reviewed',flush=True)
            except Exception as error:
                check(False,key+' '+str(error)[:2000]);records.append(dict(**record,error=str(error)[:2000]));print(record['title'],'FAILED',str(error)[:180],flush=True)
    finally:
        player.close()
        for family,sheet in sheets.items():sheet.save(args.out/(family+'-motion.jpg'),quality=93)
        failures=[c for c in checks if not c['ok']]
        report=dict(scenes=records,checks=checks,failures=failures,resolution=[480,270],fps=30,
            imageSha256=hashlib.sha256(args.image.read_bytes()).hexdigest(),musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(),
            sourceMusicSeconds=[112,112+(args.frames-1)/30],scope='Bounded serial GPU motion/palette review, not full regression, endurance or reference parity')
        (args.out/'report.json').write_text(json.dumps(report,indent=2))
        (args.out/'checks.txt').write_text('\n'.join(('PASS ' if c['ok'] else 'FAIL ')+c['label'] for c in checks)+f'\n{len(checks)} checks; {len(failures)} failures\n')
    print(f'{len(checks)} checks; {len(failures)} failures',flush=True)
    return int(bool(failures))

if __name__=='__main__':raise SystemExit(main())

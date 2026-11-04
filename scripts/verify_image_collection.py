"""Focused source/music causality and live blends for the ten image instruments."""
import argparse,base64,ctypes,json,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--collection',type=Path,required=True);ap.add_argument('--image',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    rows=[r for r in json.loads(args.collection.read_text())['scenes'] if r['composition']==0]
    fixture=Image.new('RGB',(256,256),'#1437a8');draw=ImageDraw.Draw(fixture)
    draw.rectangle((24,24,130,210),fill='#f4bb15');draw.ellipse((100,80,235,220),fill='#19e5ae')
    alternate=args.out.resolve()/'alternate.png';fixture.save(alternate)
    player=OriginalScenePlayer(args.runtime,args.runtime,320,180);player.pool=[];player.cutouts=[]
    checks=[];metrics={}
    def check(ok,label):checks.append(dict(ok=bool(ok),label=label));assert ok,label
    def actor(key,t,clip='a'):return dict(clip=clip,scene=key,revision='collection-causality',time=t,controls={})
    def request(t,actors,image,loud=True):
        x=np.arange(2048)/44100
        pcm=((.2*np.sin(x*2*np.pi*80)*(int(t*30)%9<4)+.13*np.sin(x*2*np.pi*900)*(int(t*30)%7<3)+.08*np.sin(x*2*np.pi*7000)*(int(t*30)%5<2)) if loud else np.zeros(2048)).astype('<f4')
        return dict(protocol=6,position=t,beat=t*2,bpm=120,rate=44100,generation=1,mix=0.,capture=True,
            image=str(image.resolve()),pcm=base64.b64encode(pcm.tobytes()).decode(),scenes=actors)
    def pixels(target,kind=gl.GL_FLOAT):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
        dtype=np.float32 if kind==gl.GL_FLOAT else np.uint8
        return np.frombuffer(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,kind),dtype).copy()
    def run(key,image,loud):
        player.reset()
        for frame in range(45):q=request(frame/30,[actor(key,frame/30)],image,loud);facts,raw=player.render(q)
        instance=player.instances['a'];state={p['id']:pixels(p['target']) for p in instance.passes if p.get('state')}
        # Capture the scene before the common palette stage, so colour cycling is
        # not accepted as proof that its generator responds to music.
        state['generated-pixels']=pixels(instance.passes[-2]['target'])
        assert facts['ok'] and not facts.get('mediaError'),facts
        return state
    try:
        for row in rows:
            key=row['key'];music=run(key,args.image,True);different=run(key,alternate,True);quiet=run(key,args.image,False)
            image_delta=max(float(np.mean(abs(v-different[n]))) for n,v in music.items())
            music_delta=max(float(np.mean(abs(v-quiet[n]))) for n,v in music.items())
            metrics[key]=dict(imageDifference=image_delta,musicDifference=music_delta)
            check(all(np.isfinite(v).all() for v in music.values()),key+' finite generation')
            check(image_delta>.0001,key+' source changes generation before palette')
            check(music_delta>.00001,key+' music changes generation before palette')
            print(row['title'],'source/music response verified',flush=True);time.sleep(.01)
        for left,right in [(5,8),(6,9),(7,0),(1,4)]:
            player.reset()
            for frame in range(30):
                t=frame/30;q=request(t,[actor(rows[left]['key'],t,'a'),actor(rows[right]['key'],t,'b')],args.image)
                q.update(mix=.2+.6*frame/29,transition='dissolve',transitionSeconds=16.)
                facts,raw=player.render(q)
            a=pixels(player.instances['a'].final,gl.GL_UNSIGNED_BYTE).reshape(-1,4).astype(float)
            b=pixels(player.instances['b'].final,gl.GL_UNSIGNED_BYTE).reshape(-1,4).astype(float)
            actual=np.frombuffer(raw,np.uint8).reshape(-1,4)[:,[2,1,0,3]]
            amount=q['mix'];weight=amount*amount*(3-2*amount)
            check(np.max(abs(actual-(a*(1-weight)+b*weight)))<=1.1,'new families blend both current live frames '+str((left,right)))
            check(player.render(q)[1]==raw,'new-family blend pauses exactly '+str((left,right)))
    finally:
        player.close();(args.out/'report.json').write_text(json.dumps(dict(checks=checks,metrics=metrics,
            scope='Ten representative mechanisms, source/music causality before palette and four live blends; focused, no stress/endurance/parity claim'),indent=2))
    print(f'{len(checks)} checks; 0 failures',flush=True)

if __name__=='__main__':main()

"""Serial reference stills from an exact installed catalogue; no audio device."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--music',type=Path,required=True)
    ap.add_argument('--only',nargs='+',help='Capture only these scene slugs; output is a partial manifest')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from original_scene_player import OriginalScenePlayer
    pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss','112','-i',str(args.music),'-t','3','-ac','1','-ar','44100','-f','f32le','-'],creationflags=0x08000000|0x4000),dtype='<f4')
    player=OriginalScenePlayer(args.runtime,args.runtime,640,360)
    results=[];thumbs=[]
    try:
        for index,(key,entry) in enumerate(player.entries.items()):
            if args.only and key.split('/')[1] not in args.only: continue
            player.reset();slug=key.split('/')[1]
            for frame in range(76):
                t=frame/30;end=round(t*44100);block=pcm[max(0,end-2048):end]
                q=dict(protocol=6,position=t,beat=t*121.98/60,bpm=121.98,rate=44100,generation=1,mix=0.,image='',capture=frame==75,
                    pcm=base64.b64encode(block.tobytes()).decode(),scenes=[dict(clip='reference',scene=key,revision='reference-1',time=t,controls={})])
                facts,raw=player.render(q);time.sleep(.002)
            assert facts['ok'] and not facts.get('mediaError'),facts
            im=Image.frombytes('RGBA',(640,360),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
            im.save(args.out/(slug+'.jpg'),quality=91)
            tile=Image.new('RGB',(256,170),'#181b20');tile.paste(im.resize((256,144)),(0,0))
            ImageDraw.Draw(tile).text((5,148),f'{index+1:03} {entry["title"]}',fill='white');thumbs.append(tile)
            hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runtime/'scenes'/slug).iterdir() if p.is_file()}
            results.append(dict(key=key,title=entry['title'],preview=slug+'.jpg',sourceHashes=hashes,pixelRange=int(np.frombuffer(im.tobytes(),np.uint8).max())-int(np.frombuffer(im.tobytes(),np.uint8).min())))
            if (index+1)%10==0:print(f'{index+1}/{len(player.entries)} captured',flush=True)
        for page in range((len(thumbs)+19)//20):
            sheet=Image.new('RGB',(1280,680),'#101216')
            for i,tile in enumerate(thumbs[page*20:page*20+20]):sheet.paste(tile,((i%5)*256,(i//5)*170))
            sheet.save(args.out/f'contact-{page+1:02}.jpg',quality=90)
        manifest=dict(schema=1,kind='reference stills; not current mix or motion acceptance',musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(),sourceSeconds=[112,114.5],resolution=[640,360],entries=results,
                      runtimeHashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in args.runtime.glob('*.py')})
        (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2))
        print(f'Complete: {len(results)} scene previews',flush=True)
    finally:player.close()

if __name__=='__main__':main()

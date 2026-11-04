"""Generate selectable reference images for all executable scene presets."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--music',type=Path,required=True)
    ap.add_argument('--image',type=Path,help='Explicit owned review artwork instead of the configured media pool')
    ap.add_argument('--only-preset',help='Generate only this named look after a targeted correction')
    ap.add_argument('--scene',action='append',help='Only these scene slugs for a scoped correction')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from original_scene_player import OriginalScenePlayer
    flags=0x08000000|0x4000 if sys.platform=='win32' else 0
    pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss','112','-i',str(args.music),'-t','2.7','-ac','1','-ar','44100','-f','f32le','-'],creationflags=flags),dtype='<f4')
    p=OriginalScenePlayer(args.runtime,args.runtime,640,360);rows=[];tiles=[]
    selected={'original/'+slug for slug in args.scene} if args.scene else set(p.entries)
    if selected-set(p.entries):raise ValueError('Unknown scene selection')
    try:
        for index,(key,entry) in enumerate(p.entries.items()):
            if key not in selected:continue
            p.reset();slug=key.split('/')[1];looks=[]
            for preset in entry['presets']:
                if args.only_preset and preset['id']!=args.only_preset:continue
                for frame in range(39):
                    t=frame/15;end=round(t*44100);block=pcm[max(0,end-2048):end]
                    q=dict(protocol=6,position=t,beat=(112+t-.200405)*121.98/60,bpm=121.98,rate=44100,
                        generation=1,mix=0.,image=str(args.image.resolve()) if args.image else '',capture=frame==38,pcm=base64.b64encode(block.tobytes()).decode(),
                        scenes=[dict(clip='reference',scene=key,revision=preset['id'],time=t,controls=preset['controls'])])
                    facts,raw=p.render(q);assert facts['ok'] and not facts.get('mediaError'),facts
                im=Image.frombytes('RGBA',(640,360),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
                filename=slug+'.jpg' if preset['id']=='signature' else slug+'--'+preset['id']+'.jpg'
                im.save(args.out/filename,quality=91)
                looks.append(dict(id=preset['id'],title=preset['title'],preview=filename,controls=preset['controls']))
                if preset['id']=='signature':
                    tile=Image.new('RGB',(256,170),'#181b20');tile.paste(im.resize((256,144)),(0,0))
                    ImageDraw.Draw(tile).text((5,148),f'{index+1:03} {entry["title"]}',fill='white');tiles.append(tile)
                time.sleep(.01)
            rows.append(dict(key=key,title=entry['title'],preview=slug+'.jpg',presets=looks,
                sourceHashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (args.runtime/'scenes'/slug).iterdir() if f.is_file()}))
            if (index+1)%5==0:print(f'{index+1}/{len(p.entries)} scenes / {sum(len(e["presets"]) for e in rows)} look images',flush=True)
    finally:p.close()
    for page in range((len(tiles)+19)//20):
        sheet=Image.new('RGB',(1280,680),'#101216')
        for i,tile in enumerate(tiles[page*20:page*20+20]):sheet.paste(tile,((i%5)*256,(i//5)*170))
        sheet.save(args.out/f'contact-{page+1:02}.jpg',quality=91)
    (args.out/'manifest.json').write_text(json.dumps(dict(schema=1,kind='Scene and preset reference stills; not live mix or motion acceptance',
        entries=rows,resolution=[640,360],sourceMusicSeconds=[112,112+38/15],
        musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(),
        runtimeHashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in args.runtime.glob('*.py')}),indent=2))
    print(f'Complete: {len(rows)} scenes / {sum(len(e["presets"]) for e in rows)} preset references',flush=True)

if __name__=='__main__':main()

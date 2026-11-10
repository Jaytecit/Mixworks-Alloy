"""Generate selectable reference images for all executable scene presets."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--music',type=Path)
    ap.add_argument('--procedural',action='store_true',help='Use an owned deterministic pulse instead of music')
    ap.add_argument('--assets',type=Path,help='Catalogue assets; defaults to runtime')
    ap.add_argument('--frames',type=int,default=39,help='Bounded settling frames per look (3 to 76)')
    ap.add_argument('--selection-manifest',type=Path,help='Refresh only scene keys in this reference manifest')
    ap.add_argument('--image',type=Path,help='Explicit owned review artwork instead of the configured media pool')
    ap.add_argument('--only-preset',action='append',help='Generate only these named looks after a targeted correction')
    ap.add_argument('--scene',action='append',help='Only these scene slugs for a scoped correction')
    args=ap.parse_args()
    if not 3<=args.frames<=76 or bool(args.music)==args.procedural:ap.error('Choose music or procedural input; frames must be 3 to 76')
    # A sparse reference capture must still advance retained simulations rather
    # than exceeding their discontinuity bound and resetting on every frame.
    duration=min(38/15,(args.frames-1)/4)
    args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(Path(__file__).resolve().parent if args.assets else args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw
    from original_scene_player import OriginalScenePlayer
    flags=0x08000000|0x4000 if sys.platform=='win32' else 0
    if args.procedural:
        t=np.arange(round(2.7*44100))/44100
        pcm=(.3*np.sin(t*2*np.pi*65)*np.exp(-12*(t%.5))+.08*np.sin(t*2*np.pi*440)).astype('<f4')
    else:
        pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss','112','-i',str(args.music),'-t','2.7','-ac','1','-ar','44100','-f','f32le','-'],creationflags=flags),dtype='<f4')
    assets=args.assets or args.runtime
    p=OriginalScenePlayer(args.runtime,assets,640,360);rows=[];tiles=[]
    selected={e['key'] for e in json.loads(args.selection_manifest.read_text())['entries']} if args.selection_manifest else set(p.entries)
    if args.scene:
        extra={'original/'+slug for slug in args.scene}
        selected=selected|extra if args.selection_manifest else extra
    if selected-set(p.entries):raise ValueError('Unknown scene selection')
    try:
        for index,(key,entry) in enumerate(p.entries.items()):
            if key not in selected:continue
            p.reset();slug=key.split('/')[1];looks=[]
            for preset in entry['presets']:
                if args.only_preset and preset['id'] not in args.only_preset:continue
                for frame in range(args.frames):
                    t=frame/(args.frames-1)*duration;end=round(t*44100);block=pcm[max(0,end-2048):end]
                    q=dict(protocol=6,position=t,beat=(112+t-.200405)*121.98/60,bpm=121.98,rate=44100,
                        generation=1,mix=0.,image=str(args.image.resolve()) if args.image else '',capture=frame==args.frames-1,pcm=base64.b64encode(block.tobytes()).decode(),
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
                sourceHashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (assets/'scenes'/slug).iterdir() if f.is_file()}))
            if (index+1)%5==0:print(f'{index+1}/{len(p.entries)} scenes / {sum(len(e["presets"]) for e in rows)} look images',flush=True)
    finally:p.close()
    for page in range((len(tiles)+19)//20):
        sheet=Image.new('RGB',(1280,680),'#101216')
        for i,tile in enumerate(tiles[page*20:page*20+20]):sheet.paste(tile,((i%5)*256,(i//5)*170))
        sheet.save(args.out/f'contact-{page+1:02}.jpg',quality=91)
    (args.out/'manifest.json').write_text(json.dumps(dict(schema=1,kind='Scene and preset reference stills; not live mix or motion acceptance',
        entries=rows,resolution=[640,360],sourceMusicSeconds=[112,112+duration],
        musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest() if args.music else None,
        proceduralInput=args.procedural,settlingFrames=args.frames,
        imageSha256=hashlib.sha256(args.image.read_bytes()).hexdigest() if args.image else None,
        runtimeHashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (Path(__file__).resolve().parent if args.assets else args.runtime).glob('*.py')}),indent=2))
    print(f'Complete: {len(rows)} scenes / {sum(len(e["presets"]) for e in rows)} preset references',flush=True)

if __name__=='__main__':main()

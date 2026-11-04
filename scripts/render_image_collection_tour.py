"""Soundtrack tour of the eighty authored Image Instruments, interleaved by family."""
import argparse,base64,ctypes,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--collection',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);ap.add_argument('--image',type=Path,required=True);ap.add_argument('--music',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if sys.platform=='win32':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image,ImageDraw,ImageFont
    from original_scene_player import OriginalScenePlayer
    flags=0x08000000|0x4000 if sys.platform=='win32' else 0
    rows=json.loads(args.collection.read_text())['scenes']
    rows=sorted(enumerate(rows),key=lambda item:(item[1]['composition'],item[0]//8));rows=[row for _,row in rows]
    assert len(rows)==80 and len({r['key'] for r in rows})==80
    w,h,fps,frames,warmup=960,540,24,60,48;duration=len(rows)*frames/fps;start=15.
    pcm=np.frombuffer(subprocess.check_output([shutil.which('ffmpeg'),'-v','error','-threads','1','-ss',str(start-warmup/fps),'-i',str(args.music),'-t',str(duration+warmup/fps+.1),'-ac','1','-ar','44100','-f','f32le','-'],creationflags=flags),dtype='<f4')
    if len(pcm)<round((duration+warmup/fps)*44100):raise ValueError('Soundtrack is too short for the tour')
    movie=args.out/'Alloy-Image-Instruments-80-Scene-Tour.mp4'
    encoder=subprocess.Popen([shutil.which('ffmpeg'),'-v','error','-threads','1','-filter_threads','1','-f','rawvideo','-pixel_format','rgb24',
        '-video_size',f'{w}x{h}','-framerate',str(fps),'-i','-','-ss',str(start),'-i',str(args.music),'-t',str(duration),'-map','0:v','-map','1:a',
        '-c:v','libx264','-threads','1','-preset','veryfast','-crf','20','-pix_fmt','yuv420p','-c:a','aac','-b:a','160k',
        '-af',f'afade=t=out:st={duration-.5}:d=0.5','-movflags','+faststart',str(movie)],stdin=subprocess.PIPE,creationflags=flags)
    player=OriginalScenePlayer(args.runtime,args.runtime,w,h);player.pool=[];player.cutouts=[]
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
    costs=[];chapters=[];sheet=Image.new('RGB',(1920,8*132),'#0a0e17')
    try:
        for index,row in enumerate(rows):
            player.reset();elapsed=index*frames/fps;local_cost=[]
            for frame in range(-warmup,frames):
                local=(frame+warmup)/fps;musical=elapsed+frame/fps
                end=round((musical+warmup/fps)*44100);block=pcm[max(0,end-2048):end]
                q=dict(protocol=6,position=local,beat=(start+musical-.200405)*121.98/60,bpm=121.98,rate=44100,generation=1,mix=0.,
                    capture=frame>=0,image=str(args.image.resolve()),pcm=base64.b64encode(block.tobytes()).decode(),
                    scenes=[dict(clip='tour',scene=row['key'],revision='image-instruments-tour',time=local,controls={})])
                facts,raw=player.render(q);assert facts['ok'] and not facts.get('mediaError'),facts
                if frame>=0:
                    image=Image.frombytes('RGBA',(w,h),raw,'raw','BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
                    draw=ImageDraw.Draw(image);draw.rectangle((0,h-34,w,h),fill='#0a0e17')
                    draw.text((14,h-29),f'{index+1:02}/80   {row["title"]}   /   '+row['family'].replace('-',' ').title(),font=font,fill='#edf5ff')
                    encoder.stdin.write(image.tobytes())
                    if frame==frames//2:
                        sample=image.resize((192,108));x=(index%10)*192;y=(index//10)*132
                        sheet.paste(sample,(x,y+24));ImageDraw.Draw(sheet).text((x+3,y+5),f'{index+1:02} '+row['title'],fill='white')
                        image.save(args.out/f'{index+1:02}-{row["key"].split("/")[1]}.jpg',quality=90)
                    local_cost.append(facts['renderMs'])
                time.sleep(.003)
            assert player.render(q)[1]==raw,'Paused tour output changed'
            chapters.append(dict(index=index+1,start=elapsed,length=frames/fps,**row,renderMsMedian=float(np.median(local_cost))))
            costs+=local_cost
            if (index+1)%10==0:print(f'{index+1}/80 scenes captured',flush=True)
    finally:
        encoder.stdin.close();result=encoder.wait();player.close()
    assert result==0,result
    sheet.save(args.out/'all-eighty-contact.jpg',quality=94)
    (args.out/'chapters.json').write_text(json.dumps(chapters,indent=2))
    (args.out/'report.json').write_text(json.dumps(dict(scenes=80,duration=duration,resolution=[w,h],fps=fps,frames=round(duration*fps),
        sourceMusicSeconds=[start,start+duration],warmupSeconds=warmup/fps,renderMsMedian=float(np.median(costs)),renderMsP95=float(np.percentile(costs,95)),
        imageSha256=hashlib.sha256(args.image.read_bytes()).hexdigest(),musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(),
        scope='Actual owned GPU tour; two-second scene pre-roll and hard editorial cuts; not native delivery, reference parity or listening acceptance'),indent=2))
    print(movie,flush=True)

if __name__=='__main__':main()

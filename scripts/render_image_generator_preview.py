"""Small soundtrack preview of the five original image-generator proof scenes."""
import argparse
import base64
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

SCENES = ('neon-culture', 'chromatic-shoal', 'terrain-prism', 'liquid-memory', 'signal-fracture')
PALETTES = ('Original', 'Electric neon', 'Acid lime', 'Solar fire', 'Jewel tones', 'Laser primaries', 'Mono with accent', 'Soft')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--image', type=Path, required=True)
    ap.add_argument('--music', type=Path, required=True)
    ap.add_argument('--start', type=float, default=60.)
    ap.add_argument('--seconds', type=float, default=12.)
    ap.add_argument('--width', type=int, default=960)
    ap.add_argument('--fps', type=int, default=30)
    args = ap.parse_args()
    if not 2 <= args.seconds <= 16 or args.width not in (480, 960, 1280) or args.fps not in (15, 30):
        ap.error('Use a bounded 2–16 second scene, 480/960/1280 width and 15/30 fps')
    args.out.mkdir(parents=True, exist_ok=False)
    if sys.platform == 'win32':
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), 0x4000)
    flags = 0x08000000 | 0x4000 if sys.platform == 'win32' else 0
    sys.path.insert(0, str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer

    w, h, fps = args.width, args.width * 9 // 16, args.fps
    count = round(args.seconds * fps)
    duration = len(SCENES) * count / fps
    pcm = np.frombuffer(subprocess.check_output([
        shutil.which('ffmpeg'), '-v', 'error', '-threads', '1', '-ss', str(args.start),
        '-i', str(args.music), '-t', str(duration + .1), '-ac', '1', '-ar', '44100', '-f', 'f32le', '-'
    ], creationflags=flags), dtype='<f4')
    if len(pcm) < round(duration * 44100):
        raise ValueError('Soundtrack excerpt too short')
    player = OriginalScenePlayer(args.runtime, args.runtime, w, h)
    player.pool = []; player.cutouts = []
    movie = args.out / 'Alloy-Five-Image-Generators.mp4'
    encoder = subprocess.Popen([
        shutil.which('ffmpeg'), '-v', 'error', '-threads', '1', '-filter_threads', '1',
        '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', f'{w}x{h}', '-framerate', str(fps),
        '-i', '-', '-ss', str(args.start), '-i', str(args.music), '-t', str(duration),
        '-map', '0:v', '-map', '1:a', '-c:v', 'libx264', '-threads', '1', '-preset', 'veryfast',
        '-crf', '19', '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a', '160k',
        '-af', 'afade=t=out:st='+str(duration-.5)+':d=0.5', '-movflags', '+faststart', str(movie)
    ], stdin=subprocess.PIPE, creationflags=flags)
    font_path = Path('C:/Windows/Fonts/segoeui.ttf')
    font = ImageFont.truetype(str(font_path), max(12, w//55)) if font_path.exists() else ImageFont.load_default()
    sheet = Image.new('RGB', (w, 5*(h//2+32)), '#0a0d15')
    records = []
    try:
        for index, slug in enumerate(SCENES):
            player.reset()
            entry = player.entries['original/'+slug]
            frames = []; metrics = []
            # Every scene has its own signature, plus two explicit palette choices.
            choices = [round(entry['controls']['paletteStyle']['default']*7), (index+2)%5+1, (index+3)%5+1]
            for frame in range(count):
                t = (index*count+frame)/fps; local = frame/fps
                palette = choices[min(2, frame*3//count)]
                controls = dict(paletteStyle=palette/7, paletteAmount=1.)
                end = round(t*44100)
                q = dict(protocol=6, position=t, beat=(args.start+t-.200405)*121.98/60,
                         bpm=121.98, rate=44100, generation=1, capture=True, mix=0.,
                         image=str(args.image.resolve()), pcm=base64.b64encode(pcm[max(0,end-2048):end].tobytes()).decode(),
                         scenes=[dict(clip='example', scene='original/'+slug, revision='five-image-proof', time=local, controls=controls)])
                facts, raw = player.render(q)
                if not facts['ok'] or facts.get('mediaError'): raise AssertionError(facts)
                image = Image.frombytes('RGBA', (w,h), raw, 'raw', 'BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
                draw = ImageDraw.Draw(image); footer = max(26, h//14)
                draw.rectangle((0,h-footer,w,h), fill='#0a0e18')
                draw.text((12,h-footer+3), f'{index+1}/5  {entry["title"]}   /   {PALETTES[palette]}', font=font, fill='#eff6ff')
                encoder.stdin.write(image.tobytes())
                if frame in (count//4, count*3//4):
                    slot = 0 if frame==count//4 else 1
                    sheet.paste(image.resize((w//2,h//2)), (slot*w//2,index*(h//2+32)))
                    ImageDraw.Draw(sheet).text((slot*w//2+8,index*(h//2+32)+h//2+5), entry['title'], fill='white')
                    image.save(args.out/f'{slug}-{slot+1}.jpg', quality=92)
                metrics.append(facts['renderMs'])
                time.sleep(.004)
            assert player.render(q)[1] == raw, 'Paused output changed'
            finite = {}
            for render_pass in player.instances['example'].passes:
                if render_pass.get('state'):
                    target = render_pass['target']; gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,target.fbos[target.front])
                    state = np.frombuffer(gl.glReadPixels(0,0,target.width,target.height,gl.GL_RGBA,gl.GL_FLOAT),np.float32)
                    finite[render_pass['id']] = bool(np.isfinite(state).all())
            assert all(finite.values()), finite
            records.append(dict(scene=slug, palettes=choices, pauseExact=True, finiteState=finite,
                                renderMsMedian=float(np.median(metrics)), renderMsP95=float(np.percentile(metrics,95)),
                                targetBytes=facts['targetBytes']))
            print(entry['title'], 'captured', flush=True)
    finally:
        encoder.stdin.close(); result = encoder.wait(); player.close()
    assert result == 0, result
    sheet.save(args.out/'contact-sheet.jpg', quality=94)
    report = dict(records=records, duration=duration, resolution=[w,h], fps=fps,
                  imageSha256=hashlib.sha256(args.image.read_bytes()).hexdigest(),
                  musicSha256=hashlib.sha256(args.music.read_bytes()).hexdigest(), sourceSeconds=[args.start,args.start+duration],
                  clockBPM=121.98, scope='Actual owned GPU output with soundtrack; authored preview, not reference parity, native delivery or listening acceptance')
    (args.out/'report.json').write_text(json.dumps(report,indent=2))
    print(movie, flush=True)


if __name__ == '__main__': main()

"""Bounded owned-runtime GPU checks: live blends, musical paths and complete imagery.

Uses authored image/PCM fixtures, no audio device or production media. Historical
prototype probes are preserved in the maintenance recovery archive, not executed.
"""
import argparse
import base64
import ctypes
import json
from pathlib import Path
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    if sys.platform == 'win32':
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), 0x4000)
    sys.path.insert(0, str(args.runtime.resolve()))
    import numpy as np
    from PIL import Image, ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer

    w, h = 480, 270
    fixture = Image.new('RGB', (256, 256))
    draw = ImageDraw.Draw(fixture)
    palette = [(235, 35, 35), (35, 220, 35), (35, 35, 235), (225, 205, 25)]
    for i, colour in enumerate(palette):
        x, y = (i % 2) * 128, (i // 2) * 128
        draw.rectangle((x, y, x+127, y+127), fill=colour)
    path = args.out.resolve() / 'quadrants.png'
    fixture.save(path)
    player = OriginalScenePlayer(args.runtime, args.runtime, w, h)
    player.pool = []; player.cutouts = []
    checks = []; metrics = {}

    def check(ok, label):
        checks.append(dict(ok=bool(ok), label=label))
        if not ok:
            raise AssertionError(label)

    def actor(slug, t, clip='a', controls=None):
        return dict(clip=clip, scene='original/'+slug, revision='behaviour', time=t, controls=controls or {})

    def request(t, actors, pcm=b'', **extra):
        q = dict(protocol=6, position=t, beat=t*2, bpm=120, rate=44100, generation=1,
                 mix=0., capture=True, image=str(path), scenes=actors,
                 pcm=base64.b64encode(pcm).decode())
        q.update(extra)
        return q

    def pixels(target, kind=gl.GL_UNSIGNED_BYTE):
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, target.fbos[target.front])
        return gl.glReadPixels(0, 0, target.width, target.height, gl.GL_RGBA, kind)

    def picture(raw):
        return Image.frombytes('RGBA', (w, h), raw, 'raw', 'BGRA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')

    try:
        for transition in ('feedback', 'dissolve', 'radial', 'sweep', 'sweep-reverse'):
            player.reset()
            for frame in range(31):
                progress=frame/30
                q=request(frame/30,[actor('mosaic-kiln',frame/30),actor('pulse-gallery',frame/30,'b')],
                          transition=transition,transitionSeconds=16.,mix=progress)
                facts,raw=player.render(q)
            check(facts['transition']=='feedback' and bool(facts['generativeHandoffs']),
                  f'{transition} request retains recurrent feedback through a long overlap')
            check(player.render(q)[1]==raw,f'{transition} feedback pauses exactly')

        paths = []
        for loud in (False, True):
            player.reset()
            for frame in range(100):
                x = np.arange(2048)/44100
                pcm = ((.2*np.sin(2*np.pi*80*x)+.12*np.sin(2*np.pi*900*x))*(frame%12 < 5)
                       if loud else np.zeros(2048)).astype('<f4')
                player.render(request(frame/30, [actor('firefly-assembly-colour-separation', frame/30)], pcm.tobytes()))
            target = next(e['target'] for e in player.instances['a'].passes if e['id'] == 'particles')
            state = np.frombuffer(pixels(target, gl.GL_FLOAT), np.float32).reshape(-1, 4).copy()
            check(np.isfinite(state).all(), 'particle position/velocity remain finite')
            paths.append(state)
        metrics['musicalPathDifference'] = float(np.mean(abs(paths[1][:, :2]-paths[0][:, :2])))
        check(metrics['musicalPathDifference'] > .01, 'music changes persistent particle positions, not just brightness or size')

        # A glass chart should preserve the image's four large spatial regions.
        # Sampling well inside each region tolerates local refraction and bevels.
        controls = dict(viewPattern=0., materialMode=0., warp=.94, imageMix=1.)
        for slug in ('mosaic-kiln', 'rhombus-foundry', 'enamel-scales', 'spinning-stamps'):
            player.reset()
            q = request(0., [actor(slug, 0., controls=controls)])
            facts, raw = player.render(q)
            image = picture(raw); image.save(args.out/(slug+'-glass.png'))
            a = np.asarray(image).astype(float)
            matches = 0
            for i, (u, v) in enumerate(((.39, .28), (.61, .28), (.39, .72), (.61, .72))):
                x, y = round(u*w), round(v*h)
                sample = np.median(a[y-4:y+5, x-4:x+5], axis=(0, 1))
                sample /= max(1., np.linalg.norm(sample))
                choices = np.array(palette, dtype=float)
                choices /= np.linalg.norm(choices, axis=1)[:, None]
                matches += int(np.argmax(choices @ sample) == i)
            metrics[slug+'Quadrants'] = matches
            check(matches == 4, slug+' glass preserves all four regions of one complete image')
            check(player.render(q)[1] == raw, slug+' glass pauses exactly')
            player.reset()
            hidden = player.render(request(0., [actor(slug, 0., controls=dict(controls, imageMix=0.))]))[1]
            check(np.mean(abs(np.frombuffer(raw, np.uint8).astype(float)-np.frombuffer(hidden, np.uint8))) > 5.,
                  slug+' image control changes the actual transmitted image')
    finally:
        player.close()
        (args.out/'report.json').write_text(json.dumps(dict(checks=checks, metrics=metrics,
            scope='Focused GPU behaviour; no native delivery, stress, listening or artistic acceptance'), indent=2))
    print(f'{len(checks)} checks; 0 failures')


if __name__ == '__main__':
    main()

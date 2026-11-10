"""Render one cached MilkDrop library still in an isolated, low-priority child."""
import argparse
import ctypes
import os
from pathlib import Path
import sys
import time

# The bundled embedded Python deliberately excludes the script directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--key', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if os.name == 'nt':
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), 0x4000)
    import numpy as np
    from OpenGL import GL as gl
    from PIL import Image
    from milkdrop_catalogue import catalogue
    from milkdrop_scene import Library, MilkDropScene
    from original_visual_gl import Renderer
    entries = catalogue(args.library)
    if args.key not in entries:
        raise ValueError('Preset is unavailable. Link its folder again to rescan.')
    renderer = Renderer()
    scene = None
    temporary = args.out.with_suffix('.tmp')
    try:
        scene = MilkDropScene(Library(Path(__file__).parent), entries[args.key], 320, 180, args.key, renderer)
        start = time.monotonic()
        for frame in range(45):
            sample = np.arange(2048)+frame*735
            pulse = .35+.65*max(0., np.cos(frame*np.pi/15))**4
            pcm = (pulse*(.3*np.sin(sample*2*np.pi*80/44100)+.18*np.sin(sample*2*np.pi*330/44100))).astype('<f4')
            scene.render_pcm(dict(revision='preview', time=frame/60, controls={'intensity': .5}), pcm)
            time.sleep(max(0., start+(frame+1)/60-time.monotonic()))
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, scene.final.fbos[0])
        pixels = bytes(gl.glReadPixels(0, 0, 320, 180, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE))
        image = Image.frombytes('RGBA', (320, 180), pixels).transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB')
        args.out.parent.mkdir(parents=True, exist_ok=True)
        image.save(temporary, format='JPEG', quality=88)
        temporary.replace(args.out)
    finally:
        if scene is not None:
            scene.close()
        renderer.close()
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    main()

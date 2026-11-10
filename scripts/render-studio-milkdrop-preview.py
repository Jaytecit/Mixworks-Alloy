"""Render a synchronized MilkDrop on/off comparison without opening an audio device."""

import argparse
from contextlib import ExitStack
from hashlib import sha256
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import wave

ROOT = Path(__file__).resolve().parents[1]
STUDIO = ROOT / "visual-studio"
sys.path.insert(0, str(STUDIO))

import numpy as np
from OpenGL import GL as gl
from PIL import Image, ImageDraw, ImageFont

from audio import Audio, AudioFrame, RATE
from engine import Engine, Shader
from scene import Scene

FPS, BPM = 24, 128
SIZE = 416, 234
CLIP = "MilkDrop-Nested-Comparison.mp4"
PRESET = STUDIO / "authoring" / "Prism Current.milk"
SCENES = (
    ("fault-ledger", "Fault Ledger", "Light floods the cracks"),
    ("tidal-culture", "Tidal Culture", "Light feeds living colonies"),
    ("tunnel", "Vector Tunnel", "Light wraps the walls"),
)


class Soundtrack:
    """An original eight-bar electronic loop shared by the renderers and export."""

    def __init__(self, seconds):
        self.position = 0
        t = np.arange(seconds * RATE) / RATE
        beat = t * BPM / 60
        phase = beat % 1
        kick = .53 * np.exp(-9 * phase) * np.sin(2 * np.pi * (52 * t - .58 * np.exp(-29 * phase)))
        step = np.floor(beat * 2).astype(int)
        note = np.take([36, 36, 43, 36, 39, 39, 46, 34], step % 8)
        frequency = 440 * 2 ** ((note - 69) / 12)
        oscillator = 2 * np.pi * np.cumsum(frequency) / RATE
        offbeat = ((beat + .5) % 1) ** .22 * np.exp(-3.3 * ((beat + .5) % 1))
        bass = .32 * offbeat * (np.sin(oscillator) + .31 * np.sin(2 * oscillator))
        noise = np.random.default_rng(147).uniform(-1, 1, len(t))
        hat = .11 * (noise - np.roll(noise, 1)) * np.exp(-17 * ((beat * 2 + .5) % 1))
        clap = .16 * noise * np.exp(-23 * ((beat + 1) % 2))
        melody_note = np.take([60, 67, 72, 75, 70, 67, 63, 58], step % 8)
        synth_phase = 2 * np.pi * np.cumsum(440 * 2 ** ((melody_note - 69) / 12)) / RATE
        gate = (.5 + .5 * np.cos(2 * np.pi * (beat * 2 - .12))) ** 3
        lead = .13 * gate * (np.sin(synth_phase) + .26 * np.sin(2 * synth_phase + 1.2 * np.sin(beat)))
        tail = np.clip((seconds - t) * 9, 0, 1)
        self.pcm = (np.tanh(kick + bass + hat + clap + lead) * tail).astype(np.float32)

    def samples(self, offsets):
        indices = self.position + np.rint(offsets * RATE).astype(int)
        return np.where(indices >= 0, self.pcm[np.maximum(indices, 0)], 0.)

    def clock(self):
        return BPM, BPM, (self.position * BPM / (RATE * 60)) % 1

    def write(self, path):
        with wave.open(str(path), "wb") as output:
            output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            output.writeframes((self.pcm * 32767).astype("<i2").tobytes())


def defaults(renderer):
    return {name: control.default for name, control in renderer.controls.items()}


def read_target(target, *, as_float=False):
    gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, target.fbos[target.front])
    gl.glReadBuffer(gl.GL_COLOR_ATTACHMENT0)
    kind = gl.GL_FLOAT if as_float else gl.GL_UNSIGNED_BYTE
    return gl.glReadPixels(0, 0, *target.size, gl.GL_RGBA, kind)


def comparison_card():
    w, h = SIZE
    positions = [(20 + column * (w + 12), y) for y in (118, 386) for column in range(len(SCENES))]
    canvas = Image.new("RGB", (positions[2][0] + w + 20, 640), "#11161d")
    draw = ImageDraw.Draw(canvas)
    font_dir = Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts"
    regular = ImageFont.truetype(str(font_dir / "segoeui.ttf"), 16)
    bold = ImageFont.truetype(str(font_dir / "segoeuib.ttf"), 20)
    draw.text((20, 10), "LIVE MILKDROP  /  THREE GLSL SCENES", font=bold, fill="#fff1dc")
    draw.text((20, 40), "WITH NESTED PRESET", font=regular, fill="#eeaa68")
    draw.text((canvas.width - 20, 14), "Prism Current  /  one shared 128 BPM track",
              anchor="ra", font=regular, fill="#bac8d5")
    draw.text((20, 356), "ORIGINAL SCENES  /  SAME TRACK AND TIME", font=regular, fill="#bac8d5")
    for (x, _), (_, title, caption) in zip(positions, SCENES):
        draw.text((x, 67), title, font=bold, fill="#fff1dc")
        draw.text((x, 90), caption, font=regular, fill="#bac8d5")
    return canvas, positions


def render(output, runtime, seconds, ffmpeg):
    output.mkdir(parents=True, exist_ok=True)
    soundtrack = Soundtrack(seconds)
    analysis = Audio()
    analysis.mode, analysis.live = "Preview track", soundtrack
    card, positions = comparison_card()
    stats = []
    started = time.perf_counter()
    with tempfile.TemporaryDirectory() as temp, ExitStack() as stack:
        wav = Path(temp) / "preview.wav"
        soundtrack.write(wav)
        engine = Engine(runtime, SIZE)
        stack.callback(engine.close)
        engine.load(Scene.open(PRESET))
        milk = engine.backend
        milk.set_fps(milk.handle, FPS)
        scenes = []
        for directory, _, _ in SCENES * 2:
            shader = Shader(Scene.open(STUDIO / "scenes" / directory), SIZE, engine.vao)
            stack.callback(shader.close)
            scenes.append((shader, defaults(shader)))
        units = max(len(shader.samplers) for shader, _ in scenes)
        encoder = stack.enter_context(subprocess.Popen([
            ffmpeg, "-v", "error", "-y", "-filter_threads", "1", "-f", "rawvideo", "-pixel_format", "rgb24",
            "-video_size", f"{card.width}x{card.height}", "-framerate", str(FPS), "-i", "-",
            "-i", str(wav), "-c:v", "libx264", "-preset", "veryfast", "-threads", "1",
            "-crf", "17", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "160k",
            "-movflags", "+faststart", str(output / CLIP),
        ], stdin=subprocess.PIPE))
        previous_end = 0
        for frame in range(seconds * FPS):
            end = round(frame * RATE / FPS)
            soundtrack.position = end
            audio = analysis.frame(frame / FPS, 1 / FPS)
            milk.render(AudioFrame(soundtrack.pcm[previous_end:end], audio.uniforms),
                        engine.values, end > previous_end, None)
            previous_end = end
            texture = engine.composite.milk_texture()
            for flag in (gl.GL_BLEND, gl.GL_DEPTH_TEST, gl.GL_SCISSOR_TEST, gl.GL_CULL_FACE):
                gl.glDisable(flag)
            gl.glColorMask(True, True, True, True)
            for unit in range(units):
                gl.glBindSampler(unit, 0)
            pixels = []
            for index, ((shader, values), xy) in enumerate(zip(scenes, positions)):
                shader.render(frame / FPS, 1 / FPS, audio, values, ((0, 0), (0, 0), False),
                              True, engine.composite.empty, 0., 0,
                              milk_texture=texture if index < len(SCENES) else 0)
                raw = read_target(shader.targets["syn_FinalPass"])
                pixels.append(raw)
                image = Image.frombytes("RGBA", SIZE, raw).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                card.paste(image, xy)
            if frame % FPS == 0:
                pairs = zip(pixels[:3], pixels[3:])
                differences = [float(np.mean(np.abs(np.frombuffer(a, np.uint8).astype(np.int16)
                                                     - np.frombuffer(b, np.uint8)))) for a, b in pairs]
                fields = [np.asarray(read_target(scenes[i][0].targets["culture"], as_float=True))[..., :2]
                          for i in (1, 4)]
                reagent_delta = float(np.abs(fields[0] - fields[1]).mean())
                stats.append({"seconds": frame / FPS, "imageDelta": differences, "reagentDelta": reagent_delta})
                if frame in (2 * FPS, 7 * FPS, 12 * FPS):
                    card.save(output / f"comparison-{frame // FPS:02}.png")
            encoder.stdin.write(card.tobytes())
        encoder.stdin.close()
        if encoder.wait() != 0:
            raise RuntimeError("FFmpeg could not encode the comparison.")
    evidence = {
        "frames": seconds * FPS, "fps": FPS, "bpm": BPM,
        "presetSha256": sha256(PRESET.read_bytes()).hexdigest(),
        "pcmSha256": sha256(soundtrack.pcm).hexdigest(),
        "videoSha256": sha256((output / CLIP).read_bytes()).hexdigest(),
        "wallSeconds": round(time.perf_counter() - started, 2), "samples": stats,
    }
    (output / "comparison.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    for i, (_, title, _) in enumerate(SCENES):
        if max(row["imageDelta"][i] for row in stats) <= 4:
            raise RuntimeError(f"{title} showed too little visible change.")
    if max(row["reagentDelta"] for row in stats) <= .01:
        raise RuntimeError("Tidal Culture did not show enough change in stored reagents.")
    print(output / CLIP)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--projectm", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=15)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    args = parser.parse_args()
    if args.seconds < 3:
        parser.error("Render at least three seconds.")
    render(args.output.resolve(), args.projectm.resolve(), args.seconds, args.ffmpeg)

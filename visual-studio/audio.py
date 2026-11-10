"""Demo signal, optional file playback and visual audio analysis."""

from dataclasses import dataclass

import numpy as np

if __package__:
    from .audio_sources import AlloyLive, AudioFile, FilePlayback, STUDIO_PORT
else:
    from audio_sources import AlloyLive, AudioFile, FilePlayback, STUDIO_PORT


RATE, WINDOW = 44100, 2048
OFFSETS = np.arange(-WINDOW, 0) / RATE
HANN = np.hanning(WINDOW)
FREQUENCIES = np.fft.rfftfreq(WINDOW, 1 / RATE)
BANDS = tuple((FREQUENCIES >= low) & (FREQUENCIES < high)
              for low, high in ((35, 170), (170, 1000), (1000, 4000), (4000, 15000)))


@dataclass
class AudioFrame:
    pcm: np.ndarray
    uniforms: dict


def demo(position, bpm):
    t = position + OFFSETS
    phase = np.mod(t * bpm / 60., 1.)
    bass = .55 * np.exp(-phase * 7.) * np.sin(2 * np.pi * (56. * t - .35 * np.exp(-18. * phase)))
    mid = .16 * np.exp(-np.mod(phase + .5, 1.) * 12.) * np.sin(2 * np.pi * 330. * t)
    high = .07 * np.exp(-np.mod(phase * 4., 1.) * 15.) * np.sin(2 * np.pi * 7600. * t)
    return np.ascontiguousarray(bass + mid + high, dtype=np.float32)


class Audio:
    def __init__(self, alloy_port=STUDIO_PORT):
        self.mode, self.bpm, self.gain = "Demo pulse", 124., 1.
        self.track, self.playback, self.live = None, None, None
        self.alloy_port, self.cursor = alloy_port, 0
        self.reset()

    def reset(self):
        self.smooth, self.previous = np.zeros(5), np.zeros(5)
        self.last_beat, self.beat = -10., 0.
        self.position = None
        self.motion_beat = 0.

    def open(self, path):
        track = AudioFile.open(path)
        self.set_mode("Audio file")
        self.track = track

    def set_mode(self, mode):
        live = (self.live or AlloyLive(self.alloy_port)) if mode == "Alloy live" else None
        try:
            self.stop(rewind=True)
        except Exception:
            if live and live is not self.live:
                live.close()
            raise
        if self.live and self.live is not live:
            self.live.close()
        self.live, self.mode = live, mode

    def poll_live(self):
        if self.live:
            self.live.poll(-OFFSETS[0])

    def play(self):
        if self.mode != "Audio file" or self.track is None or self.playback is not None:
            return
        import sounddevice

        if self.cursor >= self.track.frames:
            self.cursor = 0
        playback = FilePlayback(self.track, self.cursor, sounddevice)
        playback.start()
        self.playback = playback

    def stop(self, rewind=False):
        if self.playback:
            playback, self.playback = self.playback, None
            try:
                self.cursor = playback.audible_cursor()
            finally:
                playback.close()
        if rewind:
            self.cursor = 0
            self.reset()

    def close(self):
        try:
            self.stop()
        finally:
            if self.live:
                self.live.close()
                self.live = None

    def frame(self, elapsed, dt):
        detected, mix_bpm, phase = 0., 0., 0.
        if self.mode == "Demo pulse":
            pcm = demo(elapsed, self.bpm)
            mix_bpm, phase = self.bpm, (elapsed * self.bpm / 60.) % 1.
        elif self.playback:
            playback = self.playback
            if playback.stream.active:
                pcm = playback.samples(OFFSETS)
            else:
                self.playback, self.cursor = None, playback.cursor
                playback.close(abort=False)
                if playback.error:
                    raise ValueError(playback.error)
                pcm = np.zeros(WINDOW)
        elif self.live:
            pcm = self.live.samples(OFFSETS)
            detected, mix_bpm, phase = self.live.clock()
        else:
            pcm = np.zeros(WINDOW)
        pcm = np.ascontiguousarray(np.clip(pcm * self.gain, -1., 1.), dtype=np.float32)
        spectrum = abs(np.fft.rfft(pcm * HANN)) / (HANN.sum() / 2)
        levels = np.array([np.sqrt(np.mean(pcm * pcm)) * 3.,
                           *(np.linalg.norm(spectrum[band]) * 2. for band in BANDS)]).clip(0., 1.)
        if dt > 0 or self.position != elapsed:
            hits = np.maximum(0., levels - self.previous) * 4
            self.previous = levels
            self.smooth += (levels - self.smooth) * (1 - np.exp(-max(dt, .001) * 20.))
            if self.mode == "Demo pulse":
                self.beat = float(np.exp(-np.mod(elapsed * self.bpm / 60., 1.) * 9.))
            else:
                if hits[1] > .25 and elapsed - self.last_beat > .24:
                    self.last_beat = elapsed
                self.beat = float(np.exp(-max(0., elapsed - self.last_beat) * 15.))
            self.hits = hits.clip(0., 1.)
            self.position = elapsed
        if self.live and hasattr(self.live, "beat"):
            self.motion_beat = self.live.beat  # absolute native grid beat
        elif self.mode == "Demo pulse":
            self.motion_beat = elapsed * self.bpm / 60.
        elif self.live and mix_bpm:
            predicted = self.motion_beat + max(0., dt) * mix_bpm / 60.
            self.motion_beat = round(predicted - phase) + phase
        else:
            self.motion_beat += max(0., dt) * self.bpm / 60.
        uniforms = {"syn_OnBeat": (self.beat,), "alloy_BeatTime": (self.motion_beat,),
                    "syn_BPM": (detected or mix_bpm,),
                    "syn_BPMSin": ((.5 + .5 * np.cos(phase * 2 * np.pi)) if mix_bpm else 0.,),
                    "alloy_DetectedBPM": (detected,), "alloy_MixBPM": (mix_bpm if self.live else 0.,)}
        for name, level, hit in zip(("", "Bass", "Mid", "MidHigh", "High"), self.smooth, self.hits):
            uniforms[f"syn_{name}Level"] = (float(level),)
            uniforms[f"syn_{name}Hits"] = (float(hit),)
        return AudioFrame(pcm, uniforms)

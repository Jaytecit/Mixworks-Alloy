"""Beat-clock sprite motion checks; no devices, GL contexts or music fixtures."""
import importlib.util
from pathlib import Path
import sys
import unittest

STUDIO = Path(__file__).resolve().parents[1] / 'visual-studio'
sys.path.insert(0, str(STUDIO))
from media import MOTIONS, sprite_motion
from audio import Audio


class SpriteMotionTests(unittest.TestCase):
    def motion(self, name, seconds, bpm=120, beat=None):
        values = {'syn_BPM': (bpm,), 'syn_BPMSin': (0.,), 'syn_OnBeat': (0.,), 'syn_BassLevel': (0.,)}
        if beat is not None:
            values['alloy_BeatTime'] = (beat,)
        return sprite_motion(MOTIONS.index(name), seconds, values)

    def test_motions_follow_bpm(self):
        for name in ('Drift', 'Orbit', 'Spin', 'Pendulum', 'Bounce'):
            with self.subTest(name=name):
                self.assertEqual(self.motion(name, .25, 120), self.motion(name, .5, 60))
                self.assertNotEqual(self.motion(name, .25, 120), self.motion(name, .25, 60))

    def test_bounce_once_per_beat(self):
        bottom = self.motion('Bounce', 0)[0][1]
        self.assertAlmostEqual(bottom, self.motion('Bounce', .5)[0][1])
        self.assertGreater(self.motion('Bounce', .25)[0][1], bottom + .35)

    def test_absolute_native_clock_survives_seek_clip_start_and_ramp(self):
        for name in ('Drift', 'Orbit', 'Spin', 'Pendulum', 'Bounce'):
            with self.subTest(name=name):
                self.assertEqual(self.motion(name, 0, 90, 17.25), self.motion(name, 400, 140, 17.25))

    def test_native_audio_uses_absolute_grid_not_elapsed_seconds(self):
        class Clock:
            beat = 123.5
            def clock(self): return 128., 126., self.beat % 1
            def samples(self, offsets):
                import numpy as np
                return np.zeros(len(offsets))
        audio = Audio(alloy_port=0)
        audio.mode, audio.live = 'Alloy', Clock()
        self.assertEqual(audio.frame(91, 0).uniforms['alloy_BeatTime'], (123.5,))
        audio.live.beat = 4.25
        audio.reset()
        self.assertEqual(audio.frame(2, 0).uniforms['alloy_BeatTime'], (4.25,))
        audio.live = None


if __name__ == '__main__':
    unittest.main()

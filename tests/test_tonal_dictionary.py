import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('dictionary', Path(__file__).resolve().parents[1] / 'scripts/tonal-dictionary-research.py')
dictionary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dictionary)


class TonalDictionaryTests(unittest.TestCase):
    def test_invalid_boundary(self):
        for values in ([0] * 11, [True] * 12, [float('nan')] * 12, [float('inf')] * 12, [-1] * 12, [10 ** 400] * 12):
            with self.assertRaises(ValueError):
                dictionary.rank_chroma(values)

    def test_silence(self):
        self.assertEqual(dictionary.rank_chroma([0] * 12), [])

    def test_known_major_and_minor_triads(self):
        for mode in ('general', 'electronic'):
            for minor, third in ((False, 4), (True, 3)):
                chroma = [0] * 12
                for note in (0, third, 7):
                    chroma[note] = 1
                self.assertEqual(dictionary.rank_chroma(chroma, mode)[0]['key'], 12 if minor else 0)

    def test_all_transpositions(self):
        for mode in ('general', 'electronic'):
            for minor, third in ((False, 4), (True, 3)):
                for root in range(12):
                    chroma = [0] * 12
                    for note in (root, (root + third) % 12, (root + 7) % 12):
                        chroma[note] = 1
                    rows = dictionary.rank_chroma(chroma, mode)
                    self.assertEqual(rows[0]['key'], root + (12 if minor else 0))
                    self.assertAlmostEqual(sum(r['relativeSupport'] for r in rows), 1)
                    self.assertEqual(len({r['key'] for r in rows}), 24)

    def test_level_invariance(self):
        chroma = [.9, .1, .2, .3, .6, .4, .1, .7, .1, .3, .1, .2]
        normal = dictionary.rank_chroma(chroma)
        scaled = dictionary.rank_chroma([x * 1e5 for x in chroma])
        self.assertEqual([r['key'] for r in normal], [r['key'] for r in scaled])
        for a, b in zip(normal, scaled):
            self.assertAlmostEqual(a['relativeSupport'], b['relativeSupport'])
        for factor in (1e308, 1e-300):
            extreme = dictionary.rank_chroma([x * factor for x in chroma])
            self.assertEqual([r['key'] for r in normal], [r['key'] for r in extreme])

    def test_modes_retain_distinct_profiles(self):
        self.assertNotEqual(dictionary.profiles('general'), dictionary.profiles('electronic'))


if __name__ == '__main__':
    unittest.main()

import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('scores', Path(__file__).resolve().parents[1] / 'scripts/score-engine-probes.py')
scores = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scores)


def event(seconds, position=1):
    return dict(seconds=seconds, position=position, meter=4)


class ProbeScoringTests(unittest.TestCase):
    def test_duplicate_detections_cannot_reuse_one_truth_event(self):
        result = scores.score([event(1)], [event(.99), event(1.01)], .07)
        self.assertEqual(result['matched'], 1)
        self.assertEqual(result['precision'], .5)
        self.assertEqual(result['recall'], 1)

    def test_timing_and_bar_position_are_separate(self):
        result = scores.score([event(1), event(1.5, 2)], [event(1.02, 4), event(1.51, 1)], .07)
        self.assertEqual(result['matched'], 2)
        self.assertEqual(result['matchedBarPositionsCorrect'], 0)
        self.assertAlmostEqual(result['maximumMatchedErrorMs'], 20)

    def test_malformed_order_and_unknown_musical_positions_rejected(self):
        for events in ([event(2), event(1)], [event(float('nan'))], [event(1, 0)]):
            with self.assertRaises(ValueError):
                scores.score([event(1)], events, .07)


if __name__ == '__main__':
    unittest.main()

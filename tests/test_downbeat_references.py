import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('references', Path(__file__).resolve().parents[1] / 'scripts/downbeat-references.py')
references = importlib.util.module_from_spec(spec)
spec.loader.exec_module(references)


class DownbeatReferences(unittest.TestCase):
    def test_capture_retains_automatic_and_manual_without_modifying_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            automatic = root / 'analysis'
            automatic.mkdir()
            source = automatic / 'one.json'
            doc = dict(format='mixworks-track-analysis', version=1, analysisRevision=11,
                       source=dict(sha256='a' * 64, bytes=200, frames=8000, sampleRate=8000, channels=1),
                       analysis=dict(bpm=120, downbeatOrigin=.125), manual=dict(bpm=0, beatOrigin=.625),
                       sourcePath='=track.wav', peaksF32='unnecessary waveform')
            raw = json.dumps(doc).encode()
            source.write_bytes(raw)
            saved = root / 'references'
            self.assertEqual(references.capture(automatic, saved), 1)
            self.assertEqual(source.read_bytes(), raw)
            self.assertEqual(references.capture(automatic, saved), 0)
            archived = next(saved.glob('*.json'))
            first = archived.read_bytes()
            self.assertNotIn('peaksF32', json.loads(first))
            doc['analysis']['downbeatOrigin'] = .25
            doc['manual']['beatOrigin'] = .75
            source.write_text(json.dumps(doc))
            self.assertEqual(references.capture(automatic, saved), 1)
            self.assertEqual(archived.read_bytes(), first)
            output = root / 'comparison.csv'
            self.assertEqual(references.export(saved, output), 2)
            with output.open(encoding='utf-8-sig', newline='') as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual({r['user_downbeat_seconds'] for r in rows}, {'0.625', '0.75'})
            self.assertTrue(all(r['track'].startswith("'=") for r in rows))
            with self.assertRaises(FileExistsError):
                references.export(saved, output)

    def test_baseline_opt_in_and_invalid_future_data_are_kept(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'baseline.json'
            doc = dict(format='mixworks-track-analysis', version=1,
                       source=dict(sha256='b' * 64, bytes=200, frames=8000, sampleRate=8000, channels=1),
                       analysis=dict(bpm=120, downbeatOrigin=-1), manual=dict(bpm=0, beatOrigin=-1))
            source.write_text(json.dumps(doc))
            saved = root / 'records'
            self.assertEqual(references.capture(root, saved), 0)
            self.assertEqual(references.capture(root, saved, True), 1)
            doc['version'] = 99
            source.write_text(json.dumps(doc))
            before = source.read_bytes()
            self.assertEqual(references.capture(root, saved, True), 0)
            self.assertEqual(source.read_bytes(), before)
            doc['version'] = 1
            doc['manual']['beatOrigin'] = float('nan')
            self.assertFalse(references.valid(doc))


if __name__ == '__main__':
    unittest.main()

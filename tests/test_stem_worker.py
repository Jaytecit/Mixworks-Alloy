import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import stem_worker as worker


class StemAssetsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.expected = dict(schema=1, sourceSha256='a'*64, frames=101, sampleRate=48000, channels=2)
        self.manifest = dict(self.expected, stems={})
        for name in worker.NAMES:
            path = self.root / (name + '.wav')
            sf.write(path, np.zeros((101, 2), dtype='float32'), 48000, subtype='FLOAT')
            self.manifest['stems'][name] = dict(sha256=worker.digest(path))
        self.save()

    def tearDown(self): self.temp.cleanup()
    def save(self): worker.write_json(self.root / 'manifest.json', self.manifest)

    def test_valid_aligned_set(self):
        self.assertEqual(worker.validate_assets(self.root, self.expected), self.manifest)

    def test_changed_source(self):
        with self.assertRaises(ValueError): worker.validate_assets(self.root, dict(self.expected, sourceSha256='b'*64))

    def test_partial_set(self):
        (self.root/'vocals.wav').unlink()
        with self.assertRaises(OSError): worker.validate_assets(self.root, self.expected)

    def test_corrupted_file(self):
        with (self.root/'drums.wav').open('ab') as f: f.write(b'changed')
        with self.assertRaises(ValueError): worker.validate_assets(self.root, self.expected)

    def test_wrong_clock_and_nonfinite(self):
        for audio, rate in [(np.zeros((100, 2)), 48000), (np.zeros((101, 2)), 44100),
                            (np.zeros((101, 1)), 48000), (np.full((101, 2), np.nan), 48000)]:
            sf.write(self.root/'drums.wav', audio, rate, subtype='FLOAT')
            self.manifest['stems']['drums']['sha256'] = worker.digest(self.root/'drums.wav'); self.save()
            with self.assertRaises(ValueError): worker.validate_assets(self.root, self.expected)

    def test_runtime_and_pcm_invalidate_key(self):
        key = worker.cache_key('source', 'pcm', {'model': 'one'})
        for args in [('changed', 'pcm', {'model':'one'}), ('source','changed',{'model':'one'}),
                     ('source','pcm',{'model':'two'})]:
            self.assertNotEqual(key, worker.cache_key(*args))

    def test_cancel_before_loading_any_model(self):
        worker.write_json(self.root/'job.json', {})
        (self.root/'cancel').touch()
        self.assertEqual(worker.run(self.root/'job.json'), 2)
        self.assertEqual(json.loads((self.root/'status.json').read_text())['state'], 'Cancelled')
        self.assertFalse((self.root/'assets').exists())

    def test_invalid_model_preserves_other_files(self):
        sentinel = self.root/'reviewed-downbeats.json'; sentinel.write_text('keep')
        worker.write_json(self.root/'job.json', {'runtime':str(self.root/'missing.json')})
        self.assertEqual(worker.run(self.root/'job.json'), 1)
        self.assertEqual(sentinel.read_text(), 'keep')
        self.assertFalse((self.root/'assets').exists())

    def test_low_disk_refuses_before_inference(self):
        model=self.root/'model'; model.write_bytes(b'fixture')
        config=dict(model=str(model), modelSha256=worker.digest(model), workerSha256=worker.digest(worker.__file__), packages={})
        worker.write_json(self.root/'runtime.json',config)
        sf.write(self.root/'input.wav',np.zeros((100,2)),44100,subtype='FLOAT')
        worker.write_json(self.root/'job.json',dict(runtime=str(self.root/'runtime.json'),sourceSha256='a'*64,cache=str(self.root/'cache')))
        with patch.object(worker.shutil,'disk_usage',return_value=type('Disk',(),{'free':0})()):
            self.assertEqual(worker.run(self.root/'job.json'),1)
        self.assertIn('free space',json.loads((self.root/'status.json').read_text())['error'])
        self.assertFalse((self.root/'assets').exists())

    def test_recombination_gate(self):
        original=self.root/'input.wav'
        data=np.ones((101,2),dtype='float32')*.25
        sf.write(original,data,48000,subtype='FLOAT')
        for name in worker.NAMES:sf.write(self.root/(name+'.wav'),data/4,48000,subtype='FLOAT')
        self.assertEqual(worker.check_recombination(original,self.root)['residualRatio'],0)
        sf.write(self.root/'vocals.wav',data*3,48000,subtype='FLOAT')
        with self.assertRaisesRegex(ValueError,'recombination error'):worker.check_recombination(original,self.root)

    def test_recombination_cancellation(self):
        sf.write(self.root/'input.wav',np.zeros((101,2)),48000,subtype='FLOAT')
        def cancel():raise worker.Cancelled()
        with self.assertRaises(worker.Cancelled):worker.check_recombination(self.root/'input.wav',self.root,cancel)


if __name__ == '__main__': unittest.main()

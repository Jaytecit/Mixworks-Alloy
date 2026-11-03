"""Safety tests for destructive test-artifact retention, using isolated fixtures."""
from pathlib import Path
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from test_artifacts import begin_run, finish_run, require_space, safe_root


class ArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.local = self.root / '.local'; self.local.mkdir()
        self.run = self.local / 'run'; self.run.mkdir()

    def test_only_new_owned_directory_is_eligible(self):
        for path in (self.root, self.local, self.root / 'Music', self.local / 'source-baselines' / 'run', self.local / 'deps' / 'run', self.run / '..' / 'source-baselines' / 'escape'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                safe_root(path, self.root)
        (self.run / 'user.wav').write_bytes(b'keep')
        with self.assertRaises(ValueError):
            begin_run(self.run, self.root)

    def test_audio_deleted_evidence_and_sessions_kept(self):
        owner = begin_run(self.run, self.root)
        nested = self.run / 'cache'; nested.mkdir()
        for name in ('synthetic.wav', 'decoded.mwpcm', 'features.f32', 'decoded.pcm', 'fixture.mixworks', 'fixture.mixworks(1).bak'):
            (nested / name).write_bytes(b'generated')
        evidence = ('checks.txt', 'song.wav.mixworks-analysis.json', 'analysis.json', 'source.zip', 'capture.png', 'Mixworks.exe')
        for name in evidence:
            (self.run / name).write_bytes(b'keep exactly')
        outside = self.root / 'original.wav'; outside.write_bytes(b'original')
        real_session = self.root / 'real.mixworks'; real_session.write_bytes(b'real arrangement')
        result = finish_run(self.run, owner)
        self.assertEqual(result['files'], 6)
        self.assertEqual(result['bytes'], 54)
        self.assertFalse(result['errors'])
        self.assertEqual(outside.read_bytes(), b'original')
        self.assertEqual(real_session.read_bytes(), b'real arrangement')
        for name in evidence:
            self.assertEqual((self.run / name).read_bytes(), b'keep exactly')
        self.assertTrue((self.run / 'test-audio-removals.jsonl').exists())
        with self.assertRaises(ValueError):
            finish_run(self.run, owner)

    def test_changed_ownership_keeps_audio(self):
        owner = begin_run(self.run, self.root)
        (self.run / 'source.wav').write_bytes(b'keep')
        (self.run / '.mixworks-test-run.json').write_text('{}')
        with self.assertRaises(ValueError):
            finish_run(self.run, owner)
        self.assertEqual((self.run / 'source.wav').read_bytes(), b'keep')

    def test_generated_audio_backups_removed_without_touching_evidence_or_links(self):
        owner = begin_run(self.run, self.root)
        backups = ('cancelled.wav.bak', 'render.wav(2).bak', 'decoded.pcm.bak', 'render.WAV.BAK')
        for name in backups:
            (self.run / name).write_bytes(b'generated')
        evidence = self.run / 'render.wav.bak.json'; evidence.write_bytes(b'keep report')
        outside = self.root / 'original.wav.bak'; outside.write_bytes(b'original')
        os.link(outside, self.run / 'linked.wav.bak')
        result = finish_run(self.run, owner)
        self.assertEqual(result['files'], len(backups))
        self.assertEqual(result['bytes'], len(backups) * len(b'generated'))
        self.assertFalse(result['errors'])
        self.assertTrue(all(not (self.run / name).exists() for name in backups))
        self.assertEqual(evidence.read_bytes(), b'keep report')
        self.assertEqual(outside.read_bytes(), b'original')
        self.assertTrue((self.run / 'linked.wav.bak').exists())

    def test_hard_link_keeps_original_and_link(self):
        owner = begin_run(self.run, self.root)
        original = self.root / 'original.wav'; original.write_bytes(b'original')
        os.link(original, self.run / 'linked.wav')
        result = finish_run(self.run, owner)
        self.assertEqual(result['files'], 0)
        self.assertTrue((self.run / 'linked.wav').exists())
        self.assertEqual(original.read_bytes(), b'original')

    def test_directory_link_cannot_escape(self):
        owner = begin_run(self.run, self.root)
        outside = self.root / 'Music'; outside.mkdir(); (outside / 'song.wav').write_bytes(b'original')
        try:
            (self.run / 'linked').symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest('Windows account cannot create symlinks')
        self.assertEqual(finish_run(self.run, owner)['files'], 0)
        self.assertEqual((outside / 'song.wav').read_bytes(), b'original')

    def test_low_disk_fails_before_work(self):
        with patch('test_artifacts.shutil.disk_usage', return_value=type('Disk', (), dict(free=1024))()):
            with self.assertRaisesRegex(OSError, 'No user data was removed'):
                require_space(self.root)

    def test_reparse_root_is_rejected(self):
        with patch('test_artifacts.linked', side_effect=lambda p: p == self.run):
            with self.assertRaisesRegex(ValueError, 'reparse'):
                safe_root(self.run, self.root)


if __name__ == '__main__':
    unittest.main()

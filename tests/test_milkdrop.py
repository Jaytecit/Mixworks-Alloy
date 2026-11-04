"""Focused imported-catalogue validation; no GPU or audio device required."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from milkdrop_catalogue import catalogue
from original_scene_player import actor


class MilkDropCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        data = b'[preset00]\nfDecay=0.98\n'
        self.hash = hashlib.sha256(data).hexdigest()
        self.folder = self.root/self.hash
        self.folder.mkdir()
        (self.folder/'preset.milk').write_bytes(data)
        self.record = dict(schema=1, sha256=self.hash, title='Example', texturePaths=[])

    def save(self):
        (self.folder/'import.json').write_text(json.dumps(self.record))
        return catalogue(self.root)

    def test_valid_import_and_actor(self):
        entries = self.save()
        self.assertEqual(list(entries), ['milkdrop/'+self.hash])
        parsed = actor(dict(clip='one', revision='0', scene=next(iter(entries)), time=0, controls={}), entries)
        self.assertEqual(parsed['controls'], {'intensity': .5})
        with self.assertRaises(ValueError):
            actor(dict(parsed, controls={'imageMix': .5}), entries)

    def test_tampered_preset_is_not_loaded(self):
        self.save()
        (self.folder/'preset.milk').write_bytes(b'[preset00]\nfDecay=0.5\n')
        self.assertEqual(catalogue(self.root), {})

    def test_incomplete_and_malformed_imports_are_ignored(self):
        self.assertEqual(catalogue(self.root), {})
        for value in ([], None, 'bad', {'schema': 2}, dict(self.record, title=''),
                      dict(self.record, texturePaths=['relative']), dict(self.record, texturePaths=[1])):
            (self.folder/'import.json').write_text(json.dumps(value))
            self.assertEqual(catalogue(self.root), {})

    def test_content_identity_and_bounds(self):
        self.record['sha256'] = '0'*64
        self.assertEqual(self.save(), {})
        self.record['sha256'] = self.hash
        (self.folder/'preset.milk').write_bytes(b'x'*(1024*1024+1))
        self.assertEqual(self.save(), {})

    def test_missing_optional_library_does_not_change_owned_catalogue(self):
        self.assertEqual(catalogue(None), {})
        self.assertEqual(catalogue(self.root/'missing'), {})
        from visual_catalogue import catalogue as owned
        entries = owned(Path(__file__).resolve().parents[1]/'visuals/original')
        self.assertEqual(len(entries), 203)
        self.assertTrue(all(key.startswith('original/') for key in entries))


if __name__ == '__main__':
    unittest.main()

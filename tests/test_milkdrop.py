"""Focused imported-catalogue validation; no GPU or audio device required."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from milkdrop_catalogue import catalogue, validate_source
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
        self.assertEqual(len(entries), 205)
        self.assertTrue(all(key.startswith('original/') for key in entries))

    def linked(self, entries):
        folder = self.root/'linked'
        folder.mkdir(exist_ok=True)
        self.source = self.root/'external collection'
        self.source.mkdir(exist_ok=True)
        manifest = dict(schema=2, root=str(self.source), texturePaths=[str(self.source)], entries=entries)
        (folder/'collection.json').write_text(json.dumps(manifest))
        return catalogue(self.root)

    def test_linked_collection_has_no_128_limit_and_opens_sources_lazily(self):
        source = self.root/'external collection'
        entries = [dict(sha256=f'{i:064x}', title=f'Preset {i}', source=str(source/f'{i}.milk')) for i in range(140)]
        result = self.linked(entries)
        self.assertEqual(len(result), 140)
        self.assertTrue(all(Path(e['preset']).parent == source for e in result.values()))
        self.assertEqual(list(source.iterdir()), [])
        with self.assertRaisesRegex(ValueError, 'unavailable'):
            validate_source(next(iter(result.values())))

    def test_linked_duplicate_hash_and_changed_source(self):
        source = self.root/'external collection'/'preset.milk'
        entry = dict(sha256=self.hash, title='Linked', source=str(source))
        result = self.linked([entry, entry])
        self.assertEqual(len(result), 1)
        source.write_bytes((self.folder/'preset.milk').read_bytes())
        validate_source(result['milkdrop/'+self.hash])
        source.write_text('[preset00]\nfDecay=.5\n')
        with self.assertRaisesRegex(ValueError, 'changed'):
            validate_source(result['milkdrop/'+self.hash])

    def test_linked_malformed_paths_are_not_catalogued(self):
        source = self.root/'external collection'
        base = dict(sha256=self.hash, title='Linked', source=str(source/'preset.milk'))
        entries = [None, dict(base, source='relative.milk'), dict(base, source=str(self.root/'outside.milk')),
                   dict(base, sha256='../bad'), dict(base, title='')]
        self.assertEqual(self.linked(entries), {})

    def test_linked_and_copied_imports_deduplicate(self):
        self.save()
        result = self.linked([dict(sha256=self.hash, title='Linked', source=str(self.root/'external collection'/'same.milk'))])
        self.assertEqual(len(result), 1)


if __name__ == '__main__':
    unittest.main()

"""Malformed or incomplete owned-fixture candidates must never be replaced."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from capacity_fixture import MANIFEST, NAME, ensure_capacity_fixture


class CapacityFixtureTests(unittest.TestCase):
    def test_incomplete_source_is_preserved(self):
        with tempfile.TemporaryDirectory(dir=ROOT / '.local') as directory:
            folder = Path(directory)
            source = folder / NAME
            source.write_bytes(b'owned interrupted fixture')
            with self.assertRaises(ValueError):
                ensure_capacity_fixture(folder)
            self.assertEqual(source.read_bytes(), b'owned interrupted fixture')
            self.assertFalse((folder / MANIFEST).exists())

    def test_unknown_data_and_manifest_are_preserved(self):
        with tempfile.TemporaryDirectory(dir=ROOT / '.local') as directory:
            folder = Path(directory)
            source, manifest = folder / NAME, folder / MANIFEST
            source.write_bytes(b'keep existing data')
            manifest.write_text('{"unrelated":"keep"}', encoding='utf-8')
            before = manifest.read_bytes()
            with self.assertRaises(ValueError):
                ensure_capacity_fixture(folder)
            self.assertEqual(source.read_bytes(), b'keep existing data')
            self.assertEqual(manifest.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()

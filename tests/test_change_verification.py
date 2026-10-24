"""Scoped evidence safety; fixture binaries do not claim application acceptance."""
import copy
import hashlib
from pathlib import Path
import sys
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import test_verification as fixtures
from verification import create_receipt, read_json, validate_receipt, write_json
from change_verification import changed_sources, evidence, validate_scoped_receipt


class ScopedTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.VerificationTests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.folder = self.fixture.folder
        self.exe = self.fixture.exe
        summary = self.fixture.summary()
        write_json(self.folder/'full.json', summary)
        baseline = self.folder/'baseline.json'
        create_receipt(self.folder/'full.json', self.exe, baseline)
        self.before = {'CMakeLists.txt':b'project(Mixworks VERSION 0.55.6)',
                       'Source/Engine.cpp':b'audio', 'Source/AlloyLookAndFeel.cpp':b'orange'}
        self.after = dict(self.before, **{'Source/AlloyLookAndFeel.cpp':b'blue'})
        self.snapshot('before', self.before)
        self.snapshot('after', self.after)
        (self.folder/'visual.png').write_bytes(b'\x89PNG\r\n\x1a\nfixture')
        self.receipt = dict(format='mixworks-release-verification', schemaVersion=3,
            scope='presentation', **fixtures.bundle_identity(self.exe), reviewNote='Fixture colour review',
            source=evidence(self.folder/'after.json'), archive=evidence(self.folder/'after.zip'),
            baseline=dict(receipt=evidence(baseline), source=evidence(self.folder/'before.json'), archive=evidence(self.folder/'before.zip')),
            changedSources=changed_sources(self.before,self.after), visualProof=evidence(self.folder/'visual.png'), suites=[])

    def snapshot(self, name, files):
        write_json(self.folder/(name+'.json'), {n:hashlib.sha256(b).hexdigest() for n,b in files.items()})
        with zipfile.ZipFile(self.folder/(name+'.zip'), 'w') as archive:
            for n,b in files.items(): archive.writestr(n,b)

    def test_presentation_needs_no_native_suites_and_dispatches(self):
        write_json(self.folder/'scoped.json', self.receipt)
        self.assertEqual(validate_receipt(self.folder/'scoped.json', self.exe)['scope'], 'presentation')

    def test_existing_timeline_ui_is_classified_without_new_membership(self):
        before = dict(self.before, **{'Source/TimelineEditing.cpp': b'existing paste UI'})
        after = dict(before, **{'Source/TimelineEditing.cpp': b'seek after successful paste'})
        self.assertEqual(changed_sources(before, after), ['Source/TimelineEditing.cpp'])
        with self.assertRaises(ValueError):
            changed_sources(self.before, after)

    def test_engine_and_build_changes_require_full(self):
        for name in ('Source/Engine.cpp','CMakeLists.txt','Source/NewFile.cpp'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                changed_sources(self.before,dict(self.after,**{name:b'changed'}))
        after = dict(self.after, **{'CMakeLists.txt':b'project(Mixworks VERSION 0.55.7)'})
        self.assertIn('CMakeLists.txt',changed_sources(self.before,after))

    def test_section_suffix_exception_is_exactly_presentation_only(self):
        before = dict(self.before, **{'Source/EnergySections.cpp': b'paint juce::String((int) i + 1) + " ?";\r\nDSP unchanged'})
        after = dict(before, **{'Source/EnergySections.cpp': b'paint juce::String((int) i + 1);\nDSP unchanged'})
        self.assertEqual(changed_sources(before, after), ['Source/EnergySections.cpp'])
        for data in (b'changed DSP', after['Source/EnergySections.cpp'].replace(b'DSP unchanged', b'DSP changed')):
            with self.assertRaises(ValueError):
                changed_sources(before, dict(after, **{'Source/EnergySections.cpp': data}))

    def test_existing_playlist_cell_ui_is_classified_without_allowing_new_sources(self):
        before = dict(self.before, **{'Source/PlaylistTransitions.cpp': b'existing UI factory'})
        after = dict(before, **{'Source/PlaylistTransitions.cpp': b'native accessible cell descriptions'})
        self.assertEqual(changed_sources(before, after), ['Source/PlaylistTransitions.cpp'])
        with self.assertRaises(ValueError):
            changed_sources(self.before, after)

    def test_stem_caption_repair_cannot_admit_job_changes(self):
        original = 'label("Vocals, drums, bass and other • source-aligned WAVs");\r\nstartJob();'.encode()
        repaired = original.replace(' • '.encode(), b' - ')
        before = dict(self.before, **{'Source/StemUI.cpp': original})
        after = dict(before, **{'Source/StemUI.cpp': repaired})
        self.assertEqual(changed_sources(before, after), ['Source/StemUI.cpp'])
        with self.assertRaises(ValueError):
            changed_sources(before, dict(after, **{'Source/StemUI.cpp': repaired.replace(b'startJob()', b'changeJob()')}))
        with self.assertRaises(ValueError):
            changed_sources(self.before, after)

    def test_existing_timeline_header_ui_is_classified_without_allowing_new_sources(self):
        before = dict(self.before, **{'Source/TimelineHeader.cpp': b'existing ruler UI'})
        after = dict(before, **{'Source/TimelineHeader.cpp': b'native marker descriptions/actions'})
        self.assertEqual(changed_sources(before, after), ['Source/TimelineHeader.cpp'])
        with self.assertRaises(ValueError):
            changed_sources(self.before, after)

    def test_changed_frozen_source_visual_or_bundle_rejected(self):
        for name in ('after.zip','visual.png','Mixworks.exe'):
            path=self.folder/name; before=path.read_bytes(); path.write_bytes(before+b'changed')
            with self.subTest(name=name), self.assertRaises(ValueError):
                validate_scoped_receipt(self.receipt,self.exe)
            path.write_bytes(before)

    def test_ui_behaviour_requires_matching_passing_suite(self):
        self.receipt['scope']='ui-behaviour'
        with self.assertRaisesRegex(ValueError,'requires affected'):
            validate_scoped_receipt(self.receipt,self.exe)
        suite=self.fixture.summary()['suites'][14]  # window
        self.assertEqual(suite['name'],'window')
        self.receipt['suites']=[suite]
        validate_scoped_receipt(self.receipt,self.exe)
        Path(suite['report']).write_text('FAIL fixture\n1 checks; 1 failures\n')
        with self.assertRaises(ValueError): validate_scoped_receipt(self.receipt,self.exe)

    def test_changed_baseline_cannot_be_relabelled_scoped(self):
        baseline=self.folder/'baseline.json'
        value=read_json(baseline); value['scope']='presentation'; write_json(baseline,value)
        self.receipt['baseline']['receipt']=evidence(baseline)
        with self.assertRaisesRegex(ValueError,'full regression baseline'):
            validate_scoped_receipt(self.receipt,self.exe)


if __name__ == '__main__': unittest.main()

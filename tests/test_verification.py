"""Harness failure/identity tests; these fixtures do not establish real-app behaviour."""
import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import wave

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from verification import (REGRESSION_SUITES, bundle_identity, check_report, create_receipt,
                          digest, plugin_identity, read_json, regression_module_arguments, run_checks, validate_receipt, validate_regressions, write_json)

spec = importlib.util.spec_from_file_location('real_benchmark', ROOT / 'scripts/run-real-benchmark.py')
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


class VerificationTests(unittest.TestCase):
    def setUp(self):
        (ROOT / '.local').mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / '.local', prefix='verification-unit-')
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.exe = self.folder / 'Mixworks.exe'
        for name in ('Mixworks.exe', 'onnxruntime.dll', 'beat-this-final0.onnx'):
            (self.folder / name).write_bytes(name.encode())

    def report(self, text, code=0, name='checks.txt'):
        path = self.folder / name
        path.write_text(text, encoding='utf-8')
        return check_report(path, code)

    def summary(self):
        plugins = []
        for role in ('gain', 'delay', 'latency', 'instrument'):
            path = self.folder / (role + '.vst3')
            if not path.exists():
                path.write_bytes(('fixture ' + role).encode())
            plugins.append(plugin_identity(path))
        summary = dict(format='mixworks-analysis-tests', schemaVersion=2,
                       **bundle_identity(self.exe), errors=[], suites=[], nativePluginEvidence=plugins)
        for name in REGRESSION_SUITES:
            flags = [] if name == 'core' else ['--neural-unavailable-only' if name in ('missing-model', 'corrupt-model') else '--' + name + '-only']
            if name not in ('source-streaming', 'missing-model', 'corrupt-model'):
                flags += regression_module_arguments(name, [plugin['path'] for plugin in plugins])
            summary['suites'].append(dict(name=name, invocation=dict(executableSha256=digest(self.exe), flags=flags, includeStress=True),
                artifactRetention=dict(errors=[], files=0, bytes=0),
                **self.report('PASS fixture\n1 checks; 0 failures\n', name=name + '.txt')))
        return summary

    def test_report_requires_final_consistent_nonempty_results(self):
        for text in ('', 'PASS one\n', '0 checks; 0 failures\n',
                     'PASS one\n2 checks; 0 failures\n',
                     'FAIL one\n1 checks; 0 failures\n',
                     'PASS one\n1 checks; 0 failures\ntrailing data\n'):
            with self.subTest(text=text):
                result = self.report(text)
                self.assertFalse(result['complete'])
                self.assertNotEqual(result['exit'], 0)

    def test_complete_receipt_requires_split_effects_branch(self):
        summary = self.summary()
        summary['suites'] = [s for s in summary['suites'] if s['name'] != 'split-effects']
        with self.assertRaisesRegex(ValueError, 'required named'):
            validate_regressions(summary, self.exe)

    def test_reduced_catalogue_child_cannot_stand_in_for_library_suite(self):
        summary = self.summary()
        library = next(s for s in summary['suites'] if s['name'] == 'library-assets')
        library['invocation']['flags'] += ['--catalogue-reopen', str(self.folder / 'child.json')]
        with self.assertRaisesRegex(ValueError, 'conditional|[Uu]nexpected|[Uu]nsupported'):
            validate_regressions(summary, self.exe)

    def test_split_effects_requires_finite_delay_fixture(self):
        summary = self.summary()
        split = next(s for s in summary['suites'] if s['name'] == 'split-effects')
        flags = split['invocation']['flags']
        self.assertEqual(flags[flags.index('--plugin') + 1], summary['nativePluginEvidence'][1]['path'])
        validate_regressions(summary, self.exe)
        flags[flags.index('--plugin') + 1] = summary['nativePluginEvidence'][0]['path']
        with self.assertRaisesRegex(ValueError, 'module invocation'):
            validate_regressions(summary, self.exe)

    def test_gate_module_roles_keep_core_instrument_and_split_echo_distinct(self):
        paths = ['gain-module', 'echo-module', 'latency-module', 'instrument-module']
        self.assertEqual(regression_module_arguments('split-effects', paths),
                         ['--plugin', 'echo-module', '--delay-plugin', 'echo-module', '--latency-plugin', 'latency-module'])
        self.assertEqual(regression_module_arguments('core', paths)[-2:], ['--instrument', 'instrument-module'])
        self.assertEqual(regression_module_arguments('source-streaming', paths), [])
        with self.assertRaises(ValueError):
            regression_module_arguments('not-a-suite', paths)

    def test_unknown_branch_options_cannot_change_named_gate_coverage(self):
        for name in ('core', 'library-assets', 'beat-grid', 'source-streaming'):
            summary = self.summary()
            next(s for s in summary['suites'] if s['name'] == name)['invocation']['flags'].append('--unreviewed-branch')
            with self.subTest(suite=name), self.assertRaisesRegex(ValueError, 'Unexpected conditional'):
                validate_regressions(summary, self.exe)

    def test_report_failure_and_process_disagreement_cannot_pass(self):
        self.assertEqual(self.report('FAIL one\n1 checks; 1 failures\n', 1)['exit'], 1)
        for text, code in [('FAIL one\n1 checks; 1 failures\n', 0), ('PASS one\n1 checks; 0 failures\n', 3)]:
            result = self.report(text, code)
            self.assertTrue(result['errors'])
            self.assertNotEqual(result['exit'], 0)

    def test_timeout_keeps_partial_evidence_and_start_failure_is_reported(self):
        def timeout(command, **kwargs):
            Path(command[-1]).write_text('PASS before timeout\n')
            raise subprocess.TimeoutExpired(command, 1)
        with patch('verification.subprocess.run', side_effect=timeout):
            result = run_checks(self.exe, [], self.folder / 'timeout', timeout=1)
        self.assertEqual(result['exit'], 124)
        self.assertIn('PASS before timeout', Path(result['report']).read_text())
        with patch('verification.subprocess.run', side_effect=OSError('denied')):
            result = run_checks(self.exe, [], self.folder / 'denied')
        self.assertNotEqual(result['exit'], 0)
        self.assertIn('denied', result['errors'][0])

    def test_catalog_rejects_missing_duplicate_renamed_and_reused_reports(self):
        original = self.summary()
        validate_regressions(original, self.exe)
        for mutate in (lambda s: s['suites'].pop(),
                       lambda s: s['suites'].append(s['suites'][0]),
                       lambda s: s['suites'][0].update(name='unknown'),
                       lambda s: s['suites'][0].update(invocation=[]),
                       lambda s: s['suites'][0].update(report=s['suites'][1]['report']),
                       lambda s: s['suites'][0]['invocation'].update(flags=['--tempo-only']),
                       lambda s: s['suites'][0]['invocation'].update(mode='interactive-bridge')):
            changed = copy.deepcopy(original)
            mutate(changed)
            with self.assertRaises(ValueError):
                validate_regressions(changed, self.exe)

    def test_routine_runs_lower_priority_and_require_explicit_stress_opt_in(self):
        def child(command, **kwargs):
            Path(command[-1]).write_text('PASS fixture\n1 checks; 0 failures\n')
            return types.SimpleNamespace(returncode=0)
        with patch.dict('os.environ', {'MIXWORKS_TEST_STRESS': '1'}):
            for enabled in (False, True):
                with patch('verification.subprocess.run', side_effect=child) as run:
                    result = run_checks(self.exe, ['--alloy-only'], self.folder / str(enabled), include_stress=enabled)
                self.assertEqual(result['exit'], 0)
                self.assertEqual(run.call_args.kwargs['env']['MIXWORKS_TEST_STRESS'], '1' if enabled else '0')
                self.assertEqual(run.call_args.kwargs['creationflags'], getattr(subprocess, 'BELOW_NORMAL_PRIORITY_CLASS', 0))
                self.assertEqual(result['invocation']['includeStress'], enabled)

    def test_routine_alloy_cannot_be_claimed_as_complete_regressions(self):
        summary = self.summary()
        alloy = next(s for s in summary['suites'] if s['name'] == 'alloy')
        alloy['invocation']['includeStress'] = False
        with self.assertRaisesRegex(ValueError, 'omits stress'):
            validate_regressions(summary, self.exe)
        alloy['invocation']['includeStress'] = True
        validate_regressions(summary, self.exe)

    def test_alloy_stress_requires_explicit_true_evidence(self):
        for state in ('absent', None, 0, 'true'):
            summary = self.summary()
            invocation = next(s for s in summary['suites'] if s['name'] == 'alloy')['invocation']
            if state == 'absent':
                invocation.pop('includeStress')
            else:
                invocation['includeStress'] = state
            with self.subTest(state=state), self.assertRaisesRegex(ValueError, 'stress'):
                validate_regressions(summary, self.exe)

    def test_required_native_modules_cannot_silently_skip(self):
        for name, flag in (('core', '--instrument'), ('core', '--plugin'), ('track-ownership', '--plugin'), ('export', '--delay-plugin'), ('latency', '--latency-plugin')):
            summary = self.summary()
            flags = next(s for s in summary['suites'] if s['name'] == name)['invocation']['flags']
            at = flags.index(flag); del flags[at:at+2]
            with self.subTest(name=name, flag=flag), self.assertRaisesRegex(ValueError, '[Pp]lugin|[Ii]nstrument|module'):
                validate_regressions(summary, self.exe)

    def test_native_module_evidence_is_required_and_bound_to_current_bytes(self):
        summary = self.summary()
        for evidence in (None, [], summary['nativePluginEvidence'][:-1]):
            changed = copy.deepcopy(summary); changed['nativePluginEvidence'] = evidence
            with self.subTest(evidence=evidence), self.assertRaises(ValueError):
                validate_regressions(changed, self.exe)
        module = Path(summary['nativePluginEvidence'][0]['path'])
        module.write_bytes(b'changed fixture')
        with self.assertRaisesRegex(ValueError, '[Pp]lugin|module'):
            validate_regressions(summary, self.exe)

    def test_retention_failure_cannot_be_reported_as_a_successful_run(self):
        def child(command, **kwargs):
            Path(command[-1]).write_text('PASS fixture\n1 checks; 0 failures\n')
            return types.SimpleNamespace(returncode=0)
        outcomes = [dict(errors=['fixture cleanup denied'], files=0, bytes=0),
                    OSError('fixture retention journal denied')]
        for index, outcome in enumerate(outcomes):
            with self.subTest(outcome=outcome), patch('verification.subprocess.run', side_effect=child), \
                    patch('verification.finish_run', **({'side_effect': outcome} if isinstance(outcome, Exception) else {'return_value': outcome})):
                result = run_checks(self.exe, [], self.folder / f'retention-{index}')
            self.assertEqual(result['processExit'], 0)
            self.assertTrue(result['complete'])
            self.assertNotEqual(result['exit'], 0)
            self.assertTrue(result['errors'])
            self.assertTrue(result['artifactRetention']['errors'])

    def test_receipt_rejects_recorded_retention_failure_even_with_native_pass(self):
        for retention in (dict(errors=['cleanup denied']), 'invalid', dict(errors=''), {}, None, 'absent'):
            summary = self.summary()
            if retention == 'absent':
                summary['suites'][0].pop('artifactRetention')
            else:
                summary['suites'][0]['artifactRetention'] = retention
            with self.subTest(retention=retention), self.assertRaisesRegex(ValueError, '[Rr]etention'):
                validate_regressions(summary, self.exe)

    def test_receipt_binds_reports_summary_and_all_bundle_assets(self):
        summary_path = self.folder / 'summary.json'
        write_json(summary_path, self.summary())
        receipt_path = self.folder / 'receipt.json'
        create_receipt(summary_path, self.exe, receipt_path)
        validate_receipt(receipt_path, self.exe)
        for name in ('Mixworks.exe', 'onnxruntime.dll', 'beat-this-final0.onnx', 'rhythm-reliability.txt', 'summary.json'):
            path = self.folder / name
            before = path.read_bytes()
            path.write_bytes(before + b'changed')
            with self.subTest(name=name), self.assertRaises(ValueError):
                validate_receipt(receipt_path, self.exe)
            path.write_bytes(before)
        with self.assertRaises(ValueError):
            create_receipt(summary_path, self.exe, receipt_path)

    def test_legacy_improvement_boolean_is_not_release_evidence(self):
        receipt = self.folder / 'legacy.json'
        write_json(receipt, dict(format='mixworks-release-verification', verifiedImprovement=True,
                                suites=[dict(name='core', exit=0)]))
        with self.assertRaises(ValueError):
            validate_receipt(receipt, self.exe)

    def benchmark(self, mode, count=1):
        sources = [str(self.folder / f'input-{i}.wav') for i in range(count)]
        for source in sources:
            Path(source).write_bytes(source.encode())
        paths = self.folder / 'paths.json'
        write_json(paths, sources)
        output = self.folder / ('output-' + mode)

        def child(command, **kwargs):
            report = Path(command[-1])
            if mode == 'timeout':
                report.write_text('PASS before timeout\n')
                raise subprocess.TimeoutExpired(command, 1)
            if mode == 'missing':
                return types.SimpleNamespace(returncode=0)
            selected = [command[command.index(flag) + 1] for flag in ('--track-a', '--track-b')]
            rows = []
            for index, source in enumerate(selected):
                wav = report.parent / f'source-{index}.wav'
                with wave.open(str(wav), 'wb') as stream:
                    stream.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
                    stream.writeframes(b'\0\0' * 80)
                rows.append(dict(file=source, sha256=digest(source), sampleRate=8000, decodedWav=wav.name))
            failed = mode in ('review', 'strict', 'dishonest', 'unclassified')
            readiness = [dict(name='fixture result', passed=False)] if mode == 'review' else []
            write_json(report.with_name('measurements-input.json'), dict(sources=rows, readinessChecks=readiness))
            report.write_text(('FAIL' if failed else 'PASS') + f' fixture result\n1 checks; {int(failed)} failures\n')
            return types.SimpleNamespace(returncode=int(failed and mode != 'dishonest'))

        with patch('verification.subprocess.run', side_effect=child):
            code = benchmark.main(['--exe', str(self.exe), '--paths', str(paths), '--output', str(output)])
        return code, read_json(output / 'manifest.json'), read_json(output / 'analyses.json') if (output / 'analyses.json').exists() else []

    def test_benchmark_counts_odd_padding_once_and_binds_sources(self):
        code, manifest, analyses = self.benchmark('ready', count=3)
        self.assertEqual(code, 0)
        self.assertEqual(manifest['tracks'], 3)
        self.assertEqual(len(analyses), 3)
        self.assertEqual(len(manifest['runs']), 2)
        self.assertTrue(manifest['runs'][-1]['paddedLastSource'])
        self.assertEqual(len(list((self.folder / 'output-ready').glob('*/*.f32'))), 4)

    def test_benchmark_distinguishes_readiness_from_failed_execution(self):
        for mode, expected in [('review', 2), ('strict', 1), ('dishonest', 1), ('missing', 1), ('timeout', 1), ('unclassified', 1)]:
            with self.subTest(mode=mode):
                code, manifest, _ = self.benchmark(mode)
                self.assertEqual(code, expected)
                self.assertEqual(manifest['exit'], expected)
                self.assertEqual(manifest['status'], 'review-required' if mode == 'review' else 'failed')


if __name__ == '__main__':
    unittest.main()

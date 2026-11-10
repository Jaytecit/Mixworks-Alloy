"""Client/protocol boundary tests; real native behaviour is covered by verify-bridge.py."""
import json
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from alloy_bridge import BridgeError, Client, freeze_bundle, initialise, validate_root
from verification import run_checks

spec = importlib.util.spec_from_file_location('bridge_verifier', ROOT/'scripts/verify-bridge.py')
bridge_verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge_verifier)


class BridgeTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'.local').mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT/'.local', prefix='bu-')
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root = initialise(self.base/'w')
        self.client = Client(self.root)

    def server(self, mutate=lambda r:r):
        def reply():
            until = time.monotonic()+3
            while time.monotonic()<until:
                requests = list((self.root/'control/requests').glob('*.json'))
                if requests:
                    try:
                        q = json.loads(requests[0].read_text())
                    except (OSError, json.JSONDecodeError):
                        # Windows can briefly deny a file immediately after its
                        # atomic publication. Poll the same read; never resend it.
                        time.sleep(.005)
                        continue
                    result = mutate(dict(id=q['id'], ok=True, state=dict(protocolVersion=1,
                                      root=str(self.root), busy=False)))
                    (self.root/f"control/responses/{q['id']}.json").write_text(json.dumps(result))
                    return
                time.sleep(.005)
        worker = threading.Thread(target=reply, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 4)

    def test_existing_root_is_never_adopted_or_overwritten(self):
        old = (self.root/'control/token.txt').read_bytes()
        with self.assertRaises(FileExistsError):
            initialise(self.root)
        self.assertEqual((self.root/'control/token.txt').read_bytes(), old)

    def test_outside_local_and_local_itself_rejected(self):
        for path in (ROOT, ROOT/'.local', ROOT/'.local/../Source', ROOT/'.local/archive/x'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                initialise(path)

    def test_changed_workspace_identity_rejected(self):
        (self.root/'workspace.json').write_text('{}')
        with self.assertRaises(BridgeError):
            validate_root(self.root)

    def test_verifier_rejects_unowned_output_before_running_native_code(self):
        outside = ROOT/f'blocked-{self.base.name}'
        with patch.object(sys, 'argv', ['verify-bridge.py', '--exe', str(self.bundle()), '--output', str(outside)]):
            with patch.object(bridge_verifier, 'run_checks') as run:
                with self.assertRaises(ValueError):
                    bridge_verifier.main()
                run.assert_not_called()
        self.assertFalse(outside.exists())

    def test_symlink_root_rejected(self):
        alias = self.base/'alias'
        try:
            os.symlink(self.root, alias, target_is_directory=True)
        except OSError as error:
            self.skipTest(str(error))
        with self.assertRaises(ValueError):
            validate_root(alias)

    def test_authenticated_command_and_redacted_trace(self):
        self.server()
        self.assertTrue(self.client.call('status')['ok'])
        trace = (self.root/'evidence/commands.jsonl').read_text()
        self.assertNotIn(self.client.token, trace)
        self.assertEqual(json.loads(trace)['command'], 'status')

    def test_timeout_does_not_retry_or_claim_failure_to_execute(self):
        with self.assertRaisesRegex(TimeoutError, 'uncertain'):
            self.client.call('stop', timeout=.05)
        self.assertEqual(len(list((self.root/'control/requests').glob('*.json'))), 1)

    def test_preview_command_roundtrips_without_protocol_override(self):
        self.server()
        self.assertTrue(self.client.call('preview', index=0, fraction=.5)['ok'])
        trace = (self.root/'evidence/commands.jsonl').read_text()
        self.assertEqual(json.loads(trace)['command'], 'preview')
        self.assertNotIn(self.client.token, trace)

    def test_work_mode_roundtrip(self):
        self.server()
        self.assertTrue(self.client.call('work_mode', name='record-perform')['ok'])
        trace = (self.root/'evidence/commands.jsonl').read_text()
        self.assertIn('work_mode', trace)
        self.assertNotIn(self.client.token, trace)

    def test_timeline_cue_roundtrip(self):
        self.server()
        self.assertTrue(self.client.call('timeline_cue', slot=0)['ok'])
        trace = (self.root/'evidence/commands.jsonl').read_text()
        self.assertIn('timeline_cue', trace)
        self.assertNotIn(self.client.token, trace)

    def test_transition_answer_roundtrip(self):
        self.server()
        self.assertTrue(self.client.call('transition_confirm', incoming='fixture-pair', replace=False)['ok'])
        trace = (self.root/'evidence/commands.jsonl').read_text()
        self.assertIn('transition_confirm', trace)
        self.assertNotIn(self.client.token, trace)

    def test_invalid_commands_timeouts_and_protocol_overrides_never_send(self):
        for command, kwargs in [('shell', {}), ('status', {'id':'x'}), ('status', {'timeout':-1}),
                                ('status', {'timeout':float('nan')})]:
            with self.subTest(command=command, kwargs=kwargs), self.assertRaises(BridgeError):
                self.client.call(command, **kwargs)
        self.assertFalse(list((self.root/'control/requests').glob('*.json')))

    def test_nonfinite_or_oversized_payload_never_sends(self):
        for value in (float('nan'), 'x'*65536):
            with self.assertRaises((BridgeError, ValueError)):
                self.client.call('control', value=value)
        self.assertFalse(list((self.root/'control/requests').glob('*.json')))

    def test_wrong_reply_id_rejected(self):
        self.server(lambda r:dict(r,id='another-request'))
        with self.assertRaisesRegex(BridgeError, 'ID mismatch'):
            self.client.call('status')

    def test_protocol_mismatch_rejected(self):
        self.server(lambda r:dict(r,state=dict(protocolVersion=99,root=str(self.root))))
        with self.assertRaisesRegex(BridgeError, 'protocol/workspace mismatch'):
            self.client.call('status')

    def test_root_mismatch_rejected(self):
        self.server(lambda r:dict(r,state=dict(protocolVersion=1,root=str(self.base))))
        with self.assertRaisesRegex(BridgeError, 'protocol/workspace mismatch'):
            self.client.call('status')

    def test_native_error_is_preserved(self):
        self.server(lambda r:dict(r,ok=False,error='Save first'))
        with self.assertRaisesRegex(BridgeError, 'Save first'):
            self.client.call('close')

    def bundle(self, supported=True):
        source = self.base/'candidate';source.mkdir()
        (source/'Mixworks.exe').write_bytes(b'--dev-control protocolVersion control/history' if supported else b'old GUI build')
        for name in ('onnxruntime.dll','beat-this-final0.onnx'):
            (source/name).write_bytes(name.encode())
        return source/'Mixworks.exe'

    def test_legacy_binary_cannot_be_launched_as_production_by_accident(self):
        with self.assertRaisesRegex(BridgeError, 'lacks the development bridge'):
            freeze_bundle(self.bundle(False), self.root)
        self.assertFalse((self.root/'bundle').exists())

    def test_freeze_reuses_exact_bundle_and_rejects_changed_candidate(self):
        source = self.bundle()
        frozen = freeze_bundle(source, self.root)
        self.assertEqual(freeze_bundle(source, self.root), frozen)
        self.assertGreater((frozen.parent/'onnxruntime.dll').stat().st_nlink, 1)
        before = frozen.read_bytes();source.write_bytes(before+b'changed')
        with self.assertRaisesRegex(BridgeError, 'different frozen bundle'):
            freeze_bundle(source, self.root)
        self.assertEqual(frozen.read_bytes(), before)

    def test_interactive_runner_uses_common_report_and_retention(self):
        class Driver:
            def run(self, exe, flags, folder, environment, log, startup, deadline):
                self_folder = Path(environment['TEMP'])
                (self_folder/'owned.wav').write_bytes(b'owned generated fixture')
                (folder/'checks.txt').write_text('PASS fixture driver\n1 checks; 0 failures\n')
                return 0
        result = run_checks(self.bundle(), ['--dev-control'], self.base/'run', driver=Driver())
        self.assertEqual(result['exit'], 0)
        self.assertEqual(result['invocation']['mode'], 'interactive-bridge')
        self.assertEqual(result['artifactRetention']['files'], 1)
        self.assertFalse((self.base/'run/.temporary/owned.wav').exists())


if __name__ == '__main__':
    unittest.main()

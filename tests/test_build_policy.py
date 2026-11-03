"""Exercise the build entry point with inert dependencies and tiny native children."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')


@unittest.skipUnless(os.name == 'nt' and POWERSHELL, 'Windows build entry point')
class BuildPolicyTests(unittest.TestCase):
    def exercise(self, failure=None, plugins=True, absent_environment=False):
        with tempfile.TemporaryDirectory(prefix='build-policy-') as temporary:
            root = Path(temporary)
            (root / 'scripts').mkdir()
            for dep in ('JUCE', 'soundtouch'):
                folder = root / '.local/deps' / dep
                folder.mkdir(parents=True)
                (folder / 'CMakeLists.txt').touch()
            shutil.copyfile(ROOT / 'scripts/build.ps1', root / 'scripts/build.ps1')
            (root / 'scripts/fetch-analysis.ps1').write_text('# inert fixture\n')
            (root / 'observe.py').write_text('''import ctypes, json, os, sys
from pathlib import Path
kernel = ctypes.windll.kernel32
kernel.GetCurrentProcess.restype = ctypes.c_void_p
kernel.GetPriorityClass.argtypes = [ctypes.c_void_p]
record = dict(args=sys.argv[1:], cl=os.environ.get('_CL_'),
    workers=os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL'),
    priority=kernel.GetPriorityClass(kernel.GetCurrentProcess()))
with Path('commands.jsonl').open('a') as output: output.write(json.dumps(record)+'\\n')
phase = 'build' if '--build' in sys.argv else 'configure'
raise SystemExit(1 if phase == os.environ.get('FAIL_PHASE') else 0)
''', encoding='utf-8')
            (root / 'driver.ps1').write_text('''$ErrorActionPreference = 'Stop'
function git {
    if (($args -join ' ') -match 'soundtouch') { 'd994965fbbcf0f6ceeed0e72516968130c2912f0' }
    else { 'ea77444750de45e036e0279bb0291144bf8e3688' }
    $global:LASTEXITCODE = 0
}
function cmake { & $env:PYTHON_BUILD_FIXTURE 'observe.py' @args }
# The fixture starts no real build. A normal-priority caller proves that the
# entry point lowers its children itself rather than relying on the test runner.
(Get-Process -Id $PID).PriorityClass = 'Normal'
$before = (Get-Process -Id $PID).PriorityClass.ToString()
$locationBefore = (Get-Location).Path
$caught = $null
try { & './scripts/build.ps1' -TestPlugin:($env:TEST_PLUGINS -eq '1') }
catch { $caught = $_.Exception.Message }
@{ cl=$env:_CL_; workers=$env:CMAKE_BUILD_PARALLEL_LEVEL;
   priorityBefore=$before; priorityAfter=(Get-Process -Id $PID).PriorityClass.ToString();
   locationBefore=$locationBefore; locationAfter=(Get-Location).Path; error=$caught } |
   ConvertTo-Json | Set-Content 'restored.json'
''', encoding='utf-8')
            environment = dict(os.environ, _CL_='/DRA_TEST', CMAKE_BUILD_PARALLEL_LEVEL='9',
                               PYTHON_BUILD_FIXTURE=sys.executable, FAIL_PHASE=failure or '',
                               TEST_PLUGINS='1' if plugins else '0')
            if absent_environment:
                environment.pop('_CL_')
                environment.pop('CMAKE_BUILD_PARALLEL_LEVEL')
            child = subprocess.run([POWERSHELL, '-NoProfile', '-NonInteractive', '-File', str(root / 'driver.ps1')],
                                   cwd=root, env=environment, capture_output=True, text=True, timeout=30,
                                   creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            self.assertEqual(child.returncode, 0, child.stdout + child.stderr)
            commands = [json.loads(line) for line in (root / 'commands.jsonl').read_text().splitlines()]
            restored = json.loads((root / 'restored.json').read_text(encoding='utf-8-sig'))
            self.assertIn(restored['cl'], (None, '') if absent_environment else ('/DRA_TEST',))
            self.assertIn(restored['workers'], (None, '') if absent_environment else ('9',))
            self.assertEqual(restored['priorityBefore'], 'Normal')
            self.assertEqual(restored['priorityBefore'], restored['priorityAfter'])
            self.assertEqual(restored['locationBefore'], restored['locationAfter'])
            self.assertEqual(len(commands), 1 if failure == 'configure' else 2)
            for command in commands:
                self.assertEqual(command['cl'].split()[-1], '/MP1')
                self.assertEqual(command['workers'], '1')
                self.assertEqual(command['priority'], 0x4000, command)
            if failure:
                self.assertIn('configuration failed' if failure == 'configure' else 'Build failed', restored['error'])
            else:
                self.assertIsNone(restored['error'])
                build = commands[-1]['args']
                self.assertEqual(build[build.index('--parallel') + 1], '1')
                for target in ('MixworksTestGain_VST3', 'MixworksTestDelay_VST3', 'MixworksTestLatency_VST3'):
                    self.assertEqual(target in build, plugins)

    def test_one_worker_and_child_policy_with_plugins(self):
        self.exercise()

    def test_plain_build_excludes_test_modules(self):
        self.exercise(plugins=False)

    def test_configuration_failure_restores_caller(self):
        self.exercise(failure='configure')

    def test_build_failure_restores_caller(self):
        self.exercise(failure='build')

    def test_initially_absent_environment_is_restored(self):
        self.exercise(absent_environment=True)


if __name__ == '__main__':
    unittest.main()

"""Own the suite catalog, complete check reports and regression release evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from test_artifacts import begin_run, finish_run

REGRESSION_SUITES = (
    'rhythm-reliability', 'beat-grid', 'placement', 'tempo', 'tempo-browser',
    'track-ownership', 'split-effects', 'analysis-refresh', 'grid-correction', 'rhythm-decoder', 'library-assets', 'playback-cache', 'source-streaming',
    'timeline-editing', 'alloy', 'window', 'fusion-workflows', 'mastering', 'export', 'latency', 'core',
    'missing-model', 'corrupt-model',
)
SUMMARY = re.compile(r'(\d+) checks; (\d+) failures')


def regression_module_arguments(suite, paths):
    """Bind fixture roles for the exact native branch, shared by run and receipt."""
    if suite not in REGRESSION_SUITES or len(paths) != 4:
        raise ValueError('A named regression suite and four module roles are required.')
    if suite in ('source-streaming', 'missing-model', 'corrupt-model'):
        return []
    # The split-only branch measures the finite echo, not the gain fixture used
    # by the ordinary ownership branch of the same C++ function.
    primary = paths[1] if suite == 'split-effects' else paths[0]
    flags = ['--plugin', str(primary), '--delay-plugin', str(paths[1]), '--latency-plugin', str(paths[2])]
    if suite == 'core':
        flags += ['--instrument', str(paths[3])]
    return flags


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()



def plugin_identity(path):
    path = path.resolve()
    files = [path] if path.is_file() else sorted(p for p in path.rglob('*') if p.is_file())
    if not files:
        raise ValueError(f'No native plugin files found: {path}')
    return dict(path=str(path), files={str(p.relative_to(path) if path.is_dir() else p.name): digest(p) for p in files})

def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path = Path(path)
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False), encoding='utf-8')
    temporary.replace(path)


def bundle_identity(exe):
    exe = Path(exe).resolve()
    return dict(executable=str(exe), executableSha256=digest(exe),
                modelSha256=digest(exe.with_name('beat-this-final0.onnx')),
                runtimeSha256=digest(exe.with_name('onnxruntime.dll')))


def check_report(path, process_exit, process_error=None):
    """A final footer and every individual result must agree; exit zero alone is insufficient."""
    path = Path(path).resolve()
    errors = [process_error] if process_error else []
    try:
        lines = [line for line in path.read_text(encoding='utf-8-sig').splitlines() if line]
        report_hash = digest(path)
    except (OSError, UnicodeError) as error:
        lines, report_hash = [], None
        errors.append(f'Cannot read check report: {error}')
    footer = SUMMARY.fullmatch(lines[-1]) if lines else None
    checks = int(footer[1]) if footer else 0
    failed = [line for line in lines if line.startswith('FAIL ')]
    results = [line for line in lines if line.startswith(('PASS ', 'FAIL '))]
    complete = bool(footer and checks > 0 and checks == len(results)
                    and int(footer[2]) == len(failed) and len(lines) == len(results) + 1)
    if not complete:
        errors.append('Incomplete or inconsistent check report; retain partial report and process.log.')
    if complete and process_exit != (1 if failed else 0):
        errors.append(f'Process exit {process_exit} disagrees with check results.')
    return dict(exit=process_exit if process_exit else int(bool(errors or failed)),
                processExit=process_exit, complete=complete, checks=checks,
                summary=lines[-1] if footer else 'Incomplete suite; no valid final summary',
                failures=failed, errors=errors, report=str(path), reportSha256=report_hash)


def run_checks(exe, flags, folder, *, timeout=900, cwd=None, retain_audio=False, driver=None, include_stress=False):
    """Run native checks with owned retention. An interactive driver implements
    run(exe, flags, folder, environment, log, startup, deadline) -> exit code,
    owns/reaps every process it creates, and writes the same checks.txt format.
    It cannot produce an ordinary regression receipt: its invocation is marked.
    """
    folder = Path(folder)
    folder.mkdir()
    workspace = Path(__file__).resolve().parents[1]
    owner = begin_run(folder, workspace) if not retain_audio and folder.resolve().is_relative_to((workspace / '.local').resolve()) else None
    temporary = folder / '.temporary'
    temporary.mkdir()
    environment = dict(os.environ, TEMP=str(temporary.resolve()), TMP=str(temporary.resolve()),
                       MIXWORKS_TEST_STRESS='1' if include_stress else '0')
    creationflags = getattr(subprocess, 'BELOW_NORMAL_PRIORITY_CLASS', 0)
    report = (folder / 'checks.txt').resolve()
    startup = None
    if hasattr(subprocess, 'STARTUPINFO'):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    process_error = None
    invocation = dict(executable=str(Path(exe).resolve()), flags=list(flags),
                      includeStress=include_stress)
    try:
        invocation['executableSha256'] = digest(exe)
        with (folder / 'process.log').open('w', encoding='utf-8') as log:
            if driver is None:
                process = subprocess.run([str(exe), '--self-test', *flags, '--report', str(report)],
                                         cwd=cwd, stdout=log, stderr=log, startupinfo=startup, timeout=timeout,
                                         env=environment, creationflags=creationflags)
                invocation['priority'] = 'below-normal' if creationflags else 'default'
                process_exit = process.returncode
            else:
                invocation['mode'] = 'interactive-bridge'
                process_exit = driver.run(exe, flags, folder, environment, log, startup, time.monotonic()+timeout)
    except subprocess.TimeoutExpired:
        process_exit, process_error = 124, f'Suite timed out after {timeout} seconds.'
    except (OSError, ValueError, RuntimeError) as error:
        process_exit, process_error = 1, f'Cannot start suite: {error}'
    result = dict(invocation=invocation, **check_report(report, process_exit, process_error))
    if owner:
        try:
            result['artifactRetention'] = finish_run(folder, owner)
        except (OSError, ValueError) as error:
            result['artifactRetention'] = dict(errors=[str(error)], bytes=0, files=0)
        if result['artifactRetention']['errors']:
            result['errors'].append('Generated-fixture retention failed; inspect artifactRetention and the retained journal.')
            result['exit'] = result['exit'] or 1
    return result


def validate_artifact_retention(suite):
    # Current accepted suites must be owned runs. Historical reports without
    # retention evidence remain historical rather than implying successful cleanup.
    retention = suite.get('artifactRetention')
    if (not isinstance(retention, dict) or not isinstance(retention.get('errors'), list)
            or retention['errors']):
        raise ValueError('Generated-fixture retention failed or its evidence is missing/malformed.')


def validate_regressions(summary, exe):
    if not isinstance(summary, dict) or summary.get('format') != 'mixworks-analysis-tests' or summary.get('schemaVersion') != 2:
        raise ValueError('A current consolidated regression summary is required.')
    identity = bundle_identity(exe)
    if any(summary.get(key) != value for key, value in identity.items() if key != 'executable'):
        raise ValueError('Verification bundle identity differs from the candidate.')
    if summary.get('errors'):
        raise ValueError('Regression run contains infrastructure/identity errors.')
    suites = summary.get('suites')
    if (not isinstance(suites, list) or any(not isinstance(s, dict) or not isinstance(s.get('name'), str) for s in suites)
            or sorted(s['name'] for s in suites) != sorted(REGRESSION_SUITES)):
        raise ValueError('Exactly one result for each required named regression suite is required.')
    if len({str(Path(s['report']).resolve()) for s in suites}) != len(suites):
        raise ValueError('Each named suite needs its own check report.')
    # verify-analysis records these roles in a fixed order. Bind the required
    # flags as well as their bytes: native tests intentionally omit some plugin
    # assertions when the optional CLI arguments are absent.
    modules = summary.get('nativePluginEvidence')
    if not isinstance(modules, list) or len(modules) != 4:
        raise ValueError('Four native plugin/module identities are required (gain, delay, latency, instrument).')
    paths = []
    for module in modules:
        if (not isinstance(module, dict) or not isinstance(module.get('path'), str)
                or not Path(module['path']).is_absolute()):
            raise ValueError('Invalid native plugin/module identity.')
        path = Path(module['path'])
        if plugin_identity(path) != module:
            raise ValueError('Native plugin/module bytes changed after verification.')
        paths.append(str(path))
    if len(set(paths)) != 4:
        raise ValueError('Native plugin/module roles need distinct modules.')
    for suite in suites:
        validate_artifact_retention(suite)
        invocation = suite.get('invocation', {})
        if not isinstance(invocation, dict) or not isinstance(invocation.get('flags'), list) or any(not isinstance(f, str) for f in invocation['flags']):
            raise ValueError(f"Invalid invocation evidence: {suite['name']}")
        if suite.get('name') == 'alloy' and invocation.get('includeStress') is not True:
            raise ValueError('Routine Alloy coverage omits stress checks; it cannot establish a complete regression receipt.')
        if invocation.get('mode') is not None:
            raise ValueError('Interactive evidence is supplemental, not a named native regression.')
        flag = '--neural-unavailable-only' if suite['name'] in ('missing-model', 'corrupt-model') else '--' + suite['name'] + '-only'
        flags = invocation.get('flags', [])
        module_flags = regression_module_arguments(suite['name'], paths)
        if module_flags:
            required = list(zip(module_flags[::2], module_flags[1::2]))
            for required_flag, module_path in required:
                if (flags.count(required_flag) != 1 or flags.index(required_flag) + 1 >= len(flags)
                        or flags[flags.index(required_flag) + 1] != module_path):
                    raise ValueError(f"Required native plugin/instrument module invocation missing or changed: {suite['name']} {required_flag}")
        allowed = set(module_flags[::2])
        if suite['name'] != 'core':
            allowed.add(flag)
        if suite['name'] == 'source-streaming':
            allowed.add('--capacity-fixture')
        unexpected = [value for value in flags if value.startswith('--') and value not in allowed]
        if unexpected:
            raise ValueError(f"Unexpected conditional/unsupported invocation flags for {suite['name']}: {unexpected}")
        selectors = [value for value in flags if value.endswith('-only')]
        if (invocation.get('executableSha256') != identity['executableSha256']
                or selectors != ([] if suite['name'] == 'core' else [flag])):
            raise ValueError(f"Invocation does not match named suite: {suite['name']}")
        if (type(suite.get('processExit')) is not int or suite.get('exit') != 0
                or suite.get('errors') or suite.get('failures') or suite.get('complete') is not True):
            raise ValueError(f"Suite did not pass completely: {suite.get('name')}")
        actual = check_report(suite['report'], suite['processExit'])
        for key in ('reportSha256', 'checks', 'summary', 'complete', 'failures', 'errors', 'exit'):
            if suite.get(key) != actual[key]:
                raise ValueError(f"Changed or inconsistent {suite['name']} evidence: {key}")
    return identity


def create_receipt(summary_path, exe, destination):
    destination = Path(destination)
    if destination.exists():
        raise ValueError('Keep the existing receipt; select a fresh output path.')
    summary = read_json(summary_path)
    identity = validate_regressions(summary, exe)
    receipt = dict(format='mixworks-release-verification', schemaVersion=2,
                   scope='complete-regressions', **identity,
                   regressionEvidence=dict(path=str(Path(summary_path).resolve()), sha256=digest(summary_path)))
    write_json(destination, receipt)
    return receipt


def validate_receipt(receipt_path, exe):
    receipt = read_json(receipt_path)
    if receipt.get('scope') in ('presentation', 'ui-behaviour'):
        from change_verification import validate_scoped_receipt
        return validate_scoped_receipt(receipt, exe)
    if (not isinstance(receipt, dict) or receipt.get('format') != 'mixworks-release-verification' or receipt.get('schemaVersion') != 2
            or receipt.get('scope') != 'complete-regressions'):
        raise ValueError('Legacy or unsupported receipt: run the consolidated verification first.')
    evidence = receipt['regressionEvidence']
    if not isinstance(evidence, dict):
        raise ValueError('Regression evidence must identify its summary and hash.')
    if digest(evidence['path']) != evidence['sha256']:
        raise ValueError('Regression summary changed after receipt creation.')
    identity = validate_regressions(read_json(evidence['path']), exe)
    if any(receipt.get(key) != value for key, value in identity.items() if key != 'executable'):
        raise ValueError('Receipt identity differs from the candidate.')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--exe', type=Path, required=True)
    args = parser.parse_args()
    try:
        validate_receipt(args.receipt, args.exe)
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(1, f'Verification rejected: {error}\n')
    print('Verification evidence accepted for this exact bundle.')


if __name__ == '__main__':
    main()

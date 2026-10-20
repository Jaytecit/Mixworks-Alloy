"""Run recovered/automatic rhythm comparisons on hash-verified development recordings.

Exit 2 means complete execution requiring independent musical review; 1 means
incomplete/invalid execution. Cross-model agreement never establishes beat-one truth.
"""
import argparse
import json
from pathlib import Path
import subprocess

from verification import bundle_identity, digest, read_json, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--paths', type=Path, required=True, help='JSON array of absolute audio paths')
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory')
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parents[1]
    try:
        if args.output.exists():
            raise ValueError('Choose a new output directory; existing evidence is preserved.')
        paths = read_json(args.paths)
        development = {r['sha256']: r for r in read_json(workspace / 'tests/musical-validation.json')['development']}
        if not isinstance(paths, list) or not paths or any(not isinstance(p, str) or not Path(p).is_absolute() for p in paths):
            raise ValueError('Paths must be a nonempty JSON array of absolute audio paths.')
        sources = []
        for path in paths:
            sha = digest(path)
            if sha not in development:
                raise ValueError(f'Source is not in the development split: {path}')
            if any(s['sha256'] == sha for s in sources):
                raise ValueError('Each source recording may appear only once.')
            sources.append(dict(path=path, sha256=sha, split='development'))
        exe, model = args.exe.resolve(), args.model.resolve()
        identity = bundle_identity(exe)
        model_hash = digest(model)
        args.output.mkdir(parents=True, exist_ok=False)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(str(error))
        return 1
    manifest = dict(format='alloy-real-recovered-assessment', version=1, **identity,
                    recoveredModel=str(model), recoveredModelSha256=model_hash,
                    pathsManifest=str(args.paths.resolve()), pathsManifestSha256=digest(args.paths),
                    scope='Development recordings only. Fresh native automatic control, recovered observations, and explicit pulse/bar admission. No independent musical annotations or listening claim.',
                    musicalAdmission=False, results=[], errors=[])
    destination = args.output / 'manifest.json'
    write_json(destination, manifest)
    startup = None
    if hasattr(subprocess, 'STARTUPINFO'):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    for i, source in enumerate(sources):
        probe = (args.output / f'{i:02d}.json').resolve()
        invocation = [str(exe), '--rhythm-research', source['path'], '--auftakt-model', str(model),
                      '--compare-automatic', '--report', str(probe)]
        row = dict(source=source, invocation=invocation, probe=str(probe), complete=False)
        try:
            with (args.output / f'{i:02d}.log').open('w', encoding='utf-8') as log:
                process = subprocess.run(invocation, cwd=workspace, stdout=log, stderr=log, startupinfo=startup, timeout=300)
            row['processExit'] = process.returncode
            data = read_json(probe)
            row['probeSha256'] = digest(probe)
            if (process.returncode != 0 or data.get('format') != 'alloy-rhythm-research' or data.get('version') != 4
                    or data.get('error') != '' or data.get('sourceSha256') != source['sha256']
                    or data.get('executableSha256') != identity['executableSha256'] or data.get('modelSha256') != model_hash
                    or digest(source['path']) != source['sha256']):
                raise ValueError('Failed native execution or source/model/executable identity mismatch.')
            row['automaticControl'] = data['automaticControl']
            row['assessment'] = data['recoveredAssessment']
            if row['assessment']['error']:
                raise ValueError(row['assessment']['error'])
            row['complete'] = True
            print(f"{Path(source['path']).name}: pulse {row['assessment']['pulseAdmission']}, bar {row['assessment']['barAdmission']}", flush=True)
        except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
            row['error'] = str(error)
            manifest['errors'].append(dict(source=source, error=str(error)))
        manifest['results'].append(row)
        write_json(destination, manifest)
    try:
        if bundle_identity(exe) != identity or digest(model) != model_hash:
            raise ValueError('Candidate bundle/model changed during verification.')
    except (OSError, ValueError) as error:
        manifest['errors'].append(dict(error=str(error)))
    manifest['exit'] = 1 if manifest['errors'] else 2
    manifest['status'] = 'incomplete' if manifest['errors'] else 'execution-complete-musical-review-required'
    write_json(destination, manifest)
    return manifest['exit']


if __name__ == '__main__':
    raise SystemExit(main())

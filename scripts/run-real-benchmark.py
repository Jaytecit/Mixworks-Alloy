"""Probe real recordings; exit 0 for ready, 2 for musical review, 1 for failed/incomplete execution."""
import argparse
from collections import Counter
import math
from pathlib import Path

import soundfile as sf

from verification import bundle_identity, digest, read_json, run_checks, write_json


def collect_sources(metadata, pair, folder, output_root, before):
    sources = metadata['sources']
    if not isinstance(sources, list) or len(sources) != 2:
        raise ValueError('Both probe sources must have measurement evidence.')
    rows = []
    for index, (source, requested) in enumerate(zip(sources, pair)):
        if Path(source['file']).resolve() != Path(requested).resolve():
            raise ValueError('Probe source identity differs from the requested recording.')
        if source['sha256'] != before[requested] or digest(requested) != before[requested]:
            raise ValueError('Original source changed or measured source hash differs.')
        if source['decodedWav'] != f'source-{index}.wav':
            raise ValueError('Unexpected decoded probe output path; outputs were preserved.')
        wav = folder / source['decodedWav']
        if wav.is_symlink():
            raise ValueError('Decoded probe output must be an owned regular file.')
        rate = source['sampleRate']
        if not isinstance(rate, (int, float)) or not math.isfinite(rate) or rate <= 0:
            raise ValueError('Invalid measured sample rate.')
        info = sf.info(wav)
        if info.samplerate != rate or info.frames <= 0:
            raise ValueError('Decoded probe dimensions disagree with measurements.')
        pcm, _ = sf.read(wav, frames=min(info.frames, int(rate * 30)), dtype='float32', always_2d=True)
        head = folder / (source['sha256'] + f'-{index}.f32')
        head.write_bytes(pcm[:, 0].tobytes())
        source['decodedHead'] = str(head.relative_to(output_root))
        source['decodedHeadSha256'] = digest(head)
        rows.append(source)
    return rows


def classify_run(result, metadata):
    # The C++ producer explicitly names readiness checks. Never guess from failure wording.
    readiness = metadata.get('readinessChecks', [])
    if not isinstance(readiness, list) or any(not isinstance(r, dict) or type(r.get('passed')) is not bool
                                            or not isinstance(r.get('name'), str) for r in readiness):
        raise ValueError('Invalid producer readiness classification.')
    review = Counter('FAIL ' + r['name'] for r in readiness if not r['passed'])
    failures = Counter(result['failures'])
    if review - failures:
        raise ValueError('Readiness evidence disagrees with the actual failed checks.')
    result['readinessFailures'] = list(review.elements())
    result['strictFailures'] = list((failures - review).elements())
    result['readinessChecks'] = readiness


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--paths', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--workflow', action='store_true')
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = dict(format='mixworks-real-benchmark', schemaVersion=2, runs=[], errors=[], complete=False)
    rows = {}
    manifest_file = args.output / 'manifest.json'
    try:
        identity = bundle_identity(args.exe)
        manifest.update(identity)
        manifest['sha256'] = identity['executableSha256']  # Existing comparison evidence consumes this field.
        requested = read_json(args.paths)
        if not isinstance(requested, list) or not requested or any(not isinstance(p, str) or not Path(p).is_absolute() for p in requested):
            raise ValueError('Select a nonempty JSON list of absolute audio paths.')
        paths = list(dict.fromkeys(requested))
        manifest['requestedPaths'] = requested
        manifest['expectedTracks'] = len(paths)
        before = {path: digest(path) for path in paths}
        manifest['sourceSha256'] = before
        for start in range(0, len(paths), 2):
            selected = paths[start:start + 2]
            pair = selected if len(selected) == 2 else selected + selected
            folder = args.output / f'{start // 2:02d}'
            flags = ['--real-tracks-only', '--workflow-only' if args.workflow else '--analysis-only',
                     '--track-a', pair[0], '--track-b', pair[1]]
            result = dict(pair=pair, selectedPaths=selected, paddedLastSource=len(selected) == 1,
                          **run_checks(args.exe.resolve(), flags, folder, timeout=600))
            result['strictFailures'] = list(result['failures'])
            result['readinessFailures'] = []
            try:
                metadata_file = folder / 'measurements-input.json'
                metadata = read_json(metadata_file)
                measured = collect_sources(metadata, pair, folder, args.output, before)
                classify_run(result, metadata)
                result['measurementEvidence'] = dict(path=str(metadata_file.resolve()), sha256=digest(metadata_file))
                for source in measured:
                    rows.setdefault(str(Path(source['file']).resolve()), source)
                if result['exit'] == 0 and not result['errors'] and not args.workflow:
                    for index in range(2):
                        (folder / f'source-{index}.wav').unlink()
            except (OSError, ValueError, KeyError, TypeError) as error:
                result['errors'].append(f'Measurement evidence incomplete: {error}')
            manifest['runs'].append(result)
            write_json(args.output / 'analyses.json', list(rows.values()))
            write_json(manifest_file, manifest)
            print(f"{len(rows)}/{len(paths)} unique tracks; {result['summary']}; child exit {result['processExit']}", flush=True)
        if bundle_identity(args.exe) != identity:
            raise ValueError('Candidate bundle changed during the benchmark.')
        manifest['complete'] = (len(rows) == len(paths) and all(r['complete'] and not r['errors'] for r in manifest['runs']))
    except (OSError, ValueError, KeyError, TypeError) as error:
        manifest['errors'].append(str(error))
    manifest['tracks'] = len(rows)
    failed = bool(manifest['errors'] or not manifest['complete'] or any(r['errors'] or r['strictFailures'] for r in manifest['runs']))
    review = any(r['readinessFailures'] for r in manifest['runs'])
    manifest['status'] = 'failed' if failed else 'review-required' if review else 'ready'
    manifest['exit'] = 1 if failed else 2 if review else 0
    write_json(manifest_file, manifest)
    print(f"Benchmark {manifest['status']}; exit {manifest['exit']}", flush=True)
    return manifest['exit']


if __name__ == '__main__':
    raise SystemExit(main())

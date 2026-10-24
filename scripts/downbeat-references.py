"""Keep existing manual choices and export local downbeat comparisons; never modify analysis/audio."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
from datetime import datetime, timezone


def valid(doc, reference=False):
    if not isinstance(doc, dict) or doc.get('version') != 1:
        return False
    if doc.get('format') != ('mixworks-downbeat-reference' if reference else 'mixworks-track-analysis'):
        return False
    source, automatic, manual = (doc.get(k) for k in ('source', 'analysis', 'manual'))
    if not all(isinstance(x, dict) for x in (source, automatic, manual)):
        return False
    digest = source.get('sha256', '')
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        return False
    def numeric(v):
        return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)
    if not all(numeric(source.get(k)) and source[k] > 0 for k in ('sampleRate', 'frames', 'bytes', 'channels')):
        return False
    duration = source['frames'] / source['sampleRate']
    if not all(numeric(d.get(k)) for d, k in ((automatic, 'bpm'), (automatic, 'downbeatOrigin'), (manual, 'bpm'), (manual, 'beatOrigin'))):
        return False
    return all(v == -1 or 0 <= v < duration for v in (automatic['downbeatOrigin'], manual['beatOrigin']))


def documents(folder, reference=False):
    for path in sorted(folder.glob('*.json')):
        try:
            if path.is_symlink() or not path.is_file() or path.stat().st_size > 1024 * 1024:
                continue
            raw = path.read_bytes()
            doc = json.loads(raw)
            if valid(doc, reference):
                yield path, raw, doc
        except (OSError, ValueError, TypeError):
            continue


def capture(analysis, output, include_uncorrected=False):
    output.mkdir(parents=True, exist_ok=True)
    if output.is_symlink():
        raise ValueError('Reference output cannot be a symbolic link')
    count = 0
    for _, raw, doc in documents(analysis):
        if doc['manual']['beatOrigin'] < 0 and not include_uncorrected:
            continue
        digest = hashlib.sha256(raw).hexdigest()
        target = output / (doc['source']['sha256'] + '-existing-' + digest + '.json')
        if target.exists():
            continue
        doc = {k: v for k, v in doc.items() if k not in ('peaks', 'peaksF32', 'waveformRms', 'waveformRmsF32')}
        doc.update(format='mixworks-downbeat-reference', version=1,
                   kind='automatic-baseline-snapshot' if doc['manual']['beatOrigin'] < 0 else 'existing-correction-snapshot',
                   recordedUtc=datetime.now(timezone.utc).isoformat(), originalRecordSha256=digest)
        # The capture time is not the unknown original edit time.
        with target.open('x', encoding='utf-8') as stream:
            json.dump(doc, stream, indent=2, ensure_ascii=False, allow_nan=False)
        count += 1
    return count


def export(folder, output):
    rows = list(documents(folder, True))
    with output.open('x', encoding='utf-8-sig', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(('track', 'audio_sha256', 'recorded_utc', 'kind', 'detector_revision', 'automatic_bpm',
                         'automatic_downbeat_seconds', 'automatic_downbeat_method', 'downbeat_confidence',
                         'user_bpm_override', 'user_downbeat_seconds', 'previous_user_downbeat_seconds', 'source_path', 'reference_file'))
        def cell(value):
            # Literal spreadsheet text, including filenames beginning with formula characters.
            return "'" + value if isinstance(value, str) and value.startswith(('=', '+', '-', '@', '\t', '\r')) else value
        for path, _, doc in rows:
            a, m = doc['analysis'], doc['manual']
            source = doc.get('sourcePath', '')
            writer.writerow(tuple(cell(x) for x in (Path(source).name, doc['source']['sha256'], doc.get('recordedUtc', ''),
                doc.get('kind', ''), doc.get('analysisRevision', ''), a['bpm'], a['downbeatOrigin'], a.get('downbeatMethod', ''),
                a.get('downbeatConfidence', ''), m['bpm'], m['beatOrigin'], doc.get('previousManualAnchor', ''), source, str(path))))
    return len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis-dir', type=Path, help='Optionally snapshot existing corrected analysis records (read-only input)')
    parser.add_argument('--include-uncorrected', action='store_true', help='Also preserve current automatic detection as baseline evidence')
    parser.add_argument('--reference-dir', required=True, type=Path)
    parser.add_argument('--csv', type=Path, help='New CSV comparison file; existing files are never overwritten')
    args = parser.parse_args()
    if args.analysis_dir:
        print('Captured', capture(args.analysis_dir, args.reference_dir, args.include_uncorrected), 'existing analysis observations')
    if args.csv:
        print('Exported', export(args.reference_dir, args.csv), 'review observations')


if __name__ == '__main__':
    main()

"""Compare an immutable DJ.Studio snapshot with fresh Mixworks real-track probes.

Reads local measurements only. Never changes the DJ.Studio database or audio.
The snapshot folder contains djstudio-records-before.json, database-before,
djstudio-decoder-identity.json, mixworks-fresh.json, and the two PCM-head folders.
DJ.Studio labels are comparison evidence, not human-annotated ground truth.
"""
import argparse
import csv
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

import numpy as np
from scipy.signal import correlate, resample_poly

DJ_CAMELOT = ['8B', '3B', '10B', '5B', '12B', '7B', '2B', '9B', '4B', '11B',
              '6B', '1B', '8A', '3A', '10A', '5A', '12A', '7A', '2A', '9A',
              '4A', '11A', '6A', '1A']


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def camelot(value):
    return DJ_CAMELOT[value] if isinstance(value, int) and 0 <= value < 24 else '?'


def key_relation(first, second):
    if '?' in (first, second):
        return 'unknown'
    if first == second:
        return 'same'
    a, b = DJ_CAMELOT.index(first), DJ_CAMELOT.index(second)
    tonic_a = a if a < 12 else (a - 12 + 9) % 12
    tonic_b = b if b < 12 else (b - 12 + 9) % 12
    if tonic_a == tonic_b:
        return 'parallel major/minor'
    if first[:-1] == second[:-1]:
        return 'relative major/minor'
    return 'different tonic'


def decoder_offset(root, source, decoded, analysis_root=None):
    """Positive lag means Mixworks audio starts later than DJ.Studio audio."""
    head = root / 'mw-pcm-heads' / (source['sha256'] + '.f32')
    if analysis_root is not None and source.get('decodedHead'):
        relative = Path(source['decodedHead'])
        if relative.is_absolute():
            raise ValueError('Candidate decoder head must belong to its probe directory')
        head = (analysis_root / relative).resolve()
        head.relative_to(analysis_root.resolve())
        if hashlib.sha256(head.read_bytes()).hexdigest() != source['decodedHeadSha256']:
            raise ValueError('Candidate decoder head identity changed')
    mw = np.fromfile(head, dtype='<f4')
    dj = np.fromfile(root / 'dj-pcm-heads' / (source['sha256'] + '.s16'), dtype='<i2')
    dj = dj.reshape(-1, decoded['fmt']['channels'])[:, 0].astype(np.float32) / 32768
    rate = int(source['sampleRate'])
    if rate != 44100:
        divisor = math.gcd(rate, 44100)
        mw = resample_poly(mw, 44100 // divisor, rate // divisor)
    count = min(len(mw), len(dj))
    mw, dj = mw[:count], dj[:count]
    mw -= mw.mean()
    dj -= dj.mean()
    correlation = correlate(mw, dj, mode='full', method='fft')
    radius = min(8820, count - 1)
    centre = count - 1
    lag = int(np.argmax(correlation[centre - radius:centre + radius + 1])) - radius
    x, y = (mw[lag:], dj[:count - lag]) if lag >= 0 else (mw[:count + lag], dj[-lag:])
    denominator = float(np.linalg.norm(x) * np.linalg.norm(y))
    coefficient = float(np.dot(x, y)) / denominator if denominator else 0
    return lag / 44100, coefficient


def compare(root, analyses=None):
    labels = read(Path(__file__).resolve().parents[1] / 'tests/real-music-comparison.json')
    by_digest = {row['sha256']: row for row in labels['tracks']}
    originals = read(root / 'djstudio-records-before.json')
    fresh = {r['file'].lower(): r for r in read(analyses or root / 'mixworks-fresh.json')}
    decodes = {r['file'].lower(): r for r in read(root / 'djstudio-decoder-identity.json')}
    structures = {}
    for path in (root / 'database-before/track-structures-table').rglob('*'):
        if path.is_file():
            value = read(path)
            structures[value['key']] = value
    rows = []
    for original in originals:
        file = original['file']
        if file.lower() not in fresh:
            continue
        source = fresh[file.lower()]
        decoded = decodes[file.lower()]
        if not (source['recomputedForTest'] and decoded['djHashMatches']
                and source['sha256'] == original['sha256'] == decoded['sourceSha256']):
            raise ValueError(f'Unverified audio identity or cached analysis: {file}')
        if hashlib.sha256(Path(file).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError(f'Audio changed during the benchmark: {file}')
        record = original['record']
        declared = by_digest[source['sha256']]['references']
        if (declared['djStudioBpm'] != record['bpm'] or declared['djStudioKey'] != camelot(record['noteKey'])
                or declared['storedMixedInKey'] != camelot(record['mikKey'])):
            raise ValueError('The saved comparison manifest no longer matches its immutable reference snapshot')
        offset, coefficient = decoder_offset(root, source, decoded, analyses.parent if analyses else None)
        selected = structures[record['structureKey']]['beatData']
        grid = next(g for g in record['beatGrids'] if g['beatDataSource'] == record['beatDataSource'])
        raw = structures[grid['trackStructureKey']]['beatData']
        row = dict(file=file, sha256=source['sha256'], genre=record['tag'].get('genre', ''),
                   durationSeconds=source['duration'], mwBpm=source['bpm'], djBpm=record['bpm'],
                   bpmDifference=source['bpm'] - record['bpm'],
                   mwKey=source['camelot'], djKey=camelot(record['noteKey']),
                   storedMikKey=camelot(record['mikKey']), keyConfidence=source['keyConfidence'],
                   mwAlternativeKey=source['alternativeKey'],
                   variableTempo=source['variableTempo'], tempoConfidence=source['tempoConfidence'],
                   downbeatSeconds=source['downbeatOrigin'],
                   downbeatConfidence=source['downbeatConfidence'], djGrid=record['beatDataSource'],
                   djGridMadeOn=grid.get('madeOn'), djRecordSha256=original['recordSha256'],
                   decoderOffsetMs=offset * 1000, decoderCorrelation=coefficient,
                   selectedGridEqualsSavedGrid=[(b['nr'], b['time']) for b in selected]
                       == [(b['nr'], b['time']) for b in raw])
        row['keyRelationDJ'] = key_relation(row['mwKey'], row['djKey'])
        row['keyRelationMIK'] = key_relation(row['mwKey'], row['storedMikKey'])
        times = np.array([b['time'] + offset for b in selected if b['nr'] % 4 == 0
                          and 5 <= b['time'] <= decoded['frames'] / 44100 - 5])
        if coefficient < .95:
            row['barComparison'] = 'decoder alignment unverified'
        elif source['downbeatOrigin'] < 0:
            row['barComparison'] = 'Mixworks downbeat unknown'
        elif not len(times):
            row['barComparison'] = 'no comparable DJ.Studio bars'
        else:
            period = 60 / source['bpm']
            bars = (times - source['downbeatOrigin']) / (period * 4)
            errors = np.abs(bars - np.round(bars)) * period * 4 * 1000
            row['barErrorMedianMs'] = float(np.median(errors))
            row['barErrorP95Ms'] = float(np.percentile(errors, 95))
            row['barErrorMaxMs'] = float(np.max(errors))
            row['barsCompared'] = len(times)
            row['barComparison'] = 'agrees within 50 ms at p95' if row['barErrorP95Ms'] <= 50 else 'disagrees'
            row['barPhaseDifferenceBeats'] = float(np.median((bars - np.round(bars)) * 4))
        rows.append(row)
    summary = dict(tracks=len(rows), expectedTracks=len(originals), complete=len(rows) == len(originals),
                   audioIdentityVerified=len(rows), freshAnalysis=len(rows),
                   bpmWithinPoint1=sum(abs(r['bpmDifference']) <= .1 for r in rows),
                   keyAgreementDJ=sum(r['mwKey'] == r['djKey'] for r in rows),
                   keyAgreementStoredMIK=sum(r['mwKey'] == r['storedMikKey'] for r in rows),
                   djAndStoredMIKAgree=sum(r['djKey'] == r['storedMikKey'] for r in rows),
                   keyUnknown=sum(r['mwKey'] == '?' for r in rows),
                   downbeatUnknown=sum(r['downbeatSeconds'] < 0 for r in rows),
                   variableTempo=sum(r['variableTempo'] for r in rows),
                   decoderVerified=sum(r['decoderCorrelation'] >= .95 for r in rows),
                   bars=dict(Counter(r['barComparison'] for r in rows)),
                   keyRelationsDJ=dict(Counter(r['keyRelationDJ'] for r in rows)),
                   genres=dict(Counter(r['genre'] for r in rows)))
    baseline = dict(tracks=len(rows),
                    bpmWithinPoint1=sum(abs(by_digest[r['sha256']]['baseline']['bpm'] - r['djBpm']) <= .1 for r in rows),
                    keyAgreementDJ=sum(by_digest[r['sha256']]['baseline']['key'] == r['djKey'] for r in rows),
                    keyAgreementStoredMIK=sum(by_digest[r['sha256']]['baseline']['key'] == r['storedMikKey'] for r in rows),
                    downbeatUnknown=sum(by_digest[r['sha256']]['baseline']['downbeat'] < 0 for r in rows),
                    bars=dict(Counter(by_digest[r['sha256']]['baseline']['barComparison'] for r in rows)))
    summary['largePhaseDisagreements'] = sum(r.get('barErrorMedianMs', 0) >= 30000 / r['mwBpm'] for r in rows if r['mwBpm'] > 0)
    delta = {name: summary[name] - baseline[name] for name in ['bpmWithinPoint1', 'keyAgreementDJ', 'keyAgreementStoredMIK', 'downbeatUnknown']}
    return dict(summary=summary, baseline=baseline, change=delta, referencePurpose=labels['purpose'], tracks=rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--analyses', type=Path, help='Fresh candidate analyses; the saved baseline is never overwritten.')
    args = parser.parse_args()
    result = compare(args.snapshot, args.analyses)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    if result['tracks']:
        fields = list(dict.fromkeys(k for row in result['tracks'] for k in row))
        with args.output.with_suffix('.csv').open('w', newline='', encoding='utf-8-sig') as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerows(result['tracks'])
    print(json.dumps(result['summary'], indent=2))

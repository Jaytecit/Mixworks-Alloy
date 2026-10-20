"""Score frozen rhythm probes against independently constructed, hash-bound WAV fixtures."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def score(expected, observed, tolerance):
    for events in (expected, observed):
        previous = -1
        for event in events:
            time = event['seconds']
            if type(time) not in (int, float) or not math.isfinite(time) or time <= previous:
                raise ValueError('Events must have finite, strictly increasing source times.')
            if type(event['meter']) is not int or type(event['position']) is not int or not 1 <= event['position'] <= event['meter'] <= 10:
                raise ValueError('Invalid beat position or meter.')
            previous = time
    i = j = bars = 0
    deviations = []
    while i < len(expected) and j < len(observed):
        a, b = expected[i], observed[j]
        difference = b['seconds'] - a['seconds']
        if abs(difference) <= tolerance:
            deviations.append(abs(difference))
            bars += a['position'] == b['position'] and a['meter'] == b['meter']
            i += 1
            j += 1
        elif difference < 0:
            j += 1
        else:
            i += 1
    matched = len(deviations)
    return dict(expected=len(expected), observed=len(observed), matched=matched,
                precision=matched / len(observed) if observed else 0,
                recall=matched / len(expected) if expected else 0,
                f1=2 * matched / (len(expected) + len(observed)) if expected or observed else 0,
                matchedBarPositionsCorrect=bars, matchedBarPositionAccuracy=bars / matched if matched else 0,
                maximumMatchedErrorMs=max(deviations) * 1000 if deviations else None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--probes', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tolerance-ms', type=float, default=70)
    args = parser.parse_args()
    if args.output.exists() or not math.isfinite(args.tolerance_ms) or args.tolerance_ms <= 0:
        parser.error('Choose a new output and a positive finite tolerance.')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    result = dict(format='alloy-engine-construction-comparison', version=1, manifestSha256=sha(args.manifest),
                  toleranceMs=args.tolerance_ms, scope='Construction timestamps only; no commercial agreement or perceptual accuracy claim.',
                  fixtures=[], errors=[])
    for fixture in manifest['fixtures']:
        if 'beats' not in fixture['construction']:
            continue
        try:
            source = args.manifest.parent / fixture['file']
            probe = args.probes / (Path(fixture['file']).stem + '.json')
            data = json.loads(probe.read_text(encoding='utf-8-sig'))
            if (sha(source) != fixture['sha256'] or data['sourceSha256'] != fixture['sha256']
                    or data['format'] != 'alloy-rhythm-research' or data['error']):
                raise ValueError('Source/probe identity mismatch or incomplete analysis.')
            measured = score(fixture['construction']['beats'], data['beats'], args.tolerance_ms / 1000)
            measured.update(file=fixture['file'], sourceSha256=fixture['sha256'], probeSha256=sha(probe),
                            executableSha256=data['executableSha256'], modelSha256=data['modelSha256'])
            result['fixtures'].append(measured)
        except (OSError, ValueError, KeyError, TypeError) as error:
            result['errors'].append(dict(file=fixture['file'], error=str(error)))
    if not result['fixtures']:
        result['errors'].append(dict(error='No complete rhythm fixtures were examined.'))
    review = any(r['matched'] != r['expected'] or r['matched'] != r['observed']
                 or r['matchedBarPositionsCorrect'] != r['expected'] for r in result['fixtures'])
    result['exit'] = 1 if result['errors'] else 2 if review else 0
    result['status'] = 'incomplete' if result['errors'] else 'construction-review-required' if review else 'constructed-events-match'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False), encoding='utf-8')
    print(json.dumps(result, indent=2))
    return result['exit']


if __name__ == '__main__':
    raise SystemExit(main())

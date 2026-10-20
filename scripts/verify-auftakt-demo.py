"""Compare a native rhythm probe with retained, identity-bound actual-demo records."""
import argparse
import hashlib
import importlib
import json
import math
import struct
from pathlib import Path

score = importlib.import_module('score-engine-probes').score


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tolerance-ms', type=float, required=True)
    args = parser.parse_args()
    if args.output.exists() or not math.isfinite(args.tolerance_ms) or args.tolerance_ms <= 0:
        parser.error('Choose a new output and a positive finite tolerance.')
    probe = json.loads(args.probe.read_text(encoding='utf-8-sig'))
    reference = json.loads(args.reference.read_text(encoding='utf-8-sig'))
    if (probe['format'] != 'alloy-rhythm-research' or probe['error']
            or probe['observationEngine'] != 'auftakt-4.2.2-recovered'
            or probe['sourceSha256'] != reference['sourceSha256']
            or digest(probe['source']) != probe['sourceSha256']
            or digest(reference['source']) != reference['sourceSha256']
            or reference['imageSha256'] != '0624710ff4805170c98558d3410966aafffeb1392489e728d488792ec3724340'
            or reference['readOnly'] is not True or len(reference['owners']) != 1):
        raise ValueError('Incomplete probe or mismatched source/demo identity.')
    owner = reference['owners'][0]
    if (owner['guiAgreesWithPositions'] is not True
            or owner['guiCount'] != len(owner['events'])
            or len(owner['guiDownbeats']) != len(owner['events'])
            or any(flag != (event['position'] == 1)
                   for flag, event in zip(owner['guiDownbeats'], owner['events']))):
        raise ValueError('Reference result and GUI vectors disagree.')
    measured = score(owner['events'], probe['beats'], args.tolerance_ms / 1000)
    records = b''.join(struct.pack('<fii', event['seconds'], event['position'], event['meter'])
                       for event in owner['events'])
    if hashlib.sha256(records).hexdigest() != owner['recordSha256']:
        raise ValueError('Reference event bytes do not match the captured record digest.')
    passed = (measured['matched'] == measured['expected'] == measured['observed']
              and measured['matchedBarPositionsCorrect'] == measured['expected'])
    result = dict(scope='Retained actual-demo agreement only; not independent musical accuracy.',
                  sourceSha256=probe['sourceSha256'], executableSha256=probe['executableSha256'],
                  modelSha256=probe['modelSha256'], vendorExecutableSha256=reference['imageSha256'],
                  probeSha256=digest(args.probe), referenceSha256=digest(args.reference),
                  toleranceMs=args.tolerance_ms, measured=measured, passed=passed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False), encoding='utf-8')
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())

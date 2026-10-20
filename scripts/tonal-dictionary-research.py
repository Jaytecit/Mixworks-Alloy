"""Experimental reconstruction of TONART's chroma classifier, not its PCM feature extractor."""
import argparse
import json
import math
from pathlib import Path

# Read from the supplied 3.1.6 PE, not fitted to recordings or comparison labels.
# Factory: ev_7397b89da8d76231d23b1b0c3c0d71bf6e107082cede0d8a560864123b36fc7d.
# General profiles: VA 0x140408b80 / 0x140408bb0.
# Electronic profiles: VA 0x140408ca0 / 0x140408cd0.
GENERAL = (
    (6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88),
    (6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17),
)
ELECTRONIC = (
    (6.35, 0, 3.48, 0, 4.38, 4.09, 0, 5.19, 0, 3.66, 0, 2.88),
    (6.33, 0, 3.52, 5.38, 0, 3.53, 0, 4.75, 3.98, 0, 3.34, 3.17),
)
NOTES = ('C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B')


def normalize(values):
    scale = max(values, default=0)
    scaled = [x / scale for x in values] if scale else [0.0] * len(values)
    total = math.fsum(scaled)
    return [x / total for x in scaled] if total else scaled


def profiles(mode):
    base = dict(general=GENERAL, electronic=ELECTRONIC)[mode]
    # The scalar harmonic weights and add-to-original convolution are observed.
    # Mapping the referenced interval objects to 0, 7, 4 is a musical inference;
    # their static constructors have not yet been verified.
    return [normalize([2.75 * p[n] + .5 * p[(n - 7) % 12] + .2 * p[(n - 4) % 12]
                       for n in range(12)]) for p in base]


def rank_chroma(chroma, mode='electronic'):
    if not isinstance(chroma, list) or len(chroma) != 12 or any(type(x) not in (int, float) for x in chroma):
        raise ValueError('Supply exactly 12 finite nonnegative chroma values, C through B.')
    try:
        chroma = [float(x) for x in chroma]
    except OverflowError as error:
        raise ValueError('Chroma values exceed the floating-point representation.') from error
    if any(not math.isfinite(x) or x < 0 for x in chroma):
        raise ValueError('Supply exactly 12 finite nonnegative chroma values, C through B.')
    if mode not in ('general', 'electronic'):
        raise ValueError('Unknown analysis mode.')
    if not any(chroma):
        return []
    c = normalize(chroma)
    templates = profiles(mode)
    raw = []
    # Verified subtraction, dot product, sqrt and chord-tone formula:
    # ev_ffe66278ca482f40be94ed94e7c7a790242d366c65490d9c771f708a21a1faa9.
    for key in range(24):
        root, minor = key % 12, key // 12
        template = templates[minor]
        distance = math.sqrt(math.fsum((c[(n + root) % 12] - template[n]) ** 2 for n in range(12)))
        raw.append((1 - distance) ** 2 + .75 * c[root] + .5 * c[(root + (3 if minor else 4)) % 12] + .5 * c[(root + 7) % 12])
    smoothed = []
    for key in range(24):
        root, minor = key % 12, key // 12
        relative = (root + 3) % 12 if minor else 12 + (root + 9) % 12
        # Weights 1/.1/.1/.05 and L1 normalization are observed. Fifth/relative
        # interval mappings remain inferred, independently of the recovered math.
        smoothed.append((raw[key] + .1 * raw[minor * 12 + (root + 7) % 12]
                         + .1 * raw[minor * 12 + (root - 7) % 12] + .05 * raw[relative]) / 1.25)
    minimum = min(smoothed)
    # Three in-place squarings are observed, followed by L1 normalization:
    # ev_289e3be08492114b4bbe2d90ff4e26ec4889fd9204abd5e5481f8101bdb27daf.
    support = normalize([(score - minimum) ** 8 for score in smoothed])
    return [dict(key=key, label=f'{NOTES[key % 12]}:{"min" if key >= 12 else "maj"}',
                 rawScore=raw[key], relatedScore=smoothed[key], relativeSupport=support[key])
            for key in sorted(range(24), key=lambda k: (-support[k], k))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--chroma', type=Path, required=True, help='JSON with a 12-element chroma array and input provenance.')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output path; research evidence is never overwritten.')
    supplied = json.loads(args.chroma.read_text(encoding='utf-8-sig'))
    result = dict(format='alloy-tonal-dictionary-research', version=1, input=supplied,
                  scope='Partial classifier reconstruction; interval mapping inferred, PCM feature extraction and tuning not reproduced. Relative support is not accuracy.',
                  modes={mode: rank_chroma(supplied['chroma'], mode) for mode in ('general', 'electronic')})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False), encoding='utf-8')


if __name__ == '__main__':
    main()

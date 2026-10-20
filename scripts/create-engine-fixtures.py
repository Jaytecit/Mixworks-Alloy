"""Create deterministic audio and construction truth for engine reconstruction comparisons."""
import argparse
import hashlib
import json
from pathlib import Path
import wave

import numpy as np

RATE = 44100


def write_wave(folder, name, pcm, truth):
    pcm = np.asarray(pcm)
    if pcm.ndim == 1:
        pcm = np.column_stack((pcm, pcm))
    if not np.isfinite(pcm).all() or np.max(np.abs(pcm)) > 1:
        raise ValueError('Fixture clips or contains invalid PCM.')
    encoded = np.rint(pcm * 32767).astype('<i2')
    path = folder / f'{name}.wav'
    with wave.open(str(path), 'wb') as stream:
        stream.setnchannels(2)
        stream.setsampwidth(2)
        stream.setframerate(RATE)
        stream.writeframes(encoded.tobytes())
    return dict(file=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                sampleRate=RATE, frames=len(pcm), channels=2, construction=truth)


def rhythm(meter, change):
    pcm = np.zeros(RATE * 40)
    events = []
    time, ordinal = .5, 0
    while time < 39.5:
        beat = ordinal % meter + 1
        # Different drum timbres make beat one measurable without encoding labels.
        length = int(.12 * RATE)
        t = np.arange(length) / RATE
        kick = .45 * np.sin(2 * np.pi * (60 * t - 35 * t * t)) * np.exp(-35 * t)
        attack = .18 * np.sin(2 * np.pi * (1600 if beat == 1 else 2500) * t) * np.exp(-150 * t)
        begin = round(time * RATE)
        pcm[begin:begin + length] += kick + attack
        events.append(dict(seconds=begin / RATE, position=beat, meter=meter))
        time += .4 if change and ordinal >= 40 else .5
        ordinal += 1
    return pcm, dict(beats=events, note='Synthetic accented percussion; meter is construction truth, not a guarantee of perceptual uniqueness.')


def tonal_sections():
    # C major: I-IV-V-I. Repeat A after a contrasting (still C-major) B section.
    patterns = (((60, 64, 67), (65, 69, 72), (67, 71, 74), (60, 64, 67)),
                ((69, 72, 76), (62, 65, 69), (67, 71, 74), (60, 64, 67)))
    parts = []
    cents = 17
    for section in (0, 1, 0):
        chords = []
        for notes in patterns[section]:
            t = np.arange(RATE * 2) / RATE
            envelope = np.minimum(1, t / .04) * np.minimum(1, (2 - t) / .08)
            chord = sum(np.sin(2 * np.pi * 440 * 2 ** ((note - 69 + cents / 100) / 12) * t) for note in notes) * .12
            chords.append(chord * envelope)
        parts.append(np.concatenate(chords))
    return np.concatenate(parts), dict(key='C major', tuningCents=cents,
        a4Hz=440 * 2 ** (cents / 1200), sectionStartsSeconds=[0, 8, 16],
        sectionGroups=['A', 'B', 'A'], note='Constructed harmonies and repeated PCM; commercial detector labels remain observations.')


def stretch_signal():
    t = np.arange(RATE * 12) / RATE
    tone = .18 * np.sin(2 * np.pi * 440 * t)
    pcm = np.column_stack((tone, -.5 * tone))
    # Unequal signed channels expose stereo collapse and phase errors.
    for seconds in (1, 4, 8, 11):
        pcm[round(seconds * RATE), :] += (.5, .3)
    return pcm, dict(toneHz=440, rightToneRelativeGain=-.5, impulsesSeconds=[1, 4, 8, 11],
        note='For stretch-factor/duration, independent pitch, stereo and attack timing tests; perceptual quality requires music listening.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, maker in [('rhythm-4-tempo-step', lambda: rhythm(4, True)),
                        ('rhythm-3-steady', lambda: rhythm(3, False)),
                        ('tonal-structure-aba', tonal_sections), ('stretch-stereo', stretch_signal)]:
        pcm, truth = maker()
        rows.append(write_wave(args.output, name, pcm, truth))
    (args.output / 'manifest.json').write_text(json.dumps(dict(format='alloy-engine-fixtures', version=1,
        scope='Synthetic construction truth; no recordings, commercial results or held-out inputs used.',
        fixtures=rows), indent=2), encoding='utf-8')
    print(f'Created {len(rows)} fixtures in {args.output.resolve()}')


if __name__ == '__main__':
    main()

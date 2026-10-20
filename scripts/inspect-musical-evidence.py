"""Independent spectral evidence for adjudication; reports pitches and attack intervals, never key/beat truth."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.ndimage import median_filter
from scipy.signal import find_peaks, resample_poly, stft
import soundfile as sf

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--paths', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
rows = []
for file in json.loads(args.paths.read_text(encoding='utf-8-sig')):
    info = sf.info(file)
    row = dict(file=file, sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(), windows=[])
    for fraction in (.1, .3, .5, .7, .9):
        start = max(0, (info.duration - 20) * fraction)
        pcm, rate = sf.read(file, start=int(start * info.samplerate), frames=info.samplerate * 20, always_2d=True)
        pcm = resample_poly(pcm, 11025, rate, axis=0)
        mid = pcm.mean(axis=1)
        side = (pcm[:, 0] - pcm[:, 1]) / 2 if pcm.shape[1] >= 2 else np.zeros(len(pcm))
        window = dict(startSeconds=start, channels={})
        for name, audio in [('mid', mid), ('side', side)]:
            frequency, times, spectrum = stft(audio, fs=11025, nperseg=16384, noverlap=12288, boundary=None)
            magnitude = np.abs(spectrum)
            # Temporal harmonic persistence and local spectral maxima differ
            # from the native estimator's per-frame profile scoring.
            harmonic = median_filter(magnitude, size=(1, 7))
            pitches = np.zeros(128)
            for frame in harmonic.T:
                peaks, _ = find_peaks(frame, height=frame.max() * .04)
                peaks = peaks[(frequency[peaks] >= 65) & (frequency[peaks] <= 2000)]
                for peak in peaks:
                    note = 69 + 12 * np.log2(frequency[peak] / 440)
                    if abs(note - round(note)) <= .25:
                        pitches[round(note)] += frame[peak]
            order = np.argsort(pitches)[-16:][::-1]
            names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
            window['channels'][name] = dict(rms=float(np.sqrt(np.mean(audio * audio))),
                sustainedPeaks=[dict(note=names[n % 12] + str(n // 12 - 1), midi=int(n),
                                     magnitude=float(pitches[n])) for n in order if pitches[n] > 0])
        frequency, times, spectrum = stft(mid, fs=11025, nperseg=1024, noverlap=969, boundary=None)
        magnitude = np.abs(spectrum)
        window['attacks'] = {}
        for name, low, high in [('bass', 40, 180), ('noisyMid', 400, 2000)]:
            band = magnitude[(frequency >= low) & (frequency <= high)]
            flux = np.maximum(0, np.diff(np.log1p(band * 1000), axis=1)).sum(axis=0)
            if name == 'noisyMid':
                flux *= np.exp(np.log(np.maximum(1e-12, band[:, 1:])).mean(axis=0)) / np.maximum(1e-12, band[:, 1:].mean(axis=0))
            peaks, _ = find_peaks(flux, prominence=max(1e-12, np.quantile(flux, .75)), distance=12)
            attacks = times[1:][peaks]
            intervals = np.diff(attacks)
            window['attacks'][name] = dict(timesSeconds=(attacks + start).tolist(),
                intervalModesSeconds=[dict(seconds=float(v), count=int(n)) for v, n in sorted(
                    zip(*np.unique(np.round(intervals / .01) * .01, return_counts=True)), key=lambda item: -item[1])[:8]])
        row['windows'].append(window)
    rows.append(row)
    (args.output / 'evidence.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    print(Path(file).name, flush=True)

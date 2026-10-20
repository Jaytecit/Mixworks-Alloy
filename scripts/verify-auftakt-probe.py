"""Compare native features/observations with independent NumPy/SciPy calculations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import wave
import numpy as np
from scipy.fft import rfft


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def reference_features(pcm, banks):
    frames = (len(pcm) + 440) // 441
    result = np.zeros((frames, 320), dtype=np.float32)
    column = 0
    for group, (size, count, lag) in enumerate(zip((1024, 2048, 4096), (22, 46, 92), (1, 1, 2))):
        window = (1 - np.cos((np.arange(1, size + 1, dtype=np.float32) * np.float32(6.2831855)) / np.float32(size + 1))) * np.float32(.5)
        padded = np.pad(pcm, (size // 2, size // 2 + 440))
        blocks = np.lib.stride_tricks.sliding_window_view(padded, size)[::441][:frames]
        for begin in range(0, frames, 128):
            spectrum = np.abs(rfft(blocks[begin:begin + 128] * window, axis=1))
            for band in range(count):
                weights = banks[str(group)][str(band)]
                indices = sorted(map(int, weights))
                values = np.array([weights[str(k)] for k in indices], dtype=np.float32)
                # Ordered reduction, independent FFT and vectorized frame computation.
                sums = np.cumsum(spectrum[:, indices] * values, axis=1)[:, -1]
                result[begin:begin + len(spectrum), column + band] = np.log1p(sums) * np.float32(.4342944920063019)
        result[lag:, column + count:column + 2 * count] = np.maximum(0, result[lag:, column:column + count] - result[:-lag, column:column + count])
        column += count * 2
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new verification output.')
    probe = json.loads(args.probe.read_text(encoding='utf-8-sig'))
    source = Path(probe['source'])
    if (hashlib.sha256(source.read_bytes()).hexdigest() != probe['sourceSha256'] or probe['error']
            or hashlib.sha256(args.model.read_bytes()).hexdigest() != probe['modelSha256']):
        raise ValueError('Invalid probe/source/model identity.')
    with wave.open(str(source)) as stream:
        if stream.getframerate() != 44100 or stream.getsampwidth() != 2:
            raise ValueError('This independent oracle requires 44.1 kHz PCM16 WAV.')
        raw = np.frombuffer(stream.readframes(stream.getnframes()), dtype='<i2').reshape(-1, stream.getnchannels())
    pcm = (raw.astype(np.float32) / np.float32(32768)).mean(axis=1)
    recovery = load_module('filter_recovery', Path(__file__).with_name('recover-auftakt-filterbank.py'))
    banks, _ = recovery.filters(args.evidence)
    expected = reference_features(pcm, banks)
    actual = np.asarray(probe['featureValues'], dtype=np.float32).reshape(-1, 320)
    feature_error = float(np.max(np.abs(expected - actual)))
    network = load_module('network_reference', Path(__file__).with_name('auftakt-network-reference.py'))
    model = json.loads(args.model.read_text())
    predicted = network.predict(model, actual)
    observed = np.asarray(probe['classProbabilities'], dtype=np.float32)
    observation_error = float(np.max(np.abs(predicted - observed)))
    result = dict(scope='Independent reconstruction oracle; not a live vendor PCM comparison or musical acceptance.',
                  featureValues=actual.size, observationValues=observed.size,
                  featureMaximumAbsoluteError=feature_error, observationMaximumAbsoluteError=observation_error,
                  packagedNetworkTestMaximumError=probe['packagedNetworkTestMaximumError'],
                  probeSha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(), sourceSha256=probe['sourceSha256'],
                  modelSha256=probe['modelSha256'], executableSha256=probe['executableSha256'],
                  passed=feature_error < 1e-5 and observation_error < 1e-4)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

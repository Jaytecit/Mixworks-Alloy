"""Offline, one-job Demucs worker. Only a complete validated directory is published.

Input PCM is decoded by Alloy's own decoder, at the original source rate/length.
This worker never reads or writes analysis, corrections, sessions or original audio.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import random
import shutil
import sys
import time

NAMES = ['drums', 'bass', 'other', 'vocals']
SETTINGS = dict(schema=1, model='htdemucs', shifts=1, seed=1729, segment=7.8,
                overlap=0.25, decoder='juce-float32-v1', resampler='julius-0.2.7',
                quality='standard', alignment='source-samples', workerRevision=2,
                maxResidualRatio=0.15)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False), encoding='utf-8')
    os.replace(temporary, path)


def cache_key(source, pcm, runtime):
    data = dict(source=source, pcm=pcm, settings=SETTINGS, runtime=runtime)
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()


def validate_assets(folder, expected):
    import numpy as np
    import soundfile as sf
    if folder.is_symlink():
        raise ValueError('Linked stem directories are not owned cache entries')
    manifest = json.loads((folder / 'manifest.json').read_text())
    if any(manifest.get(k) != v for k, v in expected.items()):
        raise ValueError('Stem manifest does not match this source and runtime')
    if list(manifest['stems']) != NAMES:
        raise ValueError('Incomplete stem set')
    for name in NAMES:
        path = folder / (name + '.wav')
        if path.is_symlink() or digest(path) != manifest['stems'][name]['sha256']:
            raise ValueError('Stem file is missing or changed: ' + name)
        with sf.SoundFile(path) as audio:
            if (audio.frames, audio.samplerate, audio.channels, audio.subtype) != (
                    expected['frames'], expected['sampleRate'], expected['channels'], 'FLOAT'):
                raise ValueError('Stem sample clock differs from source')
            for block in audio.blocks(blocksize=65536, dtype='float32'):
                if not np.isfinite(block).all():
                    raise ValueError('Non-finite stem samples')
    return manifest


class Cancelled(Exception):
    pass


def check_recombination(source, folder, check_cancel=lambda: None):
    """Reject gross separation/resampling failures without forcing the mix sum."""
    import numpy as np
    import soundfile as sf
    from contextlib import ExitStack
    signal = error = 0.0
    samples = 0
    with ExitStack() as stack:
        original = stack.enter_context(sf.SoundFile(source))
        stems = [stack.enter_context(sf.SoundFile(folder / (n + '.wav'))) for n in NAMES]
        for block in original.blocks(blocksize=65536, dtype='float64', always_2d=True):
            check_cancel()
            summed = sum(f.read(len(block), dtype='float64', always_2d=True) for f in stems)
            if summed.shape != block.shape or not np.isfinite(summed).all():
                raise ValueError('Invalid stem sample clock or samples')
            signal += float(np.sum(block**2)); error += float(np.sum((block-summed)**2)); samples += block.size
    signal_rms = math.sqrt(signal / max(samples, 1)); error_rms = math.sqrt(error / max(samples, 1))
    ratio = error_rms / max(signal_rms, 1e-12)
    if (signal_rms > 1e-7 and ratio > SETTINGS['maxResidualRatio']) or (signal_rms <= 1e-7 and error_rms > 1e-7):
        raise ValueError(f'Stem recombination error is too high ({ratio:.1%}); original source kept')
    return dict(sourceRms=signal_rms, residualRms=error_rms, residualRatio=ratio)


def run(job_path):
    import numpy as np
    import soundfile as sf
    job_path = job_path.resolve()
    folder = job_path.parent
    request = json.loads(job_path.read_text(encoding='utf-8-sig'))
    status = folder / 'status.json'
    def report(state, progress=0, **extra):
        write_json(status, dict(state=state, progress=progress, **extra))
    def cancelled():
        if (folder / 'cancel').exists():
            raise Cancelled('Separation cancelled')
    pending = folder / 'assets'
    try:
        cancelled()
        report('Checking source')
        pcm = folder / 'input.wav'
        config = json.loads(Path(request['runtime']).read_text(encoding='utf-8-sig'))
        model_path = Path(config['model'])
        if digest(model_path) != config['modelSha256']:
            raise ValueError('Stem model changed; run setup again')
        if digest(__file__) != config['workerSha256']:
            raise ValueError('Stem worker changed; run setup again')
        packages = {name: importlib.metadata.version(name) for name in config['packages']}
        if packages != config['packages']:
            raise ValueError('Stem runtime versions changed; run setup again')
        runtime_id = dict(modelSha256=config['modelSha256'], workerSha256=config['workerSha256'], packages=packages)
        pcm_hash = digest(pcm)
        info = sf.info(pcm)
        if info.channels not in (1, 2) or not 8000 <= info.samplerate <= 192000 or not 0 < info.frames <= info.samplerate * 600:
            raise ValueError('Split supports mono/stereo tracks up to 10 minutes at 8–192 kHz')
        if info.frames * info.channels * 4 > 512 * 1024**2:
            raise ValueError('Decoded source exceeds the 512 MB separation limit')
        key = cache_key(request['sourceSha256'], pcm_hash, runtime_id)
        expected = dict(schema=1, key=key, sourceSha256=request['sourceSha256'], pcmSha256=pcm_hash,
                        frames=info.frames, sampleRate=info.samplerate, channels=info.channels,
                        settings=SETTINGS, runtime=runtime_id, offsetSamples=0)
        cache = Path(request['cache']).resolve()
        cache.mkdir(parents=True, exist_ok=True)
        target = cache / key
        if target.exists():
            validate_assets(target, expected)
            cancelled()
            report('Ready', 1, directory=str(target), reused=True)
            return 0
        # Bound storage before loading any model. No cache eviction or user-data cleanup.
        required = info.frames * info.channels * 4 * 4 + 512 * 1024**2
        if shutil.disk_usage(cache).free < required:
            raise ValueError('Not enough free space for four stems and working headroom')
        import torch
        import julius
        from demucs.hf import load_safetensors_model
        from demucs.apply import apply_model
        torch.set_num_threads(min(4, os.cpu_count() or 1))
        device = request.get('device', 'auto')
        if device not in ('auto', 'cpu', 'cuda'):
            raise ValueError('Invalid separation device')
        device = ('cuda' if torch.cuda.is_available() else 'cpu') if device == 'auto' else device
        report('Loading model', .02, device=device)
        model = load_safetensors_model(model_path).eval()
        if list(model.sources) != NAMES or model.samplerate != 44100 or model.audio_channels != 2:
            raise ValueError('Unsupported model topology')
        data, rate = sf.read(pcm, dtype='float32', always_2d=True)
        if not np.isfinite(data).all():
            raise ValueError('Non-finite source samples')
        wav = torch.from_numpy(data.T.copy())
        del data
        if info.channels == 1:
            wav = wav.expand(2, -1)
        if rate != 44100:
            wav = julius.resample_frac(wav, rate, 44100, full=True)
        # Match upstream normalisation, but retain antiphase stereo energy.
        mean = wav.mean()
        std = wav.std().clamp_min(1e-8)
        normal = (wav - mean) / std
        length = normal.shape[-1]
        started = time.monotonic()
        def separate(selected):
            random.seed(SETTINGS['seed']); torch.manual_seed(SETTINGS['seed'])
            def callback(event):
                cancelled()
                if event['state'] == 'end':
                    progress = min(.94, .04 + .90 * (event['segment_offset'] + 7.8 * 44100) / length)
                    report('Separating', progress, device=selected)
            with torch.inference_mode():
                return apply_model(model, normal[None], shifts=1, split=True, overlap=.25,
                                   segment=7.8, device=selected, num_workers=0, callback=callback)[0]
        cancelled()
        try:
            estimates = separate(device)
        except (torch.cuda.OutOfMemoryError, RuntimeError) as error:
            # Only auto device has an explicit CPU fallback. Other failures remain failures.
            if request.get('device', 'auto') != 'auto' or device != 'cuda' or not (
                    isinstance(error, torch.cuda.OutOfMemoryError) or 'CUDA' in str(error)):
                raise
            model.cpu(); torch.cuda.empty_cache(); device = 'cpu'
            report('GPU unavailable; using CPU', .04, device=device)
            estimates = separate(device)
        del normal
        pending.mkdir(exist_ok=False)
        stems = {}
        for index, name in enumerate(NAMES):
            cancelled()
            output = estimates[index] * std + mean / len(NAMES)
            if rate != 44100:
                output = julius.resample_frac(output, 44100, rate, output_length=info.frames)
            if info.channels == 1:
                output = output.mean(0, keepdim=True)
            if output.shape != (info.channels, info.frames) or not torch.isfinite(output).all():
                raise ValueError('Invalid aligned output: ' + name)
            path = pending / (name + '.wav')
            sf.write(path, output.T.numpy(), rate, subtype='FLOAT')
            stems[name] = dict(sha256=digest(path), bytes=path.stat().st_size)
            report('Validating stems', .95 + index * .01, device=device)
        quality = check_recombination(pcm, pending, cancelled)
        manifest = dict(expected, stems=stems, quality=quality, elapsedSeconds=time.monotonic()-started, device=device,
                        peakVramBytes=torch.cuda.max_memory_allocated() if device == 'cuda' else 0)
        write_json(pending / 'manifest.json', manifest)
        validate_assets(pending, expected)
        cancelled()
        # App owns final publication after checking the *original* identity again.
        report('Prepared', 1, directory=str(pending), key=key, manifest=manifest)
        return 0
    except Cancelled as error:
        report('Cancelled', error=str(error))
        return 2
    except Exception as error:
        report('Failed', error=str(error))
        return 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--job', type=Path, required=True)
    sys.exit(run(parser.parse_args().job))

"""Create or verify an owned, deterministic 90-minute WAV without replacing files."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

NAME = 'owned-90-minute-stereo-44100.wav'
MANIFEST = 'owned-90-minute-manifest.json'
FRAMES = 238140000
FILE_BYTES = 1428840044
SHA256 = '7b397038d240e21f15c560634e71077a8f044fee267ecfbe96d22a2d741df8e7'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def ensure_capacity_fixture(folder):
    folder = Path(folder).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    source, manifest = folder / NAME, folder / MANIFEST
    if not source.exists() and not manifest.exists():
        data_bytes = FRAMES * 6
        header = (b'RIFF' + struct.pack('<I', 36 + data_bytes) + b'WAVEfmt '
                  + struct.pack('<IHHIIHH', 16, 1, 2, 44100, 264600, 6, 24)
                  + b'data' + struct.pack('<I', data_bytes))
        block = bytearray(44100 * 6)
        block[:3] = (2097152).to_bytes(3, 'little', signed=True)
        block[3:6] = (-1048576).to_bytes(3, 'little', signed=True)
        with source.open('xb') as stream:
            stream.write(header)
            for _ in range(5400):
                stream.write(block)
        if source.stat().st_size != FILE_BYTES or digest(source) != SHA256:
            raise ValueError('Capacity fixture did not match its construction; the partial output was retained.')
        with manifest.open('x', encoding='utf-8') as stream:
            json.dump(dict(format='mixworks-capacity-fixture', schemaVersion=1,
                           path=str(source), purpose='Owned technical capacity fixture, not musical ground truth',
                           seconds=5400, sampleRate=44100, channels=2, frames=FRAMES,
                           decodedFloatBytes=FRAMES * 8, fileBytes=FILE_BYTES, sha256=SHA256,
                           leftImpulse=.25, rightImpulse=-.125, impulseSpacingFrames=44100), stream, indent=2)
    if not source.is_file() or not manifest.is_file():
        raise ValueError('Incomplete existing capacity fixture was kept; choose a fresh directory.')
    if source.stat().st_size != FILE_BYTES or manifest.stat().st_size > 256 * 1024:
        raise ValueError('Capacity data dimensions or the 256 KiB metadata budget do not match; existing files were kept.')
    record = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if (not isinstance(record, dict) or record.get('frames') != FRAMES or record.get('sampleRate') != 44100
            or record.get('channels') != 2 or record.get('sha256') != SHA256
            or source.stat().st_size != FILE_BYTES or digest(source) != SHA256):
        raise ValueError('Existing capacity files differ; both files were kept unchanged.')
    return dict(path=str(source), sha256=SHA256, fileBytes=FILE_BYTES,
                manifest=str(manifest), manifestSha256=digest(manifest))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--folder', type=Path, default=Path(__file__).resolve().parents[1] / '.local/long-session-26')
    args = parser.parse_args()
    try:
        identity = ensure_capacity_fixture(args.folder)
    except (OSError, ValueError) as error:
        parser.exit(1, f'Capacity fixture unavailable: {error}\n')
    print(f"Owned capacity fixture verified: {identity['path']}")


if __name__ == '__main__':
    main()

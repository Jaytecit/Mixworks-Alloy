"""Verify numerical 60/90-minute real-source workflows in isolated native processes."""
import argparse
from pathlib import Path

from verification import bundle_identity, digest, plugin_identity, read_json, run_checks, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--development-paths', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--minutes', type=int, choices=(60, 90), nargs='+', default=(60, 90))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    workspace = Path(__file__).resolve().parents[1]
    summary = dict(format='mixworks-long-session-tests', schemaVersion=1, cases=[], errors=[], complete=False,
                   scope='Repeated manual real-source arrangements and numerical callbacks; physical audio, listening and musical alignment remain unverified.')
    summary_file = args.output / 'summary.json'
    try:
        if len(set(args.minutes)) != len(args.minutes):
            raise ValueError('Choose each requested duration once.')
        exe = args.exe.resolve()
        summary.update(bundle_identity(exe))
        paths = read_json(args.development_paths)
        if (not isinstance(paths, list) or len(paths) != 7
                or any(not isinstance(p, str) or not Path(p).is_absolute() for p in paths)):
            raise ValueError('Select the explicit seven-recording development manifest.')
        summary['sources'] = [dict(path=p, sha256=digest(p)) for p in paths]
        gain = workspace / 'build/MixworksTestGain_artefacts/Release/VST3/Mixworks Test Gain.vst3'
        delay = workspace / 'build/MixworksTestDelay_artefacts/Release/VST3/Mixworks Test Delay.vst3'
        summary['plugins'] = [plugin_identity(gain), plugin_identity(delay)]
        for minutes in args.minutes:
            folder = args.output / str(minutes)
            flags = ['--long-session-only', '--minutes', str(minutes), '--development-paths', str(args.development_paths.resolve()),
                     '--plugin', str(gain), '--delay-plugin', str(delay)]
            result = dict(minutes=minutes, **run_checks(exe, flags, folder, timeout=3600, cwd=workspace))
            metrics = folder / 'long-session-metrics.json'
            if metrics.is_file():
                result['metrics'] = dict(path=str(metrics.resolve()), sha256=digest(metrics))
            summary['cases'].append(result)
            write_json(summary_file, summary)
            print(f"{minutes} minutes: {result['summary']}; exit {result['exit']}", flush=True)
        if any(digest(p['path']) != p['sha256'] for p in summary['sources']):
            raise ValueError('An original development source changed during acceptance.')
        if summary['plugins'] != [plugin_identity(gain), plugin_identity(delay)]:
            raise ValueError('A native test plugin changed during acceptance.')
        identity = bundle_identity(exe)
        if any(summary[k] != v for k, v in identity.items()):
            raise ValueError('Acceptance bundle changed during the workflow.')
        summary['complete'] = len(summary['cases']) == len(args.minutes)
    except (OSError, ValueError, TypeError) as error:
        summary['errors'].append(str(error))
    write_json(summary_file, summary)
    return int(not summary['complete'] or bool(summary['errors']) or any(r['exit'] for r in summary['cases']))


if __name__ == '__main__':
    raise SystemExit(main())

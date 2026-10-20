"""Run every required regression suite, sequentially, against one immutable bundle."""
import argparse
from pathlib import Path
import shutil

from capacity_fixture import ensure_capacity_fixture

from verification import REGRESSION_SUITES, bundle_identity, create_receipt, plugin_identity, run_checks, write_json


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, help='Write a regression receipt only after every suite passes.')
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=False)
    exe = args.exe.resolve()
    workspace = Path(__file__).resolve().parents[1]
    summary = dict(format='mixworks-analysis-tests', schemaVersion=2, suites=[], errors=[])
    summary_file = args.output / 'summary.json'
    try:
        identity = bundle_identity(exe)
        summary.update(identity)
        plugin = workspace / 'build/MixworksTestGain_artefacts/Release/VST3/Mixworks Test Gain.vst3'
        delay = workspace / 'build/MixworksTestDelay_artefacts/Release/VST3/Mixworks Test Delay.vst3'
        latency = workspace / 'build/MixworksTestLatency_artefacts/Release/VST3/Mixworks Test Latency.vst3'
        instrument = Path('C:/Program Files/Common Files/VST3/Vital.vst3')
        if not plugin.exists() or not delay.exists() or not latency.exists() or not instrument.exists():
            raise FileNotFoundError('The real test VST3 and installed Vital are required; coverage is incomplete.')
        native_plugins = [plugin, delay, latency, instrument]
        summary['nativePluginEvidence'] = [plugin_identity(p) for p in native_plugins]
        summary['capacityFixture'] = ensure_capacity_fixture(workspace / '.local/long-session-26')
        for suite in REGRESSION_SUITES:
            target = exe
            flags = ['--plugin', str(plugin), '--delay-plugin', str(delay), '--latency-plugin', str(latency)]
            if suite in ('missing-model', 'corrupt-model'):
                bundle = args.output / (suite + '-bundle')
                bundle.mkdir()
                shutil.copy2(exe, bundle / 'Mixworks.exe')
                shutil.copy2(exe.with_name('onnxruntime.dll'), bundle / 'onnxruntime.dll')
                if suite == 'corrupt-model':
                    (bundle / 'beat-this-final0.onnx').write_bytes(b'Owned invalid model fixture\n')
                target = (bundle / 'Mixworks.exe').resolve()
                flags = ['--neural-unavailable-only']
            elif suite == 'source-streaming':
                flags = ['--source-streaming-only', '--capacity-fixture', summary['capacityFixture']['path']]
            elif suite == 'core':
                flags += ['--instrument', str(instrument)]
            else:
                flags.insert(0, '--' + suite + '-only')
            result = dict(name=suite, **run_checks(target, flags, args.output / suite, cwd=workspace))
            summary['suites'].append(result)
            write_json(summary_file, summary)
            print(f"{suite}: {result['summary']}; exit {result['exit']}", flush=True)
        if bundle_identity(exe) != identity:
            raise ValueError('Candidate bundle changed during verification.')
        if summary['capacityFixture'] != ensure_capacity_fixture(workspace / '.local/long-session-26'):
            raise ValueError('Capacity fixture changed during regression verification.')
        if summary['nativePluginEvidence'] != [plugin_identity(p) for p in native_plugins]:
            raise ValueError('Native verification plugins changed during the regression run.')
    except (OSError, ValueError) as error:
        summary['errors'].append(str(error))
    write_json(summary_file, summary)
    failed = bool(summary['errors'] or any(suite['exit'] for suite in summary['suites'])
                  or len(summary['suites']) != len(REGRESSION_SUITES))
    if not failed and args.receipt:
        try:
            create_receipt(summary_file, exe, args.receipt)
        except (OSError, ValueError, KeyError, TypeError) as error:
            summary['errors'].append(str(error))
            write_json(summary_file, summary)
            failed = True
    if summary['errors']:
        print('\n'.join(summary['errors']), flush=True)
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())

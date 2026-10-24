"""Scoped local UI verification, anchored to a previously accepted full baseline."""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile

from verification import (bundle_identity, check_report, digest, read_json,
                          run_checks, validate_receipt, write_json)

# These files still require semantic review: a UI filename does not prove a
# cosmetic diff. Anything outside this list requires the complete gate.
UI_FILES = set('''AlloyLookAndFeel.cpp AlloyLookAndFeel.h AppWindow.cpp AppWindow.h
WorkspaceLayout.cpp WorkspaceLayout.h WorkspaceUI.cpp PerformanceUI.cpp PlaylistTransitions.cpp
PerformanceDeck.cpp AutomationUI.cpp TimelineAutomation.cpp TimelineHeader.cpp TempoStrip.cpp Workstation.cpp
Workstation.h MidiConnectionUI.cpp AudioSlipUI.cpp GridCorrectionUI.cpp GridCorrectionUI.h
SourceSectionUI.cpp AutoArrangementUI.cpp TransitionUI.cpp TransitionRehearsalUI.cpp
HotcueUI.cpp SectionUI.cpp MasteringUI.cpp EditSelection.cpp TimelineEditing.cpp'''.split())
UI_SUITES = {'window', 'alloy', 'tempo', 'tempo-browser', 'timeline-editing'}


def evidence(path):
    return dict(path=str(Path(path).resolve()), sha256=digest(path))


def verified_path(item):
    if digest(item['path']) != item['sha256']:
        raise ValueError('Scoped verification evidence changed: ' + item['path'])
    return Path(item['path'])


def source_snapshot(manifest, archive):
    expected = {k.replace('\\', '/'): v for k, v in read_json(manifest).items()}
    if 'CMakeLists.txt' not in expected or not any(n.startswith('Source/') for n in expected):
        raise ValueError('Full Source/CMake snapshot required.')
    with zipfile.ZipFile(archive) as bundle:
        actual = {n.replace('\\', '/'): bundle.read(n) for n in bundle.namelist()}
    if set(actual) != set(expected) or any(hashlib.sha256(actual[n]).hexdigest() != h for n, h in expected.items()):
        raise ValueError('Source archive does not match its manifest.')
    return actual


def changed_sources(before, after):
    changed = sorted(n for n in before.keys() | after.keys() if before.get(n) != after.get(n))
    for name in changed:
        if name not in before or name not in after:
            raise ValueError('Source membership changed; use full verification: ' + name)
        if name == 'CMakeLists.txt':
            normalise = lambda b: re.sub(rb'\bVERSION\s+\d+\.\d+\.\d+', b'VERSION <version>', b)
            if normalise(before[name]) != normalise(after[name]):
                raise ValueError('Build configuration changed; use full verification.')
        elif name == 'Source/StemUI.cpp':
            # One verified encoding repair, not general permission for job/UI changes.
            old = 'Vocals, drums, bass and other • source-aligned WAVs'.encode('utf-8')
            new = b'Vocals, drums, bass and other - source-aligned WAVs'
            normalise = lambda b: b.replace(b'\r\n', b'\n')
            if old not in before[name] or normalise(before[name].replace(old, new)) != normalise(after[name]):
                raise ValueError('Stem changes exceed the caption encoding repair; use full verification.')
        elif name == 'Source/EnergySections.cpp':
            old = b'juce::String((int) i + 1) + " ?"'
            new = b'juce::String((int) i + 1)'
            normalise = lambda b: b.replace(b'\r\n', b'\n')
            if old not in before[name] or normalise(before[name].replace(old, new)) != normalise(after[name]):
                raise ValueError('Section changes exceed the approved label-only edit; use full verification.')
        elif not (name.startswith('Source/') and (Path(name).name in UI_FILES or name.endswith('Tests.cpp'))):
            raise ValueError('Engine/data or unclassified source changed; use full verification: ' + name)
    return changed


def validate_scoped_receipt(receipt, exe):
    if (receipt.get('format') != 'mixworks-release-verification' or receipt.get('schemaVersion') != 3
            or receipt.get('scope') not in ('presentation', 'ui-behaviour')):
        raise ValueError('Unsupported scoped receipt.')
    identity = bundle_identity(exe)
    if any(receipt.get(k) != v for k,v in identity.items() if k != 'executable'):
        raise ValueError('Scoped candidate bundle changed.')
    baseline = receipt['baseline']
    baseline_receipt = read_json(verified_path(baseline['receipt']))
    if baseline_receipt.get('scope') != 'complete-regressions':
        raise ValueError('Scoped changes must start from a full regression baseline.')
    validate_receipt(baseline['receipt']['path'], baseline_receipt['executable'])
    for key in ('modelSha256', 'runtimeSha256'):
        if baseline_receipt[key] != receipt[key]:
            raise ValueError('Analysis dependencies changed; use full verification.')
    before = source_snapshot(verified_path(baseline['source']), verified_path(baseline['archive']))
    after = source_snapshot(verified_path(receipt['source']), verified_path(receipt['archive']))
    if changed_sources(before, after) != receipt['changedSources']:
        raise ValueError('Changed-source classification disagrees with frozen evidence.')
    if not receipt.get('reviewNote', '').strip():
        raise ValueError('Describe the semantic scope and actual visual/workflow review.')
    image = verified_path(receipt['visualProof'])
    with image.open('rb') as stream:
        if stream.read(8) != b'\x89PNG\r\n\x1a\n':
            raise ValueError('Visual proof must be a captured PNG from this candidate.')
    suites = receipt['suites']
    if receipt['scope'] == 'ui-behaviour' and not suites:
        raise ValueError('UI behaviour requires affected native suites.')
    if len({s['name'] for s in suites}) != len(suites):
        raise ValueError('Duplicate scoped suites.')
    for suite in suites:
        name = suite['name']
        if name not in UI_SUITES or suite['invocation'].get('flags') != ['--' + name + '-only']:
            raise ValueError('Unexpected scoped suite invocation.')
        if suite['invocation'].get('executableSha256') != identity['executableSha256']:
            raise ValueError('Suite tested a different binary.')
        actual = check_report(suite['report'], suite['processExit'])
        if any(suite.get(k) != actual[k] for k in actual) or actual['exit'] or actual['errors'] or actual['failures']:
            raise ValueError('Scoped suite failed or evidence changed.')
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scope', choices=('presentation', 'ui-behaviour'), required=True)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--baseline-receipt', type=Path, required=True)
    parser.add_argument('--baseline-source', type=Path, required=True)
    parser.add_argument('--baseline-archive', type=Path, required=True)
    parser.add_argument('--visual-proof', type=Path, required=True)
    parser.add_argument('--note', required=True)
    parser.add_argument('--suite', action='append', choices=sorted(UI_SUITES), default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        baseline = read_json(args.baseline_receipt)
        if baseline.get('scope') != 'complete-regressions':
            raise ValueError('Select an accepted full baseline, not a scoped receipt.')
        validate_receipt(args.baseline_receipt, baseline['executable'])
        before = source_snapshot(args.baseline_source, args.baseline_archive)
        root = Path(__file__).resolve().parents[1]
        inputs = [root/'CMakeLists.txt', *(p for p in (root/'Source').rglob('*') if p.is_file())]
        after = {p.relative_to(root).as_posix(): p.read_bytes() for p in inputs}
        changed = changed_sources(before, after)
        if args.scope == 'ui-behaviour' and not args.suite:
            raise ValueError('Select suites covering the affected UI workflow.')
        args.output.mkdir(parents=True, exist_ok=False)
        source = args.output/'candidate-source.json'; archive = args.output/'candidate-source.zip'
        write_json(source, {n:hashlib.sha256(b).hexdigest() for n,b in after.items()})
        with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as bundle:
            for n,b in after.items(): bundle.writestr(n,b)
        receipt = dict(format='mixworks-release-verification', schemaVersion=3, scope=args.scope,
                       **bundle_identity(args.exe), reviewNote=args.note, changedSources=changed,
                       visualProof=evidence(args.visual_proof), source=evidence(source), archive=evidence(archive),
                       baseline=dict(receipt=evidence(args.baseline_receipt), source=evidence(args.baseline_source),
                                     archive=evidence(args.baseline_archive)), suites=[])
        for name in dict.fromkeys(args.suite):
            receipt['suites'].append(dict(name=name, **run_checks(args.exe, ['--'+name+'-only'], args.output/name)))
        validate_scoped_receipt(receipt, args.exe)
        write_json(args.output/'receipt.json', receipt)
        print(f'Accepted {args.scope}: {len(receipt["suites"])} suites. No full gate was run.')
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Scoped verification rejected: {error}\n')


if __name__ == '__main__':
    main()

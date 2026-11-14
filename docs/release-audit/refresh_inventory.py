"""Refresh/check audit discovery registries; standard library, no app execution."""
from pathlib import Path
import argparse
import csv
import hashlib
import io
import json
import re
import subprocess
from collections import defaultdict

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUTPUTS = ('files.csv', 'control-sites.csv', 'cli-options.csv')
GROUPS = {
    'app': ('entry, ownership and process lifecycle', 'MW-R01 MW-R02 MW-U01 MW-D04', 'source dispatch/teardown review; isolated startup/close'),
    'ui': ('workstation, controls, layout and visual feedback', 'MW-U01 MW-U02 MW-U03 MW-U04 MW-U05 MW-U06 MW-U07 MW-U08', 'trace inputs/callbacks; native gestures, accessibility and captures'),
    'time': ('musical clocks, geometry and edits', 'MW-T01 MW-T02 MW-T03 MW-T04 MW-T05 MW-T06 MW-T07 MW-T08 MW-T09 MW-T10 MW-T11 MW-T12', 'independent timing/PCM and edit properties; native workflow'),
    'analysis': ('detector, source grid and corrections', 'MW-N01 MW-N02 MW-N03 MW-N04 MW-N05 MW-N06 MW-N07 MW-N08 MW-N09', 'analytic/corpus oracle; fresh source-bound analysis; failure/cancel'),
    'library': ('decode/import, catalogue, scan and preview', 'MW-L01 MW-L02 MW-L03 MW-L04 MW-L05 MW-L06', 'decode independent fixture; queue/reuse/cancel/preview integration'),
    'audio': ('programme render, DSP and processor ownership', 'MW-A01 MW-A02 MW-A03 MW-A04 MW-A05 MW-A06 MW-A07 MW-A08 MW-A09 MW-A10 MW-A11', 'analytic PCM; callback ownership/deadlines; listening/device'),
    'midi': ('controller identity, mapping and reception', 'MW-P01 MW-P02 MW-P03 MW-P04', 'enumerate typed actions; native MIDI and physical device checks'),
    'performance': ('launch, take, automation capture and private routing', 'MW-P05 MW-P06 MW-P07 MW-P08 MW-P09 MW-P10 MW-P11 MW-P12', 'sample-clock replay/null; native controls and physical private routing'),
    'data': ('current-format state, identity and recovery', 'MW-D01 MW-D02 MW-D03 MW-D04 MW-D05 MW-D06 MW-D07', 'round-trip; parser fault/identity tests; independent saved-state review'),
    'stems': ('optional worker, external assets and stem playback', 'MW-S01 MW-S02 MW-S03 MW-S04 MW-S05 MW-S06', 'worker harness, independent PCM/hash validation and listening'),
    'arrange': ('ordering, energy/section policy and rehearsal', 'MW-O01 MW-O02 MW-O03 MW-O04 MW-O05', 'constraint oracle; seeded plan/diff; runtime and musical listening'),
    'visuals': ('visual scene planning, image input and native output', 'MW-V01 MW-V02 MW-V03 MW-U01 MW-U05 MW-D01 MW-R02 MW-R03 MW-R04 MW-R06 MW-R09', 'complete schedule and handover oracle; save/reopen; actual scene/output pixels; lifecycle/errors'),
    'export': ('offline render and exchange outputs', 'MW-X01 MW-X02 MW-X03 MW-X04 MW-X05 MW-X06', 'independent decoder, null/duration analysis; failure and UI'),
    'tooling': ('build, verification, isolation and release support', 'MW-R02 MW-R03 MW-R04 MW-R05 MW-R06 MW-R07 MW-R08 MW-R09', 'inspect scripts; focused Python harness; manifests and failure injection'),
    'rights': ('third-party integration/provenance and release rights', 'MW-R03 MW-R04 MW-R10', 'exact dependency/asset identities, notices and clearance record'),
    'research': ('reconstructed algorithms and comparative research', 'MW-R03 MW-R09 MW-N01', 'establish reachable consumers, independent oracle/provenance; no roadmap claim'),
    'docs': ('requirements, historical evidence and release claims', 'MW-R11 MW-R09', 'compare claims/date/source; preserve history; verify links'),
    'assets': ('design/reference assets, not runtime resources', 'MW-R03 MW-R11', 'provenance and package inclusion/exclusion; inspect when used'),
    'framework': ('durable audit framework and inventory maintenance', 'MW-R12', 'membership/rule/link/self-review checks; no product PASS'),
}


def classify(path):
    if path.startswith('docs/release-audit/'):
        return 'framework'
    if path.startswith(('licenses/', 'Source/ThirdParty/')) or path in ('LICENSE', 'THIRD-PARTY-NOTICES.md'):
        return 'rights'
    if path.startswith('design/') or path.lower().endswith('.png'):
        return 'assets'
    name = Path(path).stem
    if path.startswith('docs/') or path.endswith('.md'):
        return 'docs'
    if path.startswith(('visual-studio/', 'visuals/')):
        return 'visuals'
    if path.startswith('scripts/visual_') or path.startswith('scripts/verify_visual_'):
        return 'visuals'
    if path.startswith('scripts/') or path.startswith('tests/test_') or path in ('CMakeLists.txt', 'Mixworks.cmd', '.gitignore'):
        return 'tooling'
    if path.startswith('tests/'):
        return 'audio' if name.startswith('Test') else 'research'
    if name.startswith(('JuceMp3',)):
        return 'library'
    if name.startswith(('Auftakt', 'RhythmResearch')):
        return 'research'
    if name.startswith(('Main', 'AppWindow', 'WindowTests')):
        return 'app'
    if name.startswith(('Stem',)):
        return 'stems'
    if name.startswith(('AutoArrangement', 'AutoPairComparison', 'EnergySections', 'SectionTests', 'RehearsalContext', 'TransitionRehearsal', 'TransitionStyles', 'MusicalEnergy')):
        return 'arrange'
    if name.startswith(('Performance', 'PrivatePreparation', 'MasterRecorder', 'Launch', 'LiveHotcue', 'LiveAutomation', 'Preroll')):
        return 'performance'
    if name.startswith(('Midi',)):
        return 'midi'
    if name.startswith(('Analysis', 'Neural', 'Rhythm', 'GridCorrection', 'BeatGrid', 'MusicTests', 'TargetedAnalysis', 'DownbeatReferences', 'RealTrack', 'EarTool', 'HarmonyEar', 'KeyEar', 'RetryReview')):
        return 'analysis'
    if name.startswith(('Visual', 'SceneLane', 'MilkDrop', 'Studio')):
        return 'visuals'
    if name.startswith(('Library', 'MusicScanner', 'Import')):
        return 'library'
    if name.startswith(('WavExport', 'ExportTests', 'Playlist')):
        return 'arrange' if name.startswith('PlaylistTransition') else 'export'
    if name.startswith(('Model', 'Recovery', 'Pcm', 'ContentDigest')):
        return 'data'
    if name.startswith(('Engine', 'ElasticAudio', 'OriginalAudio', 'Mastering', 'Spectrum', 'PeakMeter', 'OutputPeaks', 'PluginLatency', 'Latency', 'EffectTail', 'SourceStreaming', 'PlaybackCache', 'LongSession')):
        return 'audio'
    if name.startswith(('Tempo', 'Timeline', 'Transition', 'Beatmatch', 'Placement', 'Editing', 'TrackOwnership', 'Automation', 'AudioSlip', 'BarNavigation', 'ArrangementInsertion', 'EditSelection', 'PresetNode', 'Hotcue', 'SourceSection', 'SectionUI', 'WaveformDetailCache')):
        return 'time'
    if name.startswith(('Alloy', 'Workstation', 'Workspace', 'ToolPanel', 'Attract', 'UiClarity', 'UiMessages', 'TutorialMusic')):
        return 'ui'
    if name.startswith(('PromoControl', 'SelfTest', 'VerificationMetrics', 'FusionWorkflow')):
        return 'tooling'
    raise ValueError('Unclassified first-party file: ' + path)


def entries():
    raw = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT)
    paths = {p for p in raw.decode('utf-8').split('\0') if p and (ROOT / p).is_file()}
    paths.update('docs/release-audit/inventory/' + name for name in OUTPUTS)
    return sorted(paths)


def csv_text(rows, fields):
    stream = io.StringIO(newline='')
    writer = csv.DictWriter(stream, fieldnames=fields, lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue()


def generate():
    paths = entries()
    cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8-sig')
    compiled = set(re.findall(r'\b(?:Source|tests)/[\w./-]+\.cpp\b', cmake))
    content = {}
    includes = {}
    for path in paths:
        if Path(path).suffix.lower() in ('.h', '.cpp', '.inc', '.cmake', '.py', '.ps1', '.json', '.md', '.txt', '.cmd'):
            content[path] = (ROOT / path).read_text(encoding='utf-8-sig', errors='replace') if (ROOT / path).exists() else ''
        includes[path] = re.findall(r'^\s*#include [<"]([^>"]+)', content.get(path, ''), re.M)
    consumers = defaultdict(list)
    for path, dependencies in includes.items():
        for name in sorted({Path(dependency).name for dependency in dependencies}):
            consumers[name].append(path)
    files, controls, options = [], [], []
    site = re.compile(r'addItem\s*\(|addSubMenu\s*\(|add(Button|ComboBox|TextEditor|Slider)\s*\(|(?:TextButton|ToggleButton|ComboBox|TextEditor|Slider|AlloyDial|MidiKeyboardComponent)\b|setRange\s*\(|setButtonText\s*\(|setText\s*\(|setName\s*\(|\bon[A-Z]\w*\s*=|keyPressed\s*\(|keyStateChanged\s*\(|key\s*==|key\.(?:is|get)|mouse(?:Down|Up|Drag|WheelMove|DoubleClick)\s*\(|filesDropped\s*\(|itemDropped\s*\(|MidiActionKind::|createAccessibilityHandler\s*\(')
    for path in paths:
        group = classify(path)
        purpose, rules, method = GROUPS[group]
        is_test = bool(re.search(r'(?:Tests?\.(?:cpp|h)$|^tests/)', path))
        vendor = path.startswith('Source/ThirdParty/')
        kind = 'vendor-source/notice' if vendor else 'test/fixture' if is_test else 'document' if path.endswith('.md') else 'source/tool/config/asset'
        if path.endswith(('AuftaktFilterbank.inc', 'JuceMp3Length.inc')):
            kind = 'generated-or-derived-source; provenance required'
        if path.startswith('docs/release-audit/'):
            relation = 'audit-only; not application runtime'
        elif path in compiled:
            relation = 'compiled into app' + ('; test/research path, not ordinary user workflow' if is_test or group == 'research' else '')
        elif path.startswith('Source/'):
            relation = 'app compile/include input or notice; confirm exact package'
        elif path.startswith(('design/', 'docs/references/')):
            relation = 'non-runtime reference; exclude from package unless explicitly selected'
        else:
            relation = 'release requirement, build/test/support or provenance input; not assumed bundled'
        digest = '' if path.startswith('docs/release-audit/') else hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
        files.append(dict(path=path, sha256=digest, kind=kind, subsystem=group, purpose=purpose + ': ' + Path(path).name,
                          release_relation=relation, dependencies='; '.join(includes[path]) or 'see subsystem dependency map in COVERAGE.md',
                          consumers='; '.join(consumers[Path(path).name]) or ('CMakeLists.txt' if path in compiled else 'see COVERAGE.md / source registration / release review'),
                          rules=rules + (' MW-R09' if is_test else ''), verification=method,
                          exclusion='vendor internals not line-certified; integration/provenance still required' if vendor else 'no audit exclusion; nonshipping role stated above'))
        if path.startswith('Source/') and not is_test and not vendor and Path(path).suffix in ('.cpp', '.h'):
            for number, line in enumerate(content.get(path, '').splitlines(), 1):
                if site.search(line):
                    controls.append(dict(site=f'{path}:{number}', subsystem=group, source=line.strip(), rules='MW-U01 MW-U02 MW-U03 ' + rules,
                                         required_trace='expand dynamic instances; declaration > input > handler > state > feedback > Undo/save/output; see CONTROLS.md',
                                         status='NOT RUN'))
        for number, line in enumerate(content.get(path, '').splitlines(), 1):
            if path.startswith(('Source/', 'scripts/')):
                for flag in sorted(set(re.findall(r'(?<![\w-])--[a-z][a-z0-9-]*', line))):
                    options.append(dict(option=flag, site=f'{path}:{number}', subsystem=group,
                                        role='self-test selector' if flag.endswith('-only') else 'entry/tool/test/research; inspect producer before use',
                                        rules='MW-R02 MW-R06 MW-R09', status='NOT RUN'))
    return {
        'files.csv': csv_text(files, list(files[0])),
        'control-sites.csv': csv_text(controls, list(controls[0])),
        'cli-options.csv': csv_text(options, list(options[0])),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    generated = generate()
    errors = []
    rule_text = '\n'.join(p.read_text(encoding='utf-8-sig') for p in (HERE / 'rules').glob('*.md'))
    defined = re.findall(r'^### (MW-[A-Z]\d{2})\b', rule_text, re.M)
    if len(defined) != len(set(defined)):
        errors.append('Duplicate rule ID')
    referenced = set(re.findall(r'MW-[A-Z]\d{2}', '\n'.join(generated.values())))
    missing = referenced - set(defined)
    if missing:
        errors.append('Inventory references undefined rules: ' + ', '.join(sorted(missing)))
    status_file = HERE / 'inventory' / 'initial-status.csv'
    if status_file.exists():
        with status_file.open(encoding='utf-8', newline='') as stream:
            initial = list(csv.DictReader(stream))
        if len(initial) != len(defined) or {r['ruleId'] for r in initial} != set(defined):
            errors.append('Initial status ledger must account for every rule exactly once')
        allowed = {'NOT RUN', 'PASS', 'FAIL', 'BLOCKED', 'NOT APPLICABLE'}
        if any(r['status'] not in allowed for r in initial):
            errors.append('Invalid initial check status')
    for name, value in generated.items():
        path = HERE / 'inventory' / name
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8') != value:
                errors.append('Stale registry: ' + name)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(value, encoding='utf-8', newline='')
    for problem in errors:
        print(problem)
    counts = {name: len(list(csv.DictReader(io.StringIO(value)))) for name, value in generated.items()}
    print(json.dumps(dict(registries=counts, rules=len(defined), errors=errors), indent=2))
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())

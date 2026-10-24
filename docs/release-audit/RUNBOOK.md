# Execution runbook

This is a discovered procedure, **not authorization to execute it during framework creation**. Read AGENTS and RULES first. Use exact current local paths, not a historical path copied blindly. Source baseline is28cad13; all existing native/bundle claims are historical until independently rebound.

## Order, preflight and evidence

1. `git rev-parse --show-toplevel`, `git status --short`, `git log -1 --oneline`; read HANDOVER/STATUS. Record process identities read-only and latest user preservation instructions; do not control production. Preserve changed source/docs outside Git with exact-byte ZIP+SHA manifest and dirty patch. Keep private paths/token files under `.local/`.
2. `python docs/release-audit/refresh_inventory.py --check` and `python scripts/check-project.py`. Reconcile new scope/control/requirement changes. Choose short fresh `.local/raNN` roots to avoid the documented261-character Windows path failure; do not reuse nonempty run roots. Each result records exact command/environment/start/end and hashes.
3. Read source and existing reports, establish independent expected values and unresolved thresholds **before** running fixtures. Inventory tools/prerequisites by read-only discovery: Python3.11+ standard library (3.12 installed here), VS2022 C++/SDK/CMake≥3.22, trusted dependency pins, current executable+adjacent ONNX DLL/model, VST fixture modules, audio/physical devices and optional qualified stem runtime. Do not install providers/drivers or accept agreements automatically.
4. Run smallest relevant Python/document checks. When repairs are authorized, batch related changes, build once at useful checkpoint, run focused real workflows/native checks sequentially via run_checks. No full suite/stress/capacity/benchmark during ordinary iteration. Stop owned workload if desktop responsiveness worsens; lower priority is not a memory/disk limit.
5. Capture native/bridge state+pixels, independent audio/JSON measurements, listening and hardware/clean-package evidence required by rules. Missing oracle/hardware/threshold = visible BLOCKED; use safe independent work while pending.
6. Freeze final source/bundle/dependencies/plugins/fixtures. At an **explicit final release-verification checkpoint**, run full gate once plus mapped newer-feature checks. Resolve any outdated assertion against current requirement with reproduction; preserve original failure. Rebuild after fixes invalidates exact-binary evidence. Reuse unchanged-failed-suite environmental retry only with retained proof and provenance.
7. Qualify exact local release package in clean environment. Reconcile all statuses/defects/ambiguities, emit honest release report with counts and residual uncertainty. Public publication and rights clearance are separate. Record exact next step in STATUS/HANDOVER.

Per-run layout (not committed): `source/` ZIP+manifest+patch; `identity.json`; `requirements.json`; `results/<rule>.json`; `controls.csv`; `fixtures/manifest.json` + independent reference scripts/annotations; `native/<suite>/`; `ui/` captures+review notes; `audio/` compact metrics; `defects.json`; `suspicions.json`; `report.md`; `retention/`. Use fresh numbered attempts, not overwritten PASS files. Every evidence file gets SHA256. Preserve compact reference truth; generated bulk audio/session fixtures cleaned by run_checks retention unless explicitly needed for subsequent measurement, with exception reason/owner/expiry recorded.

## Build without violating the responsiveness override

`scripts/build.ps1` currently fetches/configures and builds `--parallel 3`; CMake enables MSVC `/MP`. Do not run that default for this audit. Inspect/prepare already-authorized dependencies using build/fetch pins, then use an ordinary below-normal child with one MSBuild worker and `/MP1`. No overlapping build/test. The following illustrates a scheduled **build**, not a command to run in discovery:

```python
# Save as a local owned driver; launch only at a scheduled build checkpoint.
import os, subprocess
from pathlib import Path
env = dict(os.environ)
env['_CL_'] = (env.get('_CL_', '') + ' /MP1').strip()
flags = subprocess.BELOW_NORMAL_PRIORITY_CLASS  # Windows qualification host
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
command = ['cmake', '--build', 'build', '--config', 'Release', '--target',
           'Mixworks', 'MixworksTestGain_VST3', 'MixworksTestDelay_VST3',
           'MixworksTestLatency_VST3', '--parallel', '1']
# If regeneration is needed, run a separate sequential child with the same
# env/flags: cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
# -DMIXWORKS_BUILD_TEST_PLUGIN=ON ; inspect its result before the build.
with Path('.local/ra-build.log').open('x', encoding='utf-8') as log:
    child = subprocess.Popen(command, env=env, creationflags=flags,
                             startupinfo=startup, stdout=log, stderr=log)
    print(child.pid, flush=True)
    # The driver may wait; the agent polls its tool session in <=60s intervals
    # and communicates progress. No tests until the owned child exits.
    result = child.wait()
raise SystemExit(result)
```

Choose a fresh log/driver name each attempt; capture PID and final exit code, not merely process creation. If stopping is necessary, identify and stop only that owned build child tree and retain logs. Discover output from `build/mixworks-Release.path`; do not assume the older detection-Release path still applies. The driver sets below-normal priority at creation, avoiding a race from assigning priority after a child has started.

## Focused native checks (actual harness interface)

Write a short driver file or execute this from repo root; replace the candidate/root/selector only after source/fixture review. Parent `.local/ra01` must exist, suite child must be new. No direct ordinary `exe --self-test` bypass of retention:

```python
import sys
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from verification import run_checks, write_json
exe = Path('out/releases/0.55.64-7cb413d25c19/Mixworks.exe').resolve()  # revalidate
root = Path('.local/ra01')  # create a fresh parent explicitly
root.mkdir(exist_ok=False)
result = run_checks(exe, ['--targeted-analysis-only'], root / 'retry', timeout=900)
write_json(root / 'retry-result.json', result)
assert result['exit'] == 0 and result['complete'] and not result['errors']
assert not result.get('artifactRetention', {}).get('errors')
```

Use actual source selectors; list of **22 named gate suites** from verification.py: rhythm-reliability, beat-grid, placement, tempo, tempo-browser, track-ownership, analysis-refresh, grid-correction, rhythm-decoder, library-assets, playback-cache, source-streaming, timeline-editing, alloy, window, fusion-workflows, mastering, export, latency, core, missing-model, corrupt-model. Core has no `--core-only`; missing/corrupt use `--neural-unavailable-only` with deliberately different owned bundles.

The current SelfTest has additional early-return selectors, inventoried in CLI CSV. Explicitly map/include performance-take, targeted-analysis, import-scan, grid-attack, cache-settings, independent-timeline, unlink, edit-selection, library-preview, zoom-handles, transition-swaps, audio-ui, markers, stems, live-hotcues, promo-control, musical-energy, auto-arrange, native-picker, long-navigation/long-session/long-open, analysis-cache, real-tracks, constant-four-four, split-effects, preset-nodes, cache-reopen and others only where present. Inspect exact dispatch and nested run calls; no claim all are absent from core/alloy or all needed as duplicate runs. Deduplicate by actual executed checks and required paths, not selector name alone. Performance-take currently accepts `--plugin <finite-delay-VST3>`; verify source expectation before selecting plugin flags. Never invent unsupported `--section-only` or `--midi-only` flags.

Python harness examples (select relevant tests, do not run all by habit):

```powershell
python -m unittest discover -s tests -p test_alloy_bridge.py -v
python -m unittest discover -s tests -p test_verification.py -v
python -m unittest discover -s tests -p test_test_artifacts.py -v
python -m unittest discover -s tests -p test_stem_worker.py -v
python -m unittest discover -s tests -p test_downbeat_references.py -v
python -m unittest discover -s tests -p test_change_verification.py -v
```

These commands are not interchangeable with product runtime acceptance. Some harnesses may spawn children; read current test before scheduling under the one-process policy. Confirm exit/footer/check count/errors/retention independently; a shell exit0 by itself is insufficient.

## Bridge and actual UI

Read [DEVELOPMENT-BRIDGE](../DEVELOPMENT-BRIDGE.md) in full before use. Its quick-start `--parallel 3` build example is superseded by AGENTS. Use a trusted current candidate, fresh short root; `start` copies exe and shares matching canonical model/runtime assets:

```powershell
python scripts/alloy_bridge.py init .local/ra-b01
python scripts/alloy_bridge.py start .local/ra-b01 --exe "out/releases/0.55.64-7cb413d25c19/Mixworks.exe"
python scripts/alloy_bridge.py call .local/ra-b01 status
```

Assert protocol1/root/PID/version/capabilities, disabled physical MIDI and isolated analysis/PCM/catalogue/workspace/profile paths. The bridge still opens an audio device; do not change production routing or assume silent monitor status without checking. Place owned procedural audio under root/audio; use Client.ready after async import/add/open **and assert named outcome**. Save a new name, screenshot a new name, inspect pixels, save before `close`; no force-close or production attach. Arguments via UTF8 JSON file avoid quoting errors. Timed-out mutation: inspect response/claim/child/status before any decision; no automatic retry.

For tooling changes `python scripts/verify-bridge.py --exe <trusted-candidate> --output .local/ra-bv01` runs native and live suites through retention; inspect actual captures. It is supplemental, not an accepted full receipt. Move commands don't test drag gestures; timing/probe helper output is not independent detector truth. Each audio_probe starts fresh history; it cannot prove continuous effect tails. Native Workstation screenshots omit OS chrome/file dialogs, external VST content and physical routing; obtain actual desktop/manual evidence for those.

## Fixture profiles and independent oracles

All profiles need a fixture manifest with generator/reference code, exact sample positions/rate/channels/amplitudes/source SHA and expected-state table. Generate only small current needs during iteration. No user music overwritten/copied into Git; real-source corpus is licensed/local and paths remain private.

| Profile | Construction and independent truth |
| --- | --- |
| F-UI | Empty, loading, valid, invalid, cancelled/recovered; conflicting selections, dirty/clean, locked/unlocked, prepared/pending, stopped/playing/export/private/capture. Primary control values exhaustive; pairwise other dimensions plus named unsafe intersections. Sizes1250/1440/1920/2560 where legal,100/125/150/200% scaling; single/multiple monitors. Check all actual min/max accepted bounds rather than force unsupported size. |
| F-TIME | Known impulse/low-tone attack sample indices at authored BPM40/60/90/120/128/150/200/300; accents on independently marked modulo4 bar positions, late downbeat/pickup/silence, source trims/loops/reverse, constant/ascending/descending piecewise-linear BPM map. Independent Python integral `beats=integral(BPM(t)/60 dt)` and inverse root/quadratic solve; never call TempoMap to compute expected answer. One sample is1/rate seconds; pixel rounding independent transform. |
| F-AUDIO | Silence, DC within supported path, L=.25/R=−.125 tagged impulses, isolated sine/log sweeps, stereo/mono/antiphase, finite extremes and intentional nonfinite test processor; independent channel/gain/pan/mid-side equations, known finite-delay taps125/250/375ms, sample-stamped envelope. Blocks1/63/127/511/512/1024 where accepted; rates44.1/48/96k and source-rate partitions in L01. |
| F-MEDIA | Authored lossless encodings with exact PCM/frame truth; MP3 CBR/VBR Xing/VBRI/no-header/short/truncated, independent qualified decoder with documented priming/padding treatment; Unicode, spaces, long paths around supported boundary, read-only/missing/reparse/hardlink canaries only under owned root. |
| F-GRID | Unambiguous authored tempo/bar fixtures above plus weak/broad attacks, upbeat pickup, syncopation, octave ambiguity, noise/silence and excluded variable-tempo/non4/4. Normal analysis only with empty isolated stores; no manual BPM/downbeat applied before score. Differentiate onset detector resolution from mathematical sample-time precision. |
| F-CORPUS | Source-bound independently reviewed constant4/4 material across tempo, density, intro silence/pickup/weak kick and genre; holdout material separated from tuning. Start with preserved dated DownbeatReferences and existing tests JSON/research; compare SHA and reviewer disagreement, don't overwrite earlier reviews or treat manual choices as algorithm truth. Proposed≥20 tracks/≥3 annotated passages each for initial qualification, **provisional**, final count/accuracy/coverage thresholds REQ-02. Report all failures/abstentions, not selected successes. |
| F-PLUGIN | Native TestGain/TestDelay/TestLatency modules built from tests/*.cpp with known analytic behavior; actual qualified instrument (gate currently requires installed Vital) recorded by module hash. Independent delay/gain response, latency advertised values and state. No provider installation/crashing plugin experiments without explicit scope. |
| F-MIDI | Hand-authored expected physical action table, raw controller messages and stable action IDs; all finite actions, button press/held/release, malformed and conflicting profiles, physical FLkey separately. Preserve user's saved map; profiles inside owned stores. |
| F-TAKE | Authored ordered control/transport/seek/cue/structural events on absolute integer performance frames, deterministic delay tail and pause, original arrangement independent copy; direct VST gestures included. Compare sequential live/replay/seek reconstruction/output, limits via constructed bounded event data. |
| F-DATA | Current format41 document containing every field/owner plus malformed one-field-at-a-time cases/duplicate references/truncations/unsupported versions. Expected state hand-authored, canonicalized semantic comparisons and independent source/audio validation. Permission/disk/flush failures through fault fixture, no filling user's drive. |
| F-STEM | Tiny known four-file stem set with tagged samples for routing independent of expensive inference; separate optional installed-runtime integration with normal/quiet/mono/silence/antiphase inputs and source/recombined truth. Sparse impulse separation refusal is an expected quality guard partition, not timeline mixer failure. |
| F-ARRANGE | Small controlled library≤6 sources for exhaustive feasible-order validation, contradictory activity versus loudness, known key/BPM/section/genre, seeded randomness; larger count/duration fixtures only at authorized checkpoint. Constraints independent from planner implementation; musical taste evaluated by listening. |

### Numerical rules and unresolved thresholds

- Integer frames/channel counts/source hashes/event ordering/identity/untouched bytes: exact. Nonstretched scheduling/quantized launches:≤1 output sample due to half-open interval rounding, no cumulative drift. Ordinary tempo inverse≤1e-7s, extreme valid points≤0.5sample; derives from coordinate precision needed below audio sample resolution.
- Deterministic shared float renderer max residual≤2e-6 full scale (existing finite-delay/export test guard), PCM24 comparison adds≤1 signed24-bit quantization step (`2^-23` under normalized signed representation). Silence exact on intentionally gated digital paths. Report max and RMS; RMS alone can hide one lost attack.
- Existing moved/seek attack≤25ms, general ramp fixture≤50ms are historical technical limits, not settled musical promises; N02 synthetic proposal≤0.1%BPM/p95≤10ms/max≤20ms is **provisional**. Sustained drift must be measured independently. REQ-02 must settle release corpus coverage/accuracy and permissible worst-tail error before related general-accuracy PASS.
- EQ response0.1dB at settled chosen frequencies and meter0.1dB display are measurement/quantization proposals; FFT power normalization needs analytic Hann/bin test, not arbitrary image matching. Fidelity/pitch/alias/click/listening tolerances REQ-03 unresolved. Choose before seeing candidate measurements.
- Stem recombination≤15% relative RMS, near-silent residual≤1e-7, alignment≤1sample, exact dimensions are predeclared current guard, not isolation/SDR acceptance. Source downbeat class accuracy separate from pulse accuracy; both are separate from first audible kick.
- Only Escape appearance restoration has≤1 changed pixel/RGB≤3/255, equal alpha/dimensions per specific user authorization; don't generalize to other visual/audio/state checks. Layout lane partition≤1pixel from integer partition requirement.

## Workload, physical and listening qualification

Proposed workload partitions, not advertised capacity promises: **W-small**2 tracks/2min synthetic mix; **W-typical**10 placed songs/roughly40min,4 lanes with overlaps,1 deterministic insert per lane plus master, ~100 automation points and metadata library400 entries; **W-large**40 placed sources/3h plan, metadata1000 entries and existing90min source fixture. Record decoded/resident/cache sizes separately; library entry count does not equal resident audio. Freeze exact source/module/map/count inputs per run. Proposed measurement durations: one cold and one warm launch for investigation; final3 sequential timing samples;30min programme/edit/listening,60min resource observation/20 open-close cycles when explicitly authorized. REQ-05 must approve target CPU/RAM/GPU/storage/Windows/backend/rate/block and wall-time/RSS/disk/cancel bounds first. Hard audio budget=frames/rate; missed callback/driver evidence has its own pass criteria. No speed promise from an old machine-specific assertion such as twice-real-time export.

Physical sequence: record audio-device/backend/driver/rate/buffer/channel and controller model/firmware/port; use safe listening level and independent programme/private recording, verify no bleed, then real faders/pots/pads/notes and reconnect. Do not remap/reprogram hardware, seize production devices, install drivers or restart user apps. Arrange a dedicated test window/device when required; unavailable hardware stays BLOCKED.

Listening sequence: freeze output/source hashes and rubric before playback; source/reference and result at matched level on declared headphones/monitors; check full representative output plus annotated joins/attacks/tails/cues/stems. Record exact intervals, apparent defect, repeatability and listener confidence. Cross-check sample/phase for flams and discontinuities; do not equate high confidence, low RMS error or a successful test suite with musical quality.

## Final gate and local package — conditional commands

Only when the user explicitly requests a final release-verification checkpoint including the guarded stress/capacity work:

```powershell
python scripts/verify-analysis.py --exe "<frozen-bundle>/Mixworks.exe" --output .local/ra-final01 --receipt .local/ra-final01/receipt.json --include-stress
```

This command requires built gain/delay/latency VSTs, installed Vital, substantial disk headroom and the90min capacity fixture; it runs sequentially, including missing/corrupt-model variants. `--receipt` requires `--include-stress`; do not drop stress and call the result complete. Existing22-suite gate may contain superseded defaults; inspect independently and supplement current new-feature paths. Never weaken receipt validators or reuse another executable's results to force acceptance. Full gate is necessary at final checkpoint, insufficient for hardware/listening/package/rights/whole-audit PASS.

Read deploy.ps1 and current receipt schema before local staging. Its `-VerificationManifest <receipt> -VerifyOnly` validates a candidate; actual staging is separate, retains launcher/bundle backups and never starts the app. Scoped receipt is valid only for its stated allowlist/scope and accepted full baseline; it is not a fresh full regression. This framework makes no deployment request. Public binaries remain blocked by R10 even after technical checks.

## Failure and report procedure

Record original failure immutably, minimize owned reproduction, compare independent requirement/oracle, classify confirmed defect versus suspicion versus requirement gap. Future authorized local fixes include regression reproduction and proportionate verification; current discovery includes no app fixes. Use [templates/result.json](templates/result.json) per rule/candidate. A BLOCKED subcase keeps whole rule BLOCKED, a confirmed failed subcase FAIL; partial passing cases never upgrade whole rule. Final report lists each status, unresolved defect/ambiguity, environment/fixture coverage and package/source identity, with public distribution clearance separately stated. Preserve source/evidence independently of commits and update exact continuation in STATUS/HANDOVER.

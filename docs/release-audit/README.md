# Alloy release audit — canonical index

Created 24 October 2026. **Audit execution has started; Alloy has not passed this audit.** See [STATUS](STATUS.md) for the current run, repairs and continuation. The discovery baseline below remains historical.

Source baseline: commit `28cad136131856a01e62ad86b6a1e0c245232ba0`, project `0.55.64`, analysis revision **11**, session format **41**, focused retry revision **3**. This folder supersedes no historical evidence; it is the canonical index for future release audits. Read [STATUS](STATUS.md) before execution.

## Start and authority

1. Read the root [AGENTS](../../AGENTS.md), current [HANDOVER](../../HANDOVER.md), [testing policy](../TESTING.md) and [development bridge](../DEVELOPMENT-BRIDGE.md).
2. Read [RULES](RULES.md), the subsystem catalogues it links, [COVERAGE](COVERAGE.md), [CONTROLS](CONTROLS.md), and [RUNBOOK](RUNBOOK.md).
3. Use [RUN_AUDIT_PROMPT](RUN_AUDIT_PROMPT.md) to start execution in a later session. Its authorization does not apply merely because this framework exists.

The requested target is a candidate with every applicable mandatory rule evidenced, no known unresolved in-scope defects at any severity and no unresolved release-critical requirement ambiguity. A technically qualified local candidate and a publicly distributable package are distinct decisions. Do not weaken the target to fit existing test receipts.

Current user instructions outrank historical plans. Current workflows are in scope; old-session compatibility is explicitly unnecessary. Older parser branches are still attack/error-handling surfaces, but no migration matrix or compatibility layer is required. Constant-tempo 4/4 source detection is the priority. Project tempo ramps are implemented and remain in scope; variable-tempo source detection and other meters are not promised.

## Actual architecture and boundaries

| Boundary | Discovered implementation and audit implication |
| --- | --- |
| Application | Native C++17/JUCE GUI, CMake C/C++, MSVC Windows x64 build. `Main.cpp` dispatches GUI, session/audio arguments, native self-tests, analysis/recovery/research CLIs and isolated dev/promo mode. Multiple instances allowed. No browser UI, hosted web backend, account service or application updater found. |
| State | `Model.*` owns Project/Clip, tempo conversion, source geometry, transition links, automation, stable original-placement/effect owners, takes and JSON parsing. `Workstation.*` and UI translation units own selection, commands, Undo and job admission. Workspace preferences are outside musical Undo. |
| Audio | `Engine.*` owns JUCE device callback, four lanes, clip/original-track processors, lane VST3/instrument chains, MIDI, master DSP and render state. Callback uses a try-lock and explicit silence/diagnostic paths, not proof of deadline safety. SoundTouch and elastic/cache paths must be profiled separately from offline DSP. |
| Background ownership | Workstation has separate one-thread audio, ordinary and recovery pools; library preview has one loader plus read-ahead; scanner and stem jobs publish progress; export/private/take preparation clone renderers. ONNX defaults to three intra-op threads (one with `MIXWORKS_ANALYSIS_THREADS=1`) and one inter-op thread, serialized by a timed mutex. Optional stem work runs an owned Python child with Torch/CUDA or CPU. Master WAV uses a low-priority TimeSliceThread and bounded writer FIFO. |
| Analysis | PCM decode → waveform/physical rhythm evidence → local Beat This! ONNX (22,050 Hz model, 30s contexts) → decoder/confidence and beat/bar interpretation; key/tuning, activity/energy and editable sections have distinct consumers. Focused retry uses bounded passages and SoundTouch BPMDetect; it is not the normal detector. `Auftakt*`/research tooling are separate recovered experiments, requiring provenance rather than assumed shipping algorithm status. |
| Persistence | `.mixworks` JSON v41, analysis JSON rev11 with source SHA/dimensions and manual overrides, PCM mapped stores, library snapshots/catalogue, source sections/hotcues, preset/profile JSON, workspace v1, per-instance recovery, take events and external stem manifests/WAVs. Source/caches have separate compatibility identities; a version number alone does not prove interchangeability. |
| Output | Shared renderer supports live playback, bounded effect tails, rehearsal and private preparation, offline stereo 24-bit WAV/RF64, editable performance replay/take export and optional live master WAV. Monitor mute is device-only; private headphone audio must not enter programme/export. Seeking a normal arrangement resets effect history; take seek explicitly reconstructs history. |
| Integrations | JUCE audio-device APIs including compiled Windows ASIO, MIDI/FLkey Learn/profiles, VST3 (VST2 disabled), WIC/artwork and OS file/dialog/display facilities. Nonzero-reported-latency active VSTs are blocked; full compensation and crash sandboxing are not implemented. Local file-inbox bridge is authenticated, opt-in development tooling, not remote control over a network. |

## Product/release envelope

Shipping/compiled surfaces to qualify: library scan/import/search/sort/preview/retry, four-lane timeline and range editing, explicit transitions/Auto Arrange, tempo maps and source corrections, source sections/HC1–HC9, stem separation and timeline stem controls, automation/desk/MIDI/VST3/mastering, save/relink/recovery, Arrange and Record/Perform/private preparation/takes/live master WAV, export/tracklists, layouts/displays/help/lighting, CLI and dev bridge.

Known limits are contracts to disclose, not automatic defects: no editable instrument-note recording, opaque/nondeterministic plugin changes may require master WAV; no independent stem EQ/VST or live stem automation writing; no full VST latency compensation/crash isolation; no built-in synth, general skin system, phone controller, compressed-audio export or variable-meter source grid commitment. Roadmap absence is not a licence to add these. Disabled/reserved controls must clearly explain their limits and have no hidden mutation.

Inputs offered by actual chooser/CLI: WAV, AIF/AIFF, FLAC, OGG, MP3; M3U/M3U8 playlist paths (nested playlists rejected). Outputs: stereo PCM24 WAV/RF64, M3U and text tracklist/labels plus sessions/presets/profiles. No installer, CI workflow or committed general dependency lockfile was found. `scripts/analysis-dependencies.json`, build/fetch pins and stem setup/config qualification provide partial dependency identity. Windows is the only evidenced release target; generic JUCE portability does not establish Linux/macOS support. Declare precise Windows versions/devices and package form before claiming release support (REQ-01).

## Discovery environment and limits

Repository root verified as `C:/Users/jayte/Desktop/REA/Mixworks`, remote `Jaytecit/Mixworks-Alloy`, existing branch `codex/beta-finishing-p01`. At discovery start 226 changed/new files were pending against `ef4fab6`; the user then requested a checkpoint, committed as `28cad13`. Independent exact-byte archive/manifest and original dirty status are in `.local/release-audit-20261024/pre-audit-checkpoint/`. This task changes documentation and its inventory helpers only.

Observed host reports Windows NT `10.0.26200.0`; Python 3.12, CMake and rg are available. The normal launcher text selects `out/releases/0.55.64-7cb413d25c19/Mixworks.exe`; the retained focused report records 12 retry + 51 take + 22 preview checks, not a fresh audit pass. Historical binary hash is `7cb413d25c190c4fd40ced8d7cf9998a6379148f107b8cf9c6602d44bc013fc5`. Neither current source/binary equivalence nor hardware availability was freshly verified. Do not attach to or restart a production instance.

Repository bridge/client and native harness are preferred for supported inspection/actions. Desktop UI automation is available but needs its own skill and current surface discovery; bridge screenshots omit OS chrome/popups. Hardware listening, physical MIDI/ASIO, installer testing and Windows accessibility require separate evidence. No external tool/service, runtime installation or subagent is necessary for framework discovery. [skills.sh](https://skills.sh/) was consulted as instructed; its general browser/review workflows do not replace the repository's native harness or bridge. The local verification-before-completion skill informs evidence claims.

## Inventory and maintenance

- [File inventory](inventory/files.csv): one row per tracked/nonignored first-party repository item, including documentation, test data, scripts and artwork; vendor rows classified separately.
- [Control source sites](inventory/control-sites.csv): lexical discovery of declarations, menu construction, value ranges, callbacks, keyboard/mouse/drag accessibility and MIDI sites. These are audit obligations, **not a runtime-certified count of distinct controls**.
- [CLI inventory](inventory/cli-options.csv): option strings with producer locations, including tests and experiments.
- [Dependency ledger](DEPENDENCIES.md): pinned inputs, generated/derivative assets, package clearance and local-only material.
- [Discovery findings and unresolved requirements](STATUS.md); older audits remain linked from there.

Run `python docs/release-audit/refresh_inventory.py` after adding/removing files or changing controls; review its diff and update semantic control families and rule mappings. `--check` verifies current membership, hashes, links between registries/rules and control extraction without launching Alloy. Inventory hashes describe working bytes, not normalized Git blob hashes. Framework self-files are membership-covered but deliberately unhashed to avoid self-reference. Inventory membership is exhaustive for nonignored repository files; ignored `.local/build/out` trees require candidate-specific bundle/runtime manifests, not a recursive vendor-source review. Refreshing an inventory never counts as reviewing those files or passing a product rule.

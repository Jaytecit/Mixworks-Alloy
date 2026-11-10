> Historical snapshot preserved on 4 November 2026. Completed tasks and old next steps below are inactive. Use [the outstanding roadmap](../../../FINISHING-PLAN.md) and [current handover](../../../HANDOVER.md).

> Current status,24 October 2026: EP02 selection, rigid groups, Repeat/Crop/Replace, local separation and timeline stem mixing are implemented to the scope in the [request register](../../REQUEST-STATUS-20261024.md). EP03 protection/node groups/partial transition surgery/changing-tempo retiming, EP04–06 advanced snippets/effect regions, advanced EP08 extraction and EP09 complete acceptance remain. Checklists below retain design/acceptance criteria and dated incremental history, not current completion labels.

# DJ.Studio Editing Parity Implementation Plan

> **For agentic workers:** implement one checked increment at a time, inline, preserving existing work. This product/architecture plan identifies implementation owners and acceptance checks; it is not an untested replacement patch. Do not mark a checkbox complete on the strength of this plan.

**Goal:** bring Alloy's editing in line with the evidenced DJ.Studio transition, audio-range, sample, effect and stem workflows while preserving its source-clock, automation and controller foundations.

**Architecture:** extend Project/Clip and existing command/admission/render paths. Keep selection/view state outside the saved musical document; commit candidate edits atomically with one Undo. Use ordinary automation for envelopes; add owners for genuine new concepts such as effect instances, snippets and separated assets.

**Tech Stack:** C++/JUCE, native suites, Python verification/bridge tooling and versioned JSON. Select a stem backend after measured feasibility, not by copying names from a competitor bundle.

**Baseline:** [22 October audit](../../EDITING-SYSTEM-AUDIT-20261022.md), Alloy0.55.42, analysis11/session34/preset3. Source, reports and two retained captures were inspected; no new runtime/listening/hardware acceptance is claimed.

**0.55.43 implementation checkpoint —22 October:** initial EP02 selection and EP03 clipboard/delete/paste slice delivered; see [range guide](../../RANGE-EDITING.md) and current handover for verification. Whole EP02/EP03 remain partial: group dragging, full native modifier-key acceptance, node groups, protected ranges, partial linked-transition splicing and Repeat/Crop/Replace are not complete. No effects/stem work or detector reset in this increment.

**0.55.54 group-movement increment —23 October:** rigid group dragging adds destination outlines, release-time admission, Escape cancellation and one Undo. Complete linked groups retain IDs, relative geometry and source timing. Partial linked groups, locked/unavailable sources, occupied destinations, lane overflow and incompatible captured tempo shapes refuse. Project tempo/session automation stay fixed. Changing-tempo group retiming, node groups, protection and remaining EP03 commands stay open. See HANDOVER for focused verification and deferred coverage under the responsiveness override.

## Sequence and existing owners

**0.55.55 Repeat/Crop increment —23 October:** Repeat previews a bounded additional-copy count and occupied destinations, retaining selected span/lanes/source timing, independent effects/IDs and complete internal links. Crop retains the selected interval on selected sources. Both preserve clipboard/playhead and commit with one Undo; empty crops, partial linked-transition trims and incompatible repeated tempo shapes refuse. See HANDOVER for essential-only checks and deferred coverage. EP03 remains partial: Replace, transition surgery, node groups and protection are still open.

**0.55.57 Replace increment —24 October:** Replace previews clipboard insertion into selected destination segments/range, keeps exterior slices and unselected audio, and commits with one Undo. Clipboard geometry must fit the selected time span/lanes with matching source timing; shorter content leaves a gap. Complete pairs retain internal links; partial linked replacements refuse. Clipboard/playhead and project automation/clocks are preserved. See HANDOVER for focused verification status. Transition splicing, node groups, protection and changing-tempo retiming remain open.

| Increment | Outcome | Dependency | Existing owner | Size |
| --- | --- | --- | --- | --- |
| EP01 | Current instructions and acceptance ledger | Audit | P06/P50/P52/P55, H01/H02/H04 | Small |
| EP02 | Range and multi-object selection | EP01 | P12 | Medium |
| EP03 | Safe range/group edits and protection | EP02 | P12 | Large |
| EP04 | Snippets and sample child lanes | EP03 | P13/P29 | Medium–large |
| EP05 | Movable effect regions and one real processor | EP02/EP03 | P13/P16 | Large |
| EP06 | Effect families/presets and pair tempo policies | EP05/current clocks | P16/P21/P22 | Large; several slices |
| EP07 | Measured stem backend and asset jobs | Source/cache owners; EP04 for integration | P28 | Large |
| EP08 | Stem transitions, sublanes and extraction | EP03/EP04/EP05/EP07 | P28/P13 | Large |
| EP09 | Complete journey and milestone acceptance | Accepted preceding scope | P38/P39, H01/H02/H04/H05 | Medium |

This refines existing items, without declaring unrelated finishing work complete or automatically reordering private Performance delivery. **EP01–EP04 are the recommended next editing sequence.** Sizes are relative scope, not dates. Variable-tempo source detection, other meters, a new UI framework and the full manual are not prerequisites.

## Shared invariants

- Preserve original audio, analysis/corrections, arrangements, library/cache/recovery, MIDI/workspace profiles and protected downbeat references. Editing updates do not justify a detector reset.
- Distinguish absolute source samples/seconds, source-elapsed curve time, programme time and beats. Reuse Clip mapping through trim, rate, captured tempo maps, slip, reverse, loops and preroll; displayed bar numbers are not a storage clock.
- One admitted edit means one Undo/Redo transaction. Reject/cancel/stale preview leaves project/dirty/source state unchanged. Revalidate document revision, IDs, locks, source identity, preparation/export and captured writes before commit.
- Build candidates before publication. Prepare audio/clone plugins outside the callback; keep callback work bounded and retire resources safely. Playback/export consume the same accepted edit.
- Preserve shortcuts: Shift-drag already slips audio; Alt bypasses Snap. Add an explicit Range tool and exact range fields before assigning additional gestures. Don't copy5.0 defaults silently.
- Ordinary Delete/Cut leaves a gap. Ripple is explicit. Keep point deletion explicit; don't adopt automatic point-overlap deletion from the reference.

## EP01 — Current instructions and baseline journey

**Files:** `docs/TIMELINE-EDITING.md`, `docs/TRANSITION-PRESETS.md`, `docs/PRESET-NODES.md`, `docs/README.md`, `FINISHING-PLAN.md`, `HANDOVER.md`. Proven interaction repairs use `Source/AutomationUI.cpp`, `Source/TimelineAutomation.cpp`, `Source/TransitionUI.cpp` and existing timeline tests.

- [ ] Consolidate current instructions for empty curves/single-node Add, ordinary preset replacement, None,1600 px/s, presets and source ownership. Mark older procedural descriptions as historical/compatibility material.
- [ ] Record Add/select/bend/move/delete/amount/lock/Undo/Redo/save/reopen for Volume/Bass/Mid/Treble/cutoff and a real VST parameter, with both manual and preset curves.
- [ ] Exercise a three-source arrangement at1250/1920 widths, collapsed panels and detached Editing. Selection must not seek, change audio/Read/recording or create history. Repair demonstrated failures without replacing working controls.
- [ ] Retain captures and gesture notes distinguishing native fixture, bridge command, actual input, listening and physical MIDI evidence.

**Exit/checks:** current instructions agree with the candidate; each journey has a recorded outcome/limitation. Prose uses document checks. Interaction repairs use focused suites and gestures; audio/persistence changes use the full gate.

## EP02 — Selection as an editor concept

**Modify:** `Source/Workstation.h`, `Source/Workstation.cpp`, `Source/TimelineEditing.cpp`, `Source/TimelineAutomation.cpp`, `Source/AutomationUI.cpp`, `Source/TimelineHeader.cpp`.
**Create at implementation:** `Source/EditSelection.h`, `Source/EditSelection.cpp`, `Source/EditSelectionTests.cpp`; register in `CMakeLists.txt` and `Source/SelfTest.cpp` dispatch.

- [ ] Add transient selection kinds: segment set, audio range on named segments, or nodes in one AutomationAddress. Store stable IDs, active owner and half-open project interval; don't retain vector indices across changes.
- [ ] Add Range-tool drag, exact start/end/length, context-specific Select all and Ctrl-click segment toggling. Focused node-editor Delete/arrows retain their current meaning.
- [ ] Paint endpoints/scope without seeking. Offer explicit Beat/Bar/8/16/32-bar range sizes through the existing4/4 clock, Snap and Alt bypass. Don't infer semantic phrases from zoom or shading.
- [ ] Preserve slip, body/I/O/frame drags and node/amount targets. Escape cancels. Prune stale selection on removal/Open/Undo while retaining valid IDs.
- [ ] Provide keyboard selection, native owner/range names and offscreen reveal. Start node selection in one exact curve, avoiding mixed parameter units.

**Checks:** backwards/zero-length drag, exact edge/preroll, offscreen ends, coincident clips, removed/stale owner, locked-object selection and Escape. Assert unchanged project JSON/dirty/history/transport after selection. Focused native timeline plus actual input/captures; new source membership follows full-gate policy.

**Exit:** directly select8 bars or three segments, identify owners and cancel without changing audio/document.

## EP03 — Atomic range/group edits and protection

**Modify:** `Source/Model.h`, `Source/Model.cpp`, `Source/TimelineEditing.cpp`, `Source/Workstation.cpp`, `Source/AutomationUI.cpp`, `Source/TimelineAutomation.cpp`, `Source/Engine.cpp`, `Source/EditingTests.cpp`, `Source/TimelineEditingTests.cpp`, `Source/TrackOwnershipTests.cpp`.
**Create:** `Source/ArrangementEdits.h`, `Source/ArrangementEdits.cpp`, `Source/ArrangementEditTests.cpp`; register in CMake/test dispatch. Clip still owns source slicing.

- [ ] Replace the single-Clip clipboard with a fragment: relative starts/lanes, source IDs, sliced automation/effect state and internal links. Clone mutable plugins at paste, share immutable media, and exclude/report external links without dangling IDs.
- [ ] Deliver Copy range/Paste first, then gap-preserving Cut/Delete/Crop, Duplicate/Repeat count and explicit Insert/Replace. Preview occupied destinations; ordinary Paste refuses collisions until explicit overlay lane or Replace. Replace touches selected destination segments only, without implicitly replacing lane/master automation.
- [ ] Move a group with one snapped displacement from its original alignment. Refuse lane overflow/invalid spans. Copies get fresh IDs; retain paired links only with both participants and required frame audio.
- [ ] Extend explicit all-lanes ripple through shared slicing/mapping. Reflow tempo/labels/session automation while preserving outside bends/clock values. Preview protected/locked affected objects; reject before visible mutation.
- [ ] Add saved protected-range IDs with absolute programme intervals. Protect interval timing and intersecting source content against cut/replace/ripple/slip/tempo changes. Viewing remains allowed; removing protection is explicit/undoable. Bump format only when persistent protection lands.
- [ ] Add node-group move/copy for one owner with bounded values and step ordering. Refuse same-time/crossing conflicts instead of silently merging/deleting. This is an Alloy enhancement, not proven reference node-copy parity.
- [ ] Broaden global lock refusal only when candidate comparison proves unchanged locked geometry/clocks/curves/rendered content. Don't merely remove `hasLockedTransitions()` checks.

**Checks:** independently computed slice/concatenation PCM for forward/reverse/loop/slip/rate mapping; simultaneous other-lane audio; ramps/source phase; pair crossing a range; edge/equal-time step nodes; plugin clone failure; stale preview; unavailable source; occupied destinations; locked/protected material. Require one Undo/Redo, real Save/Open and unchanged original hashes. Focused editing/ownership/timeline/tempo/export plus full gate on the final candidate.

**Exit:** copy8 bars without pre-splitting, repeat four times, move a group, delete with/without ripple and restore via Undo/reopen. Listen to boundaries; any anti-click processing must be explicit/tested.

## EP04 — Reusable snippets and sample child lanes

**Modify:** `Source/Model.h`, `Source/Model.cpp`, `Source/LibraryAssets.h`, `Source/LibraryAssets.cpp`, `Source/LibraryWorkflows.cpp`, `Source/TimelinePlacement.cpp`, `Source/Workstation.h`, `Source/Workstation.cpp`, `Source/Engine.h`, `Source/Engine.cpp`, `Source/WavExport.cpp`, `Source/WorkspaceUI.cpp`.
**Create:** `Source/Snippets.h`, `Source/Snippets.cpp`, `Source/SnippetUI.cpp`, `Source/SnippetTests.cpp`; register them.

- [ ] Save a range as a source-linked named snippet with hash/span, grid anchor and explicitly included treatment. Portable export separately packages permitted media or reports unresolved references.
- [ ] Add sample child lanes under an existing mixer lane: visual/editing rows and internal voices, without new FLkey/mixer indices. Don't globally enlarge `trackCount` to create visual room.
- [ ] Support drag placement, trim, free/sub-beat positioning, Repeat count and loop extension via Clip semantics. Show voice/collision limits and refusal.
- [ ] Reuse ordinary automation and independently owned effects. Editing an instance does not modify other instances; updating a reusable definition is explicit.

**Checks:** missing/changed media, names/IDs, analysis reuse, repeated/reversed/slipped snippets, tempo ramps, reopen/render/export and voice limits. Full gate for persistence/routing/ownership; actual drag/extend/repeat input at both sizes.

**Exit:** create a loop/vocal snippet, place independent copies and an audio riser without consuming another programme mixer lane; save/reopen/export.

## EP05 — Effect regions with one real processor

**Modify:** `Source/Model.h`, `Source/Model.cpp`, `Source/Engine.h`, `Source/Engine.cpp`, `Source/Automation.cpp`, `Source/AutomationUI.cpp`, `Source/TimelineAutomation.cpp`, `Source/TimelineEditing.cpp`, `Source/TransitionStyles.cpp`, `Source/TransitionRehearsal.cpp`, `Source/WavExport.cpp`, `Source/PluginLatency.cpp`.
**Create:** `Source/EffectRegions.h`, `Source/EffectRegions.cpp`, `Source/EffectRegionUI.cpp`, `Source/EffectRegionTests.cpp`; register them.

- [ ] Add a segment-owned instance with stable ID, processor/version, source-elapsed curve bounds, ordinary parameter curves and explicit wet/dry/bypass/tail policy. Map it with the existing automation clock; preserve current slip policy. Processor lifetime must not be hidden in paint state.
- [ ] Deliver tempo-synchronised echo first: add at range/playhead, move/resize/copy/bypass/delete/reopen. Bounds gate wet input; visible tail represents bounded decay. No callback creation/destruction/disk work.
- [ ] Offer explicit trim/reveal versus stretch-envelope resize. Move nodes with the region; use stable saved order for overlapping instances. Refuse incompatible overlaps/capacity atomically.
- [ ] Preserve ordinary VST chains. Add hosted-VST regions only after state cloning, latency and tails pass; initial built-in regions need not claim arbitrary plugin support.
- [ ] Extend saved presets with validated processor/settings/curves and versioning. Preserve node-only templates. A/B/C snapshots include regions and invalidate after changes.

**Checks:** entry/exit impulses, silence/tail, wet/dry, clock-mapped move/resize, stereo state, bypass/remove/seek, continuous versus offline output with equal warmup, missing processor, saved validation and native gestures. Full gate and short listening comparison.

**Exit:** move/resize an echo passage independently of another transition and reproduce it in Save/Open and WAV export.

## EP06 — Effects and pair tempo policies

**Modify:** new effect files plus `Source/TransitionUI.cpp`, `Source/TransitionControls.cpp`, `Source/TransitionStyles.cpp`, `Source/PresetNodes.cpp`, `Source/TempoStrip.cpp`, `Source/Beatmatch.cpp`, `Source/ElasticAudio.cpp`, `Source/Model.h`, `Source/Model.cpp`, relevant effect/tempo/export tests.

- [ ] Add reverb exit and noise riser/downsweep, then gater and loop in/out as accepted slices. Key/pitch, brake/scratch and cross-source side-chain each need measured DSP work. Reuse finite loops where correct; don't label a gain imitation as source-time manipulation.
- [ ] Expose only working processors/presets. Specify strength/wet-dry/beat timing, feedback/decay bounds, headroom and cancellation for each; compile whole presets into editable objects/curves.
- [ ] Define pair policies for constant-tempo4/4: inherit current behavior, whole-transition ramp, second-half ramp and independent source speed where feasible. Resolve neighbouring policies on shared tracks; Inherit must reproduce the current mix.
- [ ] Persist intent and compile through canonical clocks with lock/manual-timing protection. If unsynchronised speed cannot coexist with prepared mapping, retain that mode as unavailable until its engine slice passes. Don't substitute metadata/paint for clock behavior.

**Checks:** measured processor fixtures, tails/headroom/side-chain identity and playback/export/rehearsal equivalence under defined history; tempo boundary samples/phase, reverse/slip/loops, neighbouring locks/conflicts and Undo/reopen. Focused suites then full gate per coherent candidate; musical listening separately.

**Exit:** list accepted effects/tempo modes individually. Core editing can ship earlier; full effect/tempo parity stays partial while a referenced family is missing. This doesn't require variable-tempo detection or other meters.

## EP07 — Qualify a stem backend and build asset jobs

23 October0.55.47 update: [local separation](../../STEM-SEPARATION.md) now implements four-stem Demucs jobs, aligned float assets, source/runtime-bound validated reuse, progress/cancel, failure isolation and CPU fallback. [Measurements](../../research/STEM-DIRECTION-20261023.md) cover real music, exact sample clocks, residuals, mono/48kHz/quiet inputs, rejection of bad synthetic outputs and runtime comparison. Musical listening acceptance remains open. This implementation keeps ownership in Source/StemJobs.cpp/.h, Source/StemUI.cpp, Source/StemTests.cpp and scripts/stem_worker.py; it does not change the original Media/Project or saved formats. The broader routing files below remain EP08 work.

**Modify:** `Source/LibraryAssets.h`, `Source/LibraryAssets.cpp`, `Source/Model.h`, `Source/Model.cpp`, `Source/Engine.h`, `Source/Engine.cpp`, `Source/PlaybackCacheTests.cpp`, `Source/LibraryAssetTests.cpp`; dependencies only after qualification.
**Create:** `Source/StemAssets.h`, `Source/StemAssets.cpp`, `Source/StemJobs.h`, `Source/StemJobs.cpp`, `Source/StemTests.cpp`; register them. Update `docs/LICENSING.md` for new obligations.

- [ ] Measure a maintainable local backend on permitted representative4/4 audio: vocals over bass/kicks, quiet material and long sources. Record model/config/backend, CPU/device/RAM, elapsed time, quality/listening limits and dependency/weight rights. Don't reuse the competitor extension/implementation.
- [ ] Identify assets by source hash, decoder/version, model/config hash, quality, rate/channels, requested/processed interval and chunk overlap/alignment compensation. Preserve authoritative original-source sample alignment/duration; filename/BPM is insufficient.
- [ ] Run bounded asynchronous jobs with progress/cancel, disk limits, source-change checks, reusable valid cache and atomic publication. Partial/failed output cannot become Ready; clean only owned incomplete artifacts.
- [ ] Start with explicit high quality; add preview only after both qualities share an accepted alignment contract. Label quality/export readiness. Export explicitly waits, cancels or refuses missing required assets; never silently substitutes original audio.
- [ ] Measure original/stem alignment, recombination residual and chunk joins. Synthetic mapping needs known exact expectations; real separation needs predeclared measured bounds and listening, without a bit-perfect recombination promise.

**Checks:** invalid/missing model, device failure, cancellation phases, stale/repeated jobs, changed source, interrupted publication, cache reload, low disk and malformed manifest. Full gate for engine/cache/build/persistence. Retain research audio only for named subsequent measurements/listening.

**Exit:** validated reusable aligned stems with honest job/quality/failure behavior. This alone is not timeline editing. Backend quality/performance/rights failure must produce a feasibility result before EP08.

## EP08 — Stem editing, transitions and extraction

**Modify:** stem/snippet/effect files plus `Source/TimelineAutomation.cpp`, `Source/Workstation.h`, `Source/Workstation.cpp`, `Source/TransitionUI.cpp`, `Source/TransitionControls.cpp`, `Source/TransitionStyles.cpp`, `Source/Engine.cpp`, `Source/WavExport.cpp`, `Source/TrackOwnershipTests.cpp`, `Source/ExportTests.cpp`.

- [ ] Show collapsible drums/bass/melodic/vocal child lanes only for real backend outputs. Follow parent source mapping through trim/slip/tempo/reverse/loop and supported live cursors. Child rows don't consume programme mixer lanes.
- [ ] Add independent stem gain/mute/solo and ordinary automation with stable stem identity in its address. Display two/four subviews only for available asset topology; preserve explicit routing.
- [ ] Compile per-stem None/Crossfade/Swap/Full/Manual into editable nodes via existing preset/A/B/C owners, refusing unavailable/incompatible assets before mutation.
- [ ] Extract a selected stem interval as a sample. Offer explicit linked mute of its original counterpart with saved source/range identity, atomic Undo/delete and no duplicate original-plus-stem playback or orphaned mutes.
- [ ] Reuse EP05 for extracted-stem effects; add parent stem effects after routing checks. Save required quality, assets, curves and links; reopen unavailable assets honestly with relink/regenerate paths.

**Checks:** known aligned synthetic stem sums, independent levels/mute/solo, linked extraction, independent copies, missing/partial/stale assets, every clock transform, overlapping pairs, retirement/callback limits, export quality and real Save/Open/Undo/Redo. Full gate, actual sublane/range/drag input and artifact listening.

**Exit:** swap drums while retaining vocals; move a vocal range elsewhere, mute the original, add echo, save/reopen/export. Job completion, sublane paint or a Stems button cannot satisfy this milestone.

## EP09 — Accept complete journeys

- [ ] Complete a three-song mix with existing grid correction, pair presets/manual nodes, range copy/cut/repeat, snippet overlay, effect region, protection, Undo/Redo, Save/Open and WAV export.
- [ ] Repeat at1250/1920 widths and detached/collapsed layouts using mouse/keyboard. Verify focused node/range/segment commands, stale menus/jobs and missing media/effect/stem recovery.
- [ ] For full report-scope acceptance add EP06's effect/tempo list and EP08's drum/vocal extraction. Measure rendered timing/tails/headroom and listen to actual edits. Record unresolved reference behavior and separation artifacts.
- [ ] Run the complete gate once on the final milestone candidate. Never splice suites from another executable. Record physical MIDI/ASIO, private monitoring and long listening separately; unavailable acceptance stays open.
- [ ] Update handover, existing finishing IDs, formats and instructions. The full manual remains last under the wider agreed scope.

**Exit:** report core editing, effects/tempo and stem editing as separate accepted/partial milestones. Don't claim a parity percentage from menu counts or test totals.

## Verification commands and evidence

Documentation delivery: `python scripts/check-project.py`, expecting zero errors, plus review changed claims/links. No native build/regression rerun just for this audit.

Implementation focused runs must use `verification.run_checks`. A new local driver can accept a frozen executable and fresh evidence root as arguments:

```python
from pathlib import Path
import sys
sys.path.insert(0, str(Path.cwd() / 'scripts'))
from verification import run_checks, write_json

candidate = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=False)
workspace = Path.cwd()
plugins = [
    ('--plugin', 'build/MixworksTestGain_artefacts/Release/VST3/Mixworks Test Gain.vst3'),
    ('--delay-plugin', 'build/MixworksTestDelay_artefacts/Release/VST3/Mixworks Test Delay.vst3'),
    ('--latency-plugin', 'build/MixworksTestLatency_artefacts/Release/VST3/Mixworks Test Latency.vst3'),
]
plugin_args = []
for flag, relative in plugins:
    path = workspace / relative
    assert path.exists(), path
    plugin_args.extend([flag, str(path)])
results = []
for suite in ('timeline-editing', 'track-ownership'):
    results.append(dict(name=suite, **run_checks(
        candidate, ['--' + suite + '-only', *plugin_args], root / suite, cwd=workspace)))
write_json(root / 'summary.json', {'scope': 'editing-focused', 'suites': results})
assert all(r['exit'] == 0 and r['complete'] and not r['failures'] and not r['errors'] for r in results)
```

These plugin fixtures match `scripts/verify-analysis.py`. Register new tests in an existing suite or add explicit dispatcher/harness support in that increment; file existence is not execution evidence.

For a final audio/model/persistence/ownership candidate:

```powershell
python scripts/verify-analysis.py --exe $candidateExe --output $freshGateRoot --receipt $newReceiptPath
```

Variables identify that frozen candidate and new paths. Expected: every required suite complete, no failures/infrastructure/retention errors, exact-bundle receipt. Use the [bridge protocol](../../DEVELOPMENT-BRIDGE.md) only in fresh owned short roots, verify protocol/root/PID/capabilities, await work, save before graceful close and inspect pixels. Bridge extensions require client/harness and `verify-bridge.py` through run_checks.

Retain source/checkpoint hashes, failing reproductions, exact commands, reports, review and captures independently of Git. Follow [testing policy](../../TESTING.md): owned generated audio/arrangements/TEMP are removed after exit; compact evidence survives. No public release, automatic production restart or data reset follows from this plan.

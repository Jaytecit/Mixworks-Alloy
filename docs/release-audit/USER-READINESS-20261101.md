# Alloy user-readiness audit — 1 November 2026

## Version0.55.65 activated - ua17

Jay requested activation, a new UI version and launcher update. Source is now
**0.55.65**; native Workstation pixels and executable ProductVersion/FileVersion
both verified. Local launcher/desktop shortcut chain targets
`out/releases/0.55.65-912922ef0ceb/Mixworks.exe`, SHA256
`912922ef0ceba0ea841a7769d933ba0de55ceb9364ca6186501b3f5dbec11a65`.
All35 staged payload files and launcher backup hashes checked. No app launch,
restart, rescan or user-data change. Previous candidate retained.

One serial single-worker below-normal build; fresh waveform/version37/0 through
verification.run_checks, no stress or infrastructure/retention errors. Production
inputs differ from ua16 only by the CMake version; one native version capture was
added. Six waveform captures exactly match ua16's reviewed captures; prior112/0
remains separate evidence on its own executable. Source/bundle/report/UI evidence
and staging receipt retained independently of Git in `.local/ua17/`.
This explicitly authorised local-preview activation supersedes older pending
activation statements. Full readiness and broader regressions remain open.
See [checkpoint](UA17-VERSION-ACTIVATION-20261101.md).

## Source waveform/grid and fine controls - ua16

[Checkpoint](UA16-WAVEFORM-GRID-CONTROLS-20261101.md): candidate `e1d1c5737f7b` builds and passes112/0 (waveform36, grid76); twelve native captures reviewed. Reproduced early waveform smearing from overview buckets on original-file streams and catalogue drawing after hydration. Zoomed source/timeline views now use exact nonwaiting sample detail. Added waveform phase dragging (Shift one-tenth, click auditions, Undo/Escape),250/50/10ms zoom, energy colour and bar.beat/provenance markers. Full opening precision retained. No detector, saved grid/format or playback placement changes; no rescan/reset.

Red24/9 and intermediate34/1 retained; last failure was a pixel fixture counting IN text/downbeat flags, independently checked against red/green captures. Final112/0 has no infrastructure/retention errors. Four serial single-worker below-normal builds; no stress/performance/full gate/device/model. Evidence `.local/ua16/`. Launcher remains ua14; production initially observed, later inventory empty, no process control/data changes. Physical/listening/real-song accuracy and broader shared-stream/model regressions remain open. Continue remaining functional audit without repeating passing focused checks absent a relevant change or concern.

## Recovery and source relinking - ua15

[Checkpoint](UA15-RECOVERY-RELINK-20261101.md): candidate `c23eed125222` builds and passes67/0 bounded recovery/relink checks; three native captures inspected. Repaired Locate bypassing saved identity checks and leaving take snapshots at old paths. Wrong audio refuses, exact moved audio retries/saves/reopens; independent snapshot PCM matches. First52/8 was a message-loop test ordering fault, retained and fixed; final67/0 has no infrastructure/retention errors. Three successful serial one-worker below-normal builds; no stress/performance/scan/device/model work. Evidence/source identities `.local/ua15/`.

Launcher remains ua14; production PID27916 and user data untouched. Whole-rule/full readiness and broader persistence regressions remain open. Latest Jay feedback: consistent source waveform/grid offset screenshot; investigate source/timeline coordinate mapping next without assuming detector error or resetting data.

Latest activation (ua14): Jay explicitly requested launcher update and closure of
any Alloy instances. None were running. Launcher/desktop shortcut now use ua13
candidate `3a8286068928`, with237 focused checks and34 staged-file hashes verified.
No application launched; full readiness remains open. Evidence `.local/ua14/`.

Latest: [ua13 combined focused verification](UA13-FOCUSED-VERIFICATION-20261101.md).
One successful build; grid61/0, key labels23/0, destination111/0, save-export42/0.
Twelve native captures inspected. These supersede ua09–ua11's pending native
status for this scope. Physical/listening/novice/full-rule acceptance remains
open. Launcher unchanged; no production restart or new analysis.

Latest steering: [ua11 guided grid correction](UA11-GUIDED-GRID-20261101.md).
Jay requested a simple manual repair method with a separate Advanced tab. Source
implementation provides Tempo / Beat one / Check drift, bar-accent-only shifts
and explicit save/rebuild consequences. Build stopped for memory; native/pixel
acceptance pending. No automatic timing accuracy improvement is claimed.

Latest steering: [ua10 key labels](UA10-KEY-LABELS-20261101.md). Jay questioned
blanket Review labels.44 of45 inspected saved records were Review despite having
accepted detector keys. Display now classifies accepted keys as Detected and
reserves Review for tentative/invalid results. Musical accuracy is not inferred.
Build/native/pixel verification remains pending low memory; source only.

Latest: [ua09 destination safety](UA09-DESTINATION-SAFETY-20261101.md). Repaired a
source-confirmed overwrite-consent gap after filename extension correction across
Save As and exports. Bounded error/retry checks added, but build/runtime/visual
verification is pending because memory preflight prevented starting the build.
Treat as **repaired awaiting verification**. Launcher remains ua08. Full readiness
is explicitly open; no whole-rule acceptance or production-data change.

Latest: [ua08 launcher activation and save/export](UA08-SAVE-EXPORT-20261101.md).
Jay explicitly requested activation: launcher now uses local preview `9770639eb18c`,
with accepted ua07 production behaviour and test-only additions. Native core
save/export journey42/0; real output samples and dialog pixels inspected. This
supersedes the older unactivated-launcher statements below, without claiming
whole-product readiness. Continue bounded destination/error/retry audit work.

Latest: [ua05 shared lane automation and late-scene pixels](UA05-SHARED-LANE-AUTOMATION-20261101.md). Lane49, Solo40, live30 and visual24 focused checks pass on one candidate;133 section-led scenes/81 looks, GPU stills inspected. Whole-plan and human acceptance remain open.

Latest: [ua04 visual composition, independent review sorting and guide](UA04-VISUAL-COMPOSITION-20261101.md).
Section-aware scene changes, faster quiet/loud transitions and balanced full-show
variety; focused native evidence and limitations recorded. Whole plan remains open.


Latest: [ua03 lane-only Solo](UA03-LANE-STEM-SOLO-20261101.md),40 focused checks passed; native pixels inspected. Jay accepted shared neutral lane curves plus separate per-track transition fades for overlaps. Whole-plan acceptance remains open.

Previous: [ua02 live-transition repair](UA02-LIVE-TRANSITIONS-20261101.md),29 focused
native checks passed; physical/musical/wider-mode acceptance remains open. Native
pixels inspected without a browser. No performance tests or launcher activation.

Execution begun: [ua01 checkpoint](UA01-CHECKPOINT-20261101.md). Inventory/rule
coverage refreshed and observations mapped; failure classification, native core
journey and visual acceptance remain incomplete. The milestone boxes below stay
open until their full outcomes are demonstrated.

## Objective and current boundary

Make Alloy reliable and understandable for a person completing a mix without knowing its implementation. Jay requests an audit of every feature, including the observations below. A feature is accepted only when its normal user workflow works, its scope and consequences are clear, mistakes are recoverable, and saved results survive reopening. Historical implementation claims and focused test counts are supporting evidence, not current acceptance.

This is the current product-level plan, building on the existing94-rule audit and feature/control inventories rather than replacing them. The detailed implementation of each milestone follows its reproduced findings; this document does not claim an executed whole-app audit. Current source inspection and historical records inform hypotheses only. No application code or launcher was changed for this planning checkpoint.

**Latest operating constraint:** Jay is using the computer for other tasks. Do not run speed, frame-rate, load, stress or performance benchmarks until the interface is complete, functional and easy to understand. Incidental timings during concurrent use are not reliable performance evidence. Do not rerun the earlier visual benchmark. Functional checks remain proportionate, serial and resource guarded; inspect suite contents before running them to avoid embedded timing/load tests. Coordinate actual mouse/keyboard sessions rather than competing for control. Reassess readiness and computer availability before the later performance milestone.

The original-file/GPU build13 remains an unactivated development candidate. Normal shortcut remains on the older build. Do not promote a partially verified candidate as ready for users. The clean reset is complete; originals remain protected, automatic scanning is off. Do not rescan the whole library before the analysis changes have demonstrated better results on a representative subset.

## Jay's observations and acceptance requirements

| ID | Observation / requested behaviour | Current evidence | Required outcome |
| --- | --- | --- | --- |
| UA01 | Visual Auto Arrange stopped after12 tracks in a44-track mix. | User report; exact old mix was disposable and may no longer exist. No12-track cap seen in the inspected planner; this does not rule out admission, generation, loading or playback failures. | Recreate a44-track arrangement in an isolated workspace. Inspect generated coverage through the actual final audio end, save/reopen it and inspect later scenes. Distinguish an incomplete schedule from a renderer that stops. No benchmark required. |
| UA02 | Only3 scenes and2 transitions used; some scene boundaries had no transition. | Planner narrows candidates to one style family; logo/image eligibility narrows them further. It penalises immediate repeats but does not balance whole-show usage. Some empty handover windows deliberately become cuts. These mechanisms could explain repetition/cuts, not the exact report. | Whole-show coverage with deliberate variety across eligible scenes and compatible transitions; avoid needless repetition when alternatives exist. Every boundary gets an intentional, visible treatment. No silent omission; cuts should be an explicit choice rather than an unexplained fallback. Handle logo compatibility and short/overlapping handovers. |
| UA03 | Timeline beat markers and track downbeats still look misaligned. | User report; older targeted timing fixes do not establish a general solution. | Independently check detection, source-to-project conversion, snapping, waveform drawing and zoom rounding. Audible pulse, track downbeat and project bar markers agree at beginning/middle/end, after moves, trims, tempo changes and reopening. Do not move only the drawing to hide incorrect timing. |
| UA04 | Better analysis and a new scan; separate sorting of unconfirmed key and beat alignment. | General detector accuracy remains unaccepted. Existing evidence scores are not calibrated probabilities. Current long-source automatic-analysis limit is also unresolved for this workflow. | Independent key-review and grid-review columns/filters/sorts, with clear detected/needs-review/user-confirmed states. Distinguish BPM, pulse and downbeat problems. High detector scores must not imply human confirmation. Validate improvements before full rescan; make cancel/resume/failures understandable and avoid whole-song persistent copies. |
| UA05 | Editing is confusing, cluttered, repeated and difficult. | User experience report; historical audit already identified unclear ownership and competing controls. | One obvious primary route for each common edit, consistent selection/handles/shortcuts, clear track/lane/master scope, visible Undo and predictable delete/move/trim/split behaviour. Contextual advanced tools; remove redundant routes after inventory, preserve useful keyboard alternatives. |
| UA06 | One automation line per parameter per lane, spanning every track, with immediately editable8-bar start/end nodes. | New explicit product direction supersedes older empty-curve/no-auto-endpoint preferences. Jay accepted shared neutral lane curves plus separate per-track transition fades; native usability acceptance remains open. | One lane-owned curve for each parameter, shared by every placement on that lane. Working interpretation: four handles per track—start,8 bars after start,8 bars before end,end. Use parameter-appropriate neutral values until edited; do not silently fade every parameter. Show only the selected parameter to avoid clutter. Define short-track, overlap, split, move, delete, gap and tempo-change behaviour before implementation; avoid stacked conflicting nodes or jumps. |
| UA07 | Solo a selected stem on its lane only. | Existing feature register explicitly describes global stem solo; changed scope is required, not merely a label fix. | Solo vocals/bass/drums/other within the chosen lane; all other lanes retain their sound. Labels, lane mute/solo, original/stems A/B, automation, Undo, save/reopen and export agree. |
| UA08 | Transition presets cannot be placed while music is playing. | User report, not yet reproduced on build13. | Place/apply a transition through the same clear action while stopped or playing; show the target and start point. Do not stop/reseek programme playback unexpectedly or alter another lane. Explain any genuinely unavailable action. Test through actual UI and rendered audio, not command acknowledgement alone. |
| UA09 | A separate alignment bar is needed for live alignment. | New requested control. | A visually distinct draggable alignment guide independent of the playhead. Moving it must not seek, stop or move audio. Snap/free placement and visibility are explicit; use it to compare track beats while playback continues. Confirm its behaviour in the editing preview before making it universal. |

Jay accepted shared neutral lane curves plus separate per-track transition fades on1 November. Default handles must not create automatic fades. The ua05 native preview implements short-track merging, shared overlap handles and authored project-time nodes; obtain observations on the concrete experience before a broad interface rewrite. Do not require Jay to design internal ownership.

## Complete coverage, beyond the reported list

Refresh existing feature/control/file inventories against current source, including original-file playback, visual planning/output and tools added since25 October. Link each user-facing feature to an existing rule or an explicit new rule; no unlisted or silently excluded feature. Include disabled actions, dialogs, context menus, keyboard/MIDI routes and saved state. Use one current ledger with statuses **not checked, reproduced problem, repaired awaiting verification, verified, blocked**, with exact candidate and evidence. Record usability separately from technical correctness so passing code does not conceal confusing interaction.

Coverage must include:

- Startup, new/open/save/save-as/recovery, missing files, cancellation and errors.
- Library import/scan/retry, metadata, sorting/filtering/search, preview, source correction and review status.
- BPM/key/pulse/downbeat detection, source/project clocks, preroll, snapping, zoom and waveform/grid display.
- Manual placement and Auto Arrange, extension, selection, trim/slip/move/split/delete, grouping/locking, Undo/Redo.
- Lane/track/master controls, automation, transition frames/presets, stem extraction/routing/solo and effects/plugins/tails.
- Arrange and Record/Perform modes, private audition, recording/takes/replay, MIDI mappings and device routing.
- Visual scene library, automatic/manual arrangement, image inputs, transitions, preview/fullscreen/output lifecycle.
- Audio export and every currently offered output format, playlist/preset exchange, reopen and exported-content verification. Do not imply audiovisual export exists if it is not implemented.
- Layout/readability/scaling, terminology, feedback, discoverability, keyboard/focus/accessibility and help.
- Installation/package/update/uninstall, dependencies, diagnostics, source integrity, later resource limits and distribution prerequisites.

Hardware, musical listening and external-user usability checks remain explicit acceptance work. Test crashes are failures requiring classification; old tests may be updated only against an agreed current behaviour with retained failure evidence, never just to make the dashboard green.

## Ordered milestones

**Latest steering (ua07):** Jay reviewed the native ua05 preview and found the
Editing controls unclear and poorly organised, specifically slicing, selecting,
copying and arbitrary unsnapped dragging. The preview-feedback prerequisite is
now satisfied. Implement the task-led editing redesign informed by MixMeister
and DJ.Studio; see [ua07](UA07-EDITING-WORKFLOW-20261101.md). Shared lane curves,
lane-only stem Solo and section-aware visuals have focused evidence in ua03–05;
whole-milestone acceptance remains open. The ua06 fresh-analysis attempt stopped
at its memory guard with no detector result; continue lighter workflow work and
do not start the other source evaluations under the same memory conditions.

**ua07 result:** task-led redesign implemented, workflow32/0 and native pixels
reviewed. Jay accepts the direction: "Clearer; keep this direction". Free
selection, click-to-position, slicing, copying and compact mixer access are
demonstrated in an isolated native fixture. Continue this direction. Physical,
musical, newcomer and remaining whole-feature audit work is still open.

### 1. Establish the current truth and the simpler workflow

- [ ] Refresh the existing inventory and link UA01–UA09 to it; distinguish old-version reports from current defects by reproduction.
- [ ] Classify existing broad failures into application defects, obsolete expectations and harness faults; preserve each original failure. Defer timing/load checks under the latest instruction.
- [ ] Walk the core journey: find music → review analysis → place and align → edit/fade → add transitions/visuals → save/export.
- [ ] Produce a coherent editing preview with clear selected-object scope, lane automation, alignment guide and contextual tools. Obtain Jay's observations on the experience before a broad interface rewrite.

**Visible result:** a comprehensible workspace and one prioritised current defect list; no claim that every feature is already fixed. Primary source areas: Workstation/WorkspaceUI/Layout, TimelineEditing/Header/Automation, EditSelection, AutomationUI, existing audit inventories and rules.

### 2. Make the music and grid trustworthy

- [ ] Establish a fresh representative constant-tempo4/4 evaluation set with independently reviewed BPM, pulse, downbeat and key; do not recreate deleted annotations as if they survived. Use REA/reference apps where useful, with disagreement and ambiguity explicit.
- [ ] Diagnose each alignment layer and improve detector/review behaviour against separate tuning and evaluation recordings. Include quiet intros, syncopation, half/double tempo ambiguity and long sources.
- [ ] Add independent key/grid review sorting and clear correction/confirmation actions; ensure source save really persists.
- [ ] Demonstrate improved accuracy and usable correction on the subset, then perform the fresh full-library scan in a coordinated window. Retain concise results and failures, not full decoded song copies.

**Visible result:** tracks line up musically, uncertain results are easy to find, and corrections remain correct after reopening. Primary owners: Analysis/AnalysisCache/Model/GridCorrection, library workflows/UI and timing/analysis audit rules. Numerical and listening acceptance must be explicit before calling analysis reliable.

### 3. Finish editing, lane automation and live controls as one experience

- [ ] Implement the accepted editing preview and lane-owned automation, including neutral pre-created endpoint handles and defined overlap/short-track behaviour.
- [ ] Implement lane-only stem solo, the independent alignment guide and transition placement during playback.
- [ ] Verify real selection/drag/keyboard actions, scope, Undo/Redo, save/reopen and audio/export results together. Reconcile mode-specific tools rather than duplicating another control panel.

**Visible result:** place two tracks, align them, adjust fades and a live transition without first creating nodes or discovering hidden target rules. Primary owners: Model/Automation/Engine, timeline/editing UI, StemMixing and transition controls. Include Record/Perform and MIDI consequences in the ownership change.

### 4. Complete the visual show workflow

- [ ] Reproduce the44-track case; validate schedule coverage, scene eligibility, variety and every handover independently of renderer speed.
- [ ] Repair generation and any later-scene loading/playback failures; make scene/transition selection and deliberate cuts understandable.
- [ ] Verify save/reopen and representative beginning/middle/end output with real pixels. No speed/frame acceptance at this stage.

**Visible result:** a complete editable visual show through the final track, with intentional variety and transitions. Primary owners: VisualArrangement/VisualScenes/SceneLane and Python scene worker/compositor/presenter. Prior60fps sections do not establish44-track planning correctness.

### 5. Close every remaining functional and usability gap

- [ ] Complete the ledger across effects, recording, replay, devices, MIDI, exports, recovery, errors, access and packaging.
- [ ] Run complete user journeys on the same candidate; inspect actual saved/exported output and listen where musical behaviour matters.
- [ ] Arrange a small first-use trial with people who did not build Alloy. Record where they hesitate or need explanation; repair the main workflow accordingly. Jay's successful use is necessary but cannot substitute for newcomer evidence.

**Visible result:** every promised feature has a current outcome and the ordinary mixing journey is understandable. A broken core feature blocks release; any proposed removal/deferment of an existing promised feature must be explicit, not hidden in the audit.

### 6. Qualify performance and release only after interface acceptance

- [ ] Once the interface is complete and understandable and the computer is available, agree a representative workload and run serial memory/storage, long-playback and frame-pacing checks under controlled conditions.
- [ ] Run the final appropriate full regression gate, including previously deferred timing checks; identify all remaining hardware/listening/package limits honestly.
- [ ] Verify clean installation, persistence/export and crash recovery, and resolve the documented JUCE/ASIO/SoundTouch distribution requirements before any public package.

**Release decision:** no unresolved core functional defects, no misleading analysis certainty, acceptable first-use workflows, supported hardware/output evidence, bounded resources on the agreed workload, passing final verification and distribution clearance. Neither a high test count nor a smooth short demo meets this alone.

## Checkpoint policy and method

Own triage and implementation; Jay supplies observations, not exhaustive technical specifications. Batch related repairs, one build worker and one test process, no build/test overlap. Keep brief durable evidence for each milestone. Avoid another open-ended series of performance experiments or repeated whole gates during interface work. No subagents or additional worktrees needed for this planning checkpoint.

Method discovery consulted [skills.sh](https://www.skills.sh/) as requested. Its catalogue does not establish Alloy-specific correctness or replace the repository's existing native-app audit. No additional skill/tool installed. This is a product audit and sequencing plan; code-level implementation plans should be limited to the next evidenced milestone, not speculative rewrites of the entire app.

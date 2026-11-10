> Continuation and priorities moved to [the outstanding roadmap](../../FINISHING-PLAN.md), especially W28/W30. The chronological runs below retain their original next-step labels and candidate-bound evidence; those labels are historical. No rule has been promoted by documentation cleanup.

# Audit execution status and continuation

This is the dated release-audit ledger. For the latest source/local activation and
visual work, read [HANDOVER](../../HANDOVER.md). Later focused visual checks do not
close this ledger's unresolved application-wide qualification items.


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

Latest: [ua08 launcher and save/export journey](UA08-SAVE-EXPORT-20261101.md).
Launcher now uses `9770639eb18c`, a local preview with the accepted ua07 production
code and expanded tests. Save/Save As/reopen/WAV/playlist/track-list/cancel:42/0,
written PCM oracle and dialog pixels inspected. Source/bundle/report identities
and launcher chain verified. No restart, scan or broad gate. Whole-rule acceptance
remains open; next bounded work is destination safety and error/retry paths.

Latest: [ua07 editing workflow](UA07-EDITING-WORKFLOW-20261101.md). Audio edits,
Fades & timing and Automation are separated; free selection and waveform click
positioning work. Final focused workflow32/0, native pixels inspected at1250x800
and1440x900 across the repair; compact lanes retain Mute/Solo/level. Jay accepts
the direction: "Clearer; keep this direction". Prior same-workflow candidate also
passed lane49/0 before the final compact-strip-only repair. Full usability and
whole-plan acceptance remain open. ua06 ear62/0 resolves the obsolete version42
assertion; fresh real-source analysis stopped at its memory guard before a result.

Latest: [ua05 shared lane automation and late-scene pixels](UA05-SHARED-LANE-AUTOMATION-20261101.md). Lane49, Solo40, live30 and visual24 focused checks pass on one candidate;133 section-led scenes/81 looks, GPU stills inspected. Whole-plan and human acceptance remain open.

Latest: [ua04 visual composition, independent review sorting and guide](UA04-VISUAL-COMPOSITION-20261101.md).
Section-aware scene changes, faster quiet/loud transitions and balanced full-show
variety; focused native evidence and limitations recorded. Whole plan remains open.


## Active checkpoint ua02 — live transitions, 1 November 2026

[Repair/evidence](UA02-LIVE-TRANSITIONS-20261101.md): UA08 Add now works in normal
playback without moving the playhead; focused navigation seeks only while stopped.
Candidate `b13a673a362d0460e51a8d365e258fd110e0269536a0154bf4b07119a86da5e0`
passes29 focused native callback/PCM/history/reopen/export-renderer checks. Native
pixels inspected. UA08 remains repaired awaiting physical/musical/wider-mode
verification; no whole-rule acceptance. All attempts in `.local/ua02/`; no owned
jobs or launcher change. No performance/stress/load/frame checks or scan.
Next source entry: `Get-Content Source/StemUI.cpp | Select-Object -Skip 165 -First 30`,
then Engine.cpp stem-solo mask; prepare minimal tagged two-lane reproduction.
Scrolling at1250x800 remains UA05 editing-usability feedback.

## Active checkpoint ua01 — 1 November 2026

[First user-readiness execution](UA01-CHECKPOINT-20261101.md) supersedes ra09 as
the active continuation. Inventory repaired/refreshed, visual rules added, nine
observations mapped in [current cases](inventory/current-cases.csv). All97 current
rules and9 observation cases remain unaccepted. [Retained failure records](inventory/retained-failures.csv)
preserve227 observations including duplicates; initial classification only.
Source confirms global stem Solo, stopped-only Add Transition and existing KEY
CHECK sorting; no native reproduction yet. Editing concept is simulated; browser
security prevented local-file visual inspection. Source/binary identities preserved
in `.local/ua01/`; no native app/build/benchmark/scan or activation.

Next exact source entry: `Get-Content Source/TransitionUI.cpp -TotalCount 50`,
then inspect `canInsertArrangement` and prepare one bounded two-track live
transition reproduction. No broad SceneLane suite (embedded FPS assertion).
Keep current plan constraints; no owned jobs remain. Details, source trace,
preview limitations and unresolved automation overlap semantics are in the checkpoint.

## Current direction — 1 November 2026

The [whole-app user-readiness plan](USER-READINESS-20261101.md) is the current priority, including Jay's latest issue list and usability requirements. Refresh this audit's feature/control coverage against current source; the dated runs below remain historical evidence, not acceptance of the November candidate. No whole-rule status is promoted by the planning work. Speed/frame-rate/load/stress testing is deferred until the interface is complete, functional and understandable and the computer is available. Current work is functional reproduction, workflow simplification and repair; keep full-library scanning off until analysis improvements have been validated on a representative subset.

## Active run ra09 — take-seek lifecycle and error cases, 25 October 2026

User requested minimal lifecycle/error continuation, preservation and staging.
**AUD-D14/D15 confirmed and repaired.** A transient engine request counter now
invalidates pending seek input/completion after Stop, Play, programme seek, mode
changes, document commit or a newer audition request. Both asynchronous boundaries
check it. The progress wrapper now owns modal completion and observes dismissal
before success, so native window close, Escape and Cancel cancel even after worker
completion. Cancellation is cooperative; UI reclamation waits without blocking for
worker exit. Owner destruction cancels safely. No detector or saved-format change.

Final candidate: **182 focused checks, zero failures/infrastructure/retention
errors**: lifecycle/error95 and performance/replay/export/parser87, sequential via
`verification.run_checks`, below-normal, stress disabled. Tiny owned sources only;
no real device. Lifecycle cases cover later Stop/Play-Stop/mode round-trip/new direct
or cancelled seek intent/document replacement, close/Cancel, rate mismatch, missing
initial/event source, malformed snapshot, retry and owner teardown. Programme/take/
Undo preservation asserted; actual progress pixels inspected. No full gate, stress,
capacity, benchmark, hardware, physical gesture or listening acceptance.

Two sequential builds, one worker, `/MP1`, below-normal. The final shared-header
change required recompiling dependents. Red89/9 includes **7 application failures**
and **2 fixture failures**: missing-source fixtures used catalogue `path` rather
than session `file`; corrected for final checks and fresh error-feedback assertions.
No detector/model run, production cleanup or repeated passing broad suite. Shared
engine changes normally call for broader regression; the user's iterative minimal-
check override applies, and that coverage remains explicitly deferred.

**Staged for next launch:** `out/releases/0.55.64-1ed54e11c84f/Mixworks.exe`;
SHA256 `1ed54e11c84f53e48946749e34136d06e77b7380a05d968a885b1260edd874ce`.
Launcher atomically updated; old launcher retained locally and at
`out/Launch.cmd.bak.ra09`. Exact source/archive/bundle/dependency/decoder/plugin/
report/retention/preservation identities checked. Focused local stage only; version
0.55.64, analysis11/session41/retry3 unchanged. Source changes: `Engine.h`, `Engine.cpp`,
`PerformanceCapture.cpp`, `PerformanceModeUI.cpp`, `PerformanceTakeTests.cpp`, `SelfTest.cpp`.

Independent evidence/checkpoints and defect/control ledgers: `.local/ra09/`.
Production PID9640 remains unchanged r646; no restart/session/device/profile control,
import resumption, music edits or analysis/correction/cache reset. Running scan/
recovery writes retained. All94 whole rules remain78 NOT RUN/16 BLOCKED. Next:
remaining master-WAV/playlist chooser and SUS-01 independent-oracle cases. General
plugin/device-loss failures and cancellation during blocking/nontrivial DSP remain
unverified; bounded lifecycle passes do not establish those or whole-rule acceptance.


## Previous run ra08 — mode, capture exclusions and take seek, 25 October 2026

User requested the remaining controls, minimal checks, preservation and staging.
**AUD-D11/D12/D13 confirmed and repaired** in `PerformanceModeUI.cpp`:
- Mode completion rechecks export before side effects. Reselecting the current mode
  keeps private preview and workspace untouched; valid mode exit still finalises capture.
- Capture-exclusion choices bind the displayed lane/master eligibility and mode,
  rejecting stale choices after another eligibility change or leaving Record/Perform.
- Seek input requires a complete finite numeric value in range. Real Cancel remains
  effective after the worker finishes but before JUCE's completion timer. Dialog and
  worker completion both reject a changed work mode; existing ID/content guards remain.

Final exact candidate: **107 focused checks, zero failures/infrastructure/retention
errors**, below-normal through `verification.run_checks`, stress disabled. One tiny
owned WAV and a fake interface support private routing, active-capture exclusion,
finalisation, audible seek/Stop, cancellation, malformed/range input, removed/edited/
reordered takes, export admission and mode-change completion. Programme cursor/Undo
remain unchanged by seek. Actual seek-dialog pixels inspected. No hardware, physical
gestures, listening, broad regression, stress/capacity/benchmark or whole-rule PASS.

Five sequential incremental build attempts (one worker, `/MP1`, below-normal): one
fixture compile failure corrected the WAV stream type; four builds succeeded. The
first native run exited with stack overflow before the control cases. Moving the
owned Workstation to the heap let the fixture run: red **107/10 failures**. First
repaired run **107/1 failure** exposed a queued fake-device change notification that
stopped preview; the fixture now drains it before playback. App repair unchanged.
Final **107/0**; original sources/logs/reports for all attempts retained independently.

**Staged for next launch:** `out/releases/0.55.64-7df669117d50/Mixworks.exe`;
SHA256 `7df669117d50d2e382ac2544709b720ceab4f04a5dd0a66c1613b736480cb229`.
`out/Launch.cmd` atomically updated; earlier launcher is retained locally and at
`out/Launch.cmd.bak.ra08`. Source/archive/bundle/dependency/decoder/plugin/report/
retention/preservation identities verified. Focused local stage only, not complete
regressions or public release. Version 0.55.64, analysis 11/session 41/retry 3 unchanged.
Source changes only `PerformanceModeUI.cpp`, `PerformanceTakeTests.cpp`, `SelfTest.cpp`;
no shared header, engine, DSP, detector or saved-format change.

Evidence/checkpoints and control/defect ledgers: `.local/ra08/`. Production PID 9640
still runs unchanged r646; no restart, session/device/profile control, import
resumption, music edits or analysis/correction/cache reset. Ongoing production
analysis/recovery writes are preserved. All 94 whole rules remain 78 NOT RUN/16 BLOCKED.
Next: take-seek native window-close/cancellation and external Stop/newer-request
invalidation, rate/missing-source failures, then remaining master-WAV/playlist
choosers and SUS-01 independent oracles. These are open cases, not confirmed defects;
no repeated passing checks or full gate without a relevant new case/checkpoint.


## Previous run ra07 — export targets and remaining take controls, 25 October 2026

User requested continuing the export-target investigation/control audit, minimal
checks, production preservation and staging verified changes. **SUS-10 confirmed
and repaired as AUD-D10**: chooser/deferred-tempo callbacks could export a different
take after removal/reorder, export changed content or reopen after export admission
changed. Requests now retain the original take ID and SHA256 content fingerprint,
resolve current positions at completion and recheck busy/export/capture admission.
The scratch export ordinal is assigned only at the synchronous renderer handoff.

Three sequential incremental builds, one worker, `/MP1`, below-normal. Red target
fixture **60 checks/7 failures**. First repaired run **75 checks/1 fixture failure**:
Remove event correctly preserves surviving sequence IDs; the assertion incorrectly
expected renumbering. Test corrected; application repair unchanged. Final exact
candidate **75 checks/zero failures/infrastructure/retention errors**, all through
`verification.run_checks`, stress disabled. WAV sample counts distinguish targets;
real non-native chooser/menu/modal completions cover reorder/removal/changed content,
Cancel, busy/export admission and deferred-tempo retry. The real tempo worker installs
the continuation; the fixture holds it until completion before changing target state.
Five capture-exclusion owners and event Remove/Undo also have bounded case evidence.
The owned WAV chooser pixels were inspected. No broad gate, stress/capacity/benchmark,
physical gestures/devices, musical listening or whole-family/whole-rule acceptance.

**Staged for next launch:** `out/releases/0.55.64-79dccdf91e11/Mixworks.exe`;
SHA256 `79dccdf91e11c89a56787d9c6de8877864f2ab4f9afef6be976306ace74cab38`.
`out/Launch.cmd` updated atomically; prior launcher retained locally and in
`out/Launch.cmd.bak.ra07`. Source/archive/bundle/dependency/decoder/plugin/report/
retention/preservation identities verified. This is focused local staging, not a
complete regression receipt or public release. Version0.55.64, analysis11/session41/
retry3 unchanged. App source changed only `Workstation.cpp`, `PerformanceTakeTests.cpp`
and `SelfTest.cpp`; no shared header, engine, saved-format or detector change.

Independent sources, original failed reports, corrected fixture, native checks,
controls and preservation/staging evidence: `.local/ra07/`. Running PID9640 remains
on unchanged r646. Music, existing analysis/corrections/caches, references, devices,
profiles, session and deferred imports were not controlled/reset; ongoing app writes
are preserved. All94 whole rules remain78 NOT RUN/16 BLOCKED. Next: remaining
SUS-04 mode/exclusion/seek/cancel controls and SUS-01 supplemental independent oracles;
native picker gestures and fresh capture-state export journey remain unverified.


## Previous run ra06 — 25 October 2026

User requested conditional coverage/control continuation, minimal tests, production
preservation and staging verified application changes. **AUD-D09 confirmed/repaired**
in verification tooling. No Source/CMake or binary change was needed; the existing
stage `0.55.64-66ebcab993b3` and launcher remain current and unchanged.

- The original22 named suites omitted the early-return split-effects branch of
  `runTrackOwnershipTests`. An ordinary ownership pass did not establish it ran.
  Complete receipts also accepted `--catalogue-reopen` under a normal library-assets
  label, although that flag selects shortened child coverage.
- The catalog now requires23 suites, adding split-effects with the finite echo
  module as primary fixture. Runner/validator share role arguments: ordinary
  ownership retains gain, core retains its instrument. Unreviewed conditional flags
  cannot alter complete-suite coverage. Historical22-suite receipts stay unchanged
  and do not satisfy the revised contract; no new full gate has been run.
- Red main harness17/2 failures; final20/0. Scoped harness11/0 after correcting its
  exposed absolute-position fixture assumption (original11/1 retained). Other
  supplemental/model/backend/prerequisite paths still require explicit audit cases.
- One native run on the staged candidate: **29 focused split-effects checks/zero
  failures/infrastructure/retention errors**, below-normal via `verification.run_checks`,
  stress disabled. Tiny owned source/session cleaned; reports and5 PNGs retained;
  shared-effects/Auto Arrange pixels inspected. Seven bounded control families now
  have case evidence, including Split/Delete/Undo, injected mouse selection, shared
  effect/node owners, save/reopen/export/tails and the Extend default. Capture-only
  assertions are distinguished from functional controls; no physical gestures,
  general plugin/device or listening acceptance inferred.

Evidence/checkpoints/conditional and control-case maps: `.local/ra06/`. No compiler,
new executable, redundant staging, launcher change, production app/device/profile
control, import resumption or user-data reset. Running r646 and scan/recovery output
are preserved. Staged185-check acceptance remains its original scoped ra05 evidence;
this29-check supplement is separate. All94 whole rules remain **78 NOT RUN/16 BLOCKED/
0 PASS/0 FAIL/0 N/A**. AUD-D09 is partial MW-R09 evidence, not whole-rule PASS.

**Exact next work:** unconfirmed SUS-10 in `Workstation::chooseExport`: deferred-tempo
retry captures a numeric take index; file-chooser completion uses mutable
`selectedExportTake`. Trace `beginWavExport`/replay preparation and reproduce removal,
reorder and changed busy/capture state in a fresh owned picker/continuation fixture
before repair. No production chooser/files. Continue SUS-01 supplemental oracles
and remaining SUS-04 controls; do not rerun passing suites/full gate without a
relevant change or explicit checkpoint. No hardware/accuracy/package claim.

## Previous run ra05 — 25 October 2026

User expressly requested the private-popup investigation, minimal tests, no music/
analysis/running-session changes, and **staging plus launcher update on this pass**.
SUS-09 confirmed/repaired as **AUD-D08**. Independent evidence: `.local/ra05/`.

- Ten private-popup failures reproduced in47 checks: wrong selected source, changed
  busy/export/mode, stale discard/transport/control drafts and late library completion
  overwriting newer prepare/discard or targeting another lane. Tests use real owned
  modal callbacks, three tiny owned sources and one deterministically gated worker.
- Choices capture source IDs/lane and private-state fingerprints. Completion repeats
  admission; request tokens invalidate earlier library jobs even after prepare then
  discard returns to an empty state. Source membership/identity, selected library
  ID, lane and private context are checked before cache publication/preparation.
  Drafts cannot edit a replaced renderer. UI request tokens are transient component
  state, not saved format or engine state. Shared headers were not changed.
- Two incremental builds, one worker, `/MP1`, below-normal, no overlap. Final same
  candidate **185 checks/zero failures/infrastructure/retention errors**: private47,
  take UI29, performance/replay/export/parser87, library-preview22. Stress disabled;
  all ordinary native retention through `verification.run_checks`. No broad full
  gate, capacity/benchmark, physical hardware, listening, bridge or clean-package
  acceptance. Final popup, Record/Perform and fake-headphone chooser pixels reviewed.

**Staged:** `out/releases/0.55.64-66ebcab993b3/Mixworks.exe`, SHA256
`66ebcab993b3ad79b01faacec4adaf912edaf9932e6b220009cbf304dd129575`.
Version0.55.64/analysis11/session41/retry3 unchanged; hash distinguishes this local
update. It includes ra02–ra04 fixes, freshly covered by relevant focused tests on
this candidate. The source/archive, dependency pins, decoder patch, plugin, reports,
retention, pixels and preservation were verified before the scoped local stage.
The retained r646 staging method was adapted; standard complete-regression receipt
requirements were not weakened and no complete receipt was fabricated.

`out/Launch.cmd` now selects this immutable release for the next launch. Previous
launcher retained under `.local/ra05/` and `out/Launch.cmd.bak.ra05`; running PID9640
still uses unchanged r646 and was never restarted or controlled. Existing retained
analysis/reference JSONs remain unchanged; production's own scan/recovery output is
preserved. No user music edits, cache reset, device/profile/session control, bulk
import resumption, Git commit/push or public publication.

All94 whole rules remain **78 NOT RUN/16 BLOCKED/0 PASS/0 FAIL/0 N/A**; AUD-D08 adds
partial MW-U01/MW-U02/MW-P08 evidence. Full controls/hardware/listening/accuracy/
performance/clean-package/rights requirements remain open. **Next:** SUS-01 review
of conditional test coverage and independent oracles, then remaining SUS-04 dynamic
control families. Native headphone/device journeys and wider concurrent private
preparation/plugin cases remain distinct from these bounded callback checks.
Do not repeat passing focused tests without relevant changes; keep the running app,
its generated data and deferred-import checkpoint untouched.

## Previous run ra04 — 25 October 2026

User specifically authorized stale-popup investigation and repairs with minimal
tests, leaving music, analysis and the running session alone. **SUS-08 confirmed
and repaired as AUD-D07**. Independent evidence: `.local/ra04/`.

- The real take-menu/event-dialog callbacks reproduced8 failures in29 checks:
  removed and reordered targets, stale event drafts, changed export/recording
  admission and master-WAV choice after programme playback began. A stale discard
  could shift the active capture index; the owned red fixture adds one sentinel
  take to observe the fault without permitting an out-of-bounds status read.
- Menu results now bind to the take IDs present when opened. Removed IDs reject;
  reordered takes retain identity. Completion rechecks busy/export/capture/playing
  conditions appropriate to the action. Event and seek dialogs compare SHA256
  take fingerprints before using saved ordinals or reconstructed state; chooser
  completion also rechecks stopped-state admission. Stale drafts do not add Undo.
- New `--performance-take-ui-only` dispatches an isolated branch of the existing
  friend test entry. Final **29 checks/zero failures/infrastructure/retention
  errors**, including unchanged edit/discard/cancel and Undo, actual owned popup/
  modal completion, supported Undo and a genuine Play-to-recording transition.
  One48,104-byte generated WAV was removed by retained cleanup; its isolated
  analysis/profile data stay outside production. Popup/editor PNGs inspected.
- Three incremental build attempts, sequential, one worker, `/MP1`, below-normal:
  initial fixture compile failure retained (private access/Windows declarations),
  then corrected test build and repaired UI build passed. Two native runs only.
  No full performance/parser rerun, broad regression, stress, capacity, benchmark,
  bridge, physical MIDI/output or human test. Source/format/revision version unchanged.

After verification, four whitespace-only blank lines in the new test were cleaned.
`source-after`/`changed-source-final.zip` preserve exact tested inputs;
`post-check-whitespace.json` maps their hashes to `source-completion`. This formatting
changes no tokens or line numbers and does not inherit a new native-run claim.

Changed application files: `PerformanceModeUI.cpp`, `PerformanceTakeTests.cpp`,
`SelfTest.cpp`. The **unstaged** repaired candidate has SHA256
`92715f51086a93e7fd9c0b0f15aef55d1113df22702d93d8e361731ad033ab9e`.
Normal r646 binary/launcher and the production process remain unchanged; ongoing
scan/recovery updates are preserved. Older ra02/ra03 results are not fresh passes
for this candidate. All94 whole-rule statuses remain **78 NOT RUN/16 BLOCKED/
0 PASS/0 FAIL/0 N/A**. AUD-D07 supplies partial MW-U01/MW-U02/MW-P07 evidence;
remaining hardware/listening/package/rights/requirements acceptance stays open.

**Exact next work:** SUS-09 is a static suspicion in `showPrivatePreparationMenu`:
the callback initially checks only lifetime/cancel, then reads current selected
clip/transition/library source and routes. Its asynchronous library completion
also needs target/request-generation and busy/mode admission review. Reproduce
with a fresh owned native fixture before claiming a defect; do not use production
selections/devices or resume imports. Continue SUS-01 conditional-case/oracle
reconciliation. Seek-thread cancellation/completion and native file-chooser
interaction are not proved by the29 callback checks; their source guards were
reviewed but no physical/whole-journey acceptance is inferred. Recheck RAM before
further native work; do not rerun passing checks without a relevant change.

## Previous run ra03 — 25 October 2026

User requested continuing the next audit items, with minimal tests and no music,
analysis or running-session control. This pass repairs **AUD-D06/SUS-07** and
extends static SUS-01/SUS-04 evidence. Independent archives, exact failed/final
sources and candidates, reports and preservation records: `.local/ra03/`.

- Confirmed AUD-D06: reserved take-event kinds `pluginState` and `midi` passed
  take/session parsing while the replay switch ignored them, allowing silent
  omission from a supposedly complete take. The parser now rejects them before
  admission. Tests separately retain all nine supported event kinds; no invented
  editable MIDI-note or opaque plugin-state replay implementation was added.
- Two incremental builds (one worker, `/MP1`, below-normal) and two parser-only
  runs: red34/4 failures, final **34 checks/zero failures/infrastructure/retention
  errors**. Native runs used `verification.run_checks`, with no audio fixture,
  plugin/device or UI construction. No broader performance/bridge/full/stress/
  capacity/listening checks were repeated. Existing shadowing warnings persist.
- SUS-01: refreshed79-function lexical map and manually reviewed conditional
  dispatch. Outside the22-suite static closure:18 supplemental test entry points,
  2 nested helpers and2 excluded production/research commands. Review now explicitly
  separates split-effects-only from ordinary ownership, selector alternatives,
  optional real stem integration, catalogue reopening and installed-Vital branches.
  Old approximate dispatch candidates are not runtime coverage. No test oracle was
  rewritten merely because current workflows differ from older assumptions.
- SUS-04: statically traced5 take/private-control families. The take popup's171
  active IDs are disjoint at the32-take bound. Static routes/guards/history and
  remaining cases are in `performance-controls-reviewed.csv`; this is not native
  gesture or asynchronous admission acceptance. The281 original control obligations
  remain unpassed and incompletely expanded.

Only two application source files changed this pass. Version0.55.64,
analysis11/session41/retry3 remain unchanged. The new executable is **unstaged**;
the normal r646 launcher/binary and production session were not controlled.
ra02's74-check result belongs to its older candidate, not this one. All94 whole
rules remain **78 NOT RUN/16 BLOCKED/0 PASS/0 FAIL/0 N/A**; AUD-D06 adds partial
MW-D02/MW-P07 evidence only. Hardware/listening/package/requirements/rights remain
open. Ongoing production scan/recovery activity is preserved, never rolled back.

**Exact next work:** SUS-04/SUS-08: in a fresh owned fixture, open the take popup
or event-number dialog, change the take list/capture state through supported
commands before completion, then check stable target and admission on callback.
The menu captures numeric take indices, and its common completion guard checks
busy but not capture state; the first event-number dialog also retains only an
index. The later seek/edit dialogs do contain ID guards. This is a **static
suspicion**, not yet a reproduced defect. Continue SUS-01 conditional case/oracle
reconciliation separately. Recheck RAM before native work, avoid repeating the
passing parser checks without a relevant change, and leave production imports,
data, devices and running session alone.

## Previous run ra02 — 25 October 2026

User requested audit continuation/local repairs with minimal testing and no music,
analysis or running-session changes. SUS-06 is now confirmed and repaired as
**AUD-D05**; no other suspicion is promoted to a confirmed defect. Before/after
source ZIPs, exact candidate identities, original failures, run results and
preservation manifests are under `.local/ra02/`, independently of Git.

- Take parsing now rejects fractional frame/sequence counters and compares integer
  limits without double rounding. Integral numeric representations still work.
- Initial and structural-event project snapshots must pass the existing pure
  structural parser; nested take lists are rejected before recursive parsing.
  Failure leaves the caller's take unchanged, before media/plugin hydration.
- The parser rejects more than64 structural snapshots. The common capture append
  path now enforces the same limit for quantized private launches as ordinary
  structural commits, marks overflow incomplete and continues ordinary capture.
- Red parser run:19 checks,12 failures. First focused run:73 checks,one new-fixture
  failure because an empty programme correctly refused recording. The fixture now
  reuses the suite's tiny synthetic source. Final focused run: **74 checks,zero
  failures/infrastructure/retention errors**, through `verification.run_checks`.
  Includes the existing finite-delay VST, live/replay/export and save/reopen cases.
- Three incremental builds and native runs were sequential, below-normal with
  one worker and `/MP1`, stress disabled. No full regression, benchmark, capacity,
  hardware/listening or clean-package acceptance; no production app control,
  recovery/import resumption, device change, data reset, staging, commit or push.

The repaired executable is retained only as an **unstaged audit candidate**. The
normal launcher/staged r646 binary are unchanged. Analysis11/session41/retry3 and
the source version0.55.64 are unchanged; exact hashes distinguish these candidates.
All94 whole-rule statuses remain **78 NOT RUN/16 BLOCKED/0 PASS/0 FAIL/0 N/A**.
AUD-D05 supplies partial evidence for MW-D02/MW-P07/MW-P08; complete acceptance of
those rules is still open. Earlier candidate/tooling results remain historical.

**Next safe work:** continue SUS-01 mapping of supplemental/conditional tests and
SUS-04 dynamic control/oracle coverage. SUS-07 notes that reserved `midi` and
`pluginState` event kinds appear admissible although replay has no handler; this
is a static suspicion requiring a minimal reproduction and contract review, not a
claimed defect. Retain all human/requirements/rights blockers. Recheck resources
before any native/build work; no repeated tests without relevant changes. Leave
production sessions and572 deferred imports alone.

## Previous run ra01 — 24 October 2026

The user invoked RUN_AUDIT_PROMPT and authorized local audit/repairs. Scope was then
confirmed as **portable Windows x64 beta on this Windows 11 host; optional
integrations may be unavailable**. The owner chose to retain unresolved numerical
accuracy/performance thresholds. One final bounded stress/capacity checkpoint is
authorized by that prompt, but has **not** run; resource and requirement blockers
remain. No public distribution is authorized or cleared.

**Identity:** repository HEAD `b7e6c316379b28e6255b472a183ac5186078586d`, initially
clean; source 0.55.64/analysis11/session41/retry3. Exact Source/CMake hashes match
the retained r646 source manifest, with no added/missing/changed input. Staged
executable SHA256 `7cb413d25c190c4fd40ced8d7cf9998a6379148f107b8cf9c6602d44bc013fc5`
and launcher remain unchanged. Only documentation/build/verification tooling and
Python harness tests changed; no application rebuild or native run was needed
for these repairs. No commit, push, staging or package publication was performed.

Independent before-source ZIP/manifest/dirty patch and bundle identities:
`.local/ra01/source-before/`, `identity-before.json`. Completion source/docs/tooling
archive and manifest: `.local/ra01/source-after/`. Original failed attempts remain
retained beside accepted checks; never overwrite them when continuing.

| Confirmed case | Repair and verification |
| --- | --- |
| DISC-D01 / PASS | Reconciled README, docs index and affected guide claims with current version, independent insertion, implemented stems/Perform, soft activity/genre policies and focused/full testing policy. Dated historical evidence retained. Static claim table and document checks; this does not complete all R11 runtime/diagnostic/manual obligations. |
| DISC-D02 / PASS | build.ps1 now uses one worker, `_CL_ /MP1`, below-normal priority before children, and restores environment/priority/location on success or error. Bridge quick start uses that script and generated executable path. Five inert build-policy tests pass after the original four failed. No compiler/dependency download was run. |
| AUD-D03 / PASS | Retention failure could leave run_checks successful and be accepted by full/scoped receipts. Fault reproductions confirmed it. Overall run now fails while preserving native processExit, and both validators independently reject recorded retention errors/malformed metadata. |
| AUD-D04 / PASS | Missing/nonboolean stress evidence and missing native module arguments/identities could pass complete-receipt validation while assertions were omitted. Reproductions confirmed it. Require explicit Alloy stress true, four exact current module identities and required unique flags. Historical receipts remain unchanged and may fail current stricter validation; no retroactive release claim. |

Accepted **tooling-only** results: build-policy5, verification15, scoped11,
retention7 discovered = **37 passed, one skipped, zero failures**. The skip is the
Windows account's inability to create a directory symlink; hardlink and
reparse-root refusal tests pass. Suites were sequential; inert/fault fixtures do
not establish application acceptance. Exact commands, report hashes and retry
provenance: `.local/ra01/focused-tooling.json`. Small intermediate repair failures
(flag-variable collision and scoped-fixture invocation) are retained and corrected.

Static map:78 test functions,57 dispatch-option occurrences,22 named gate suites;
ten supplemental entry points are outside the static gate closure. Conditional
branches such as `--split-effects-only` still require case-level mapping even when
their parent function appears in the gate. Preliminary semantic ledger has281
control obligations, including90 generated MIDI actions and75 disjoint typed
menu IDs. **All control rows remain NOT RUN**; factories/modal/JUCE/dynamic-resource
expansion and independent oracle review are incomplete. Lexical inventories are
refreshed separately; their counts never establish runtime coverage.

Current per-rule ledger: **78 NOT RUN,16 BLOCKED,0 PASS,0 FAIL,0 NOT APPLICABLE**
across94 rules. Four repaired defect cases do not promote containing rules to
PASS. Both strict technical qualification and public distribution targets remain
unmet. Per-rule records, requirements, defects, suspicions, maps and evidence hashes
are under `.local/ra01/`; original discovery ledger below remains historical.

**Blockers:** MEM-01 recorded about1.3 GiB available physical RAM on the16 GiB host
before/after lightweight work. Clean build, app/model/native runs and final broad
work were withheld to protect responsiveness; no other app was closed to make
room. REQ-01 scope is resolved; clean-environment evidence remains ENV-01/REQ-06.
REQ-02/03/05 numerical/listening/performance limits remain unresolved by owner
choice; REQ-04 formal accessibility, REQ-07 manual scope, physical/listening evidence
and JUCE/ASIO/SoundTouch/other package rights remain open. Resource observations
are not invented acceptance thresholds.

**Preservation:** no production app/session/device/mapping/music/analysis/cache/
arrangement/recovery/reference mutation, cleanup/reset or bulk import recovery.
No Mixworks/compiler/build process was observed at preflight or checkpoint.
Only exact owned temporary harness fixtures were removed by their tests. No owned
jobs are left running. Current/previous bundles and all prior evidence remain.

**Exact next work:** investigate SUS-06 with a smallest malformed-take native
reproduction: fractional frame/sequence,65 structural snapshots and malformed/nested
project payloads in `readTake`, then trace open/prepare/replay side effects before
confirming and repairing scope. No application defect/fix is claimed from this
static suspicion. Start with `Get-Content Source/PerformanceTakeTests.cpp -TotalCount 120`
and the retained `.local/ra01/suspicions.json`; this is continuation, not a new
discovery pass. Recheck available RAM before scheduling any compiler/native child.
Continue remaining control/oracle/trust reviews while resource-heavy work is
blocked. Use fresh short owned roots and current trusted candidate, run native
checks only through run_checks, and save before graceful bridge close. Final
clean build/package freeze and the single authorized full checkpoint come after
repairs and safe resource preflight; do not repeatedly rerun broad checks.

## Historical discovery snapshot

24 October2026. **Discovery and audit-rule creation only. No application fixes, build, runtime/native regression, listening, hardware test, data cleanup, deployment or public release performed in this pass.**

## Source/evidence continuity

- User requested committing latest work first: commit`28cad136131856a01e62ad86b6a1e0c245232ba0`, branch`codex/beta-finishing-p01`,226 changed/new files, 30435 insertions/1935 deletions. Source checkpoint held358 files. No push requested/performed.
- Independent pre-audit exact-byte archive/SHA manifest, original dirty status and tracked patch: `.local/release-audit-20261024/pre-audit-checkpoint/`. Framework completion evidence and full updated checkpoint are retained alongside it; Git alone is not evidence retention.
- CMake0.55.64, analysis11, session41, retry3 observed in source. Launcher text selects0.55.64-7cb413d25c19. Retained focused report says85 passing checks (12retry/51performance/22preview) for historical executable SHA`7cb413d25c190c4fd40ced8d7cf9998a6379148f107b8cf9c6602d44bc013fc5`; **not rerun/revalidated as audit PASS**.
- All94 durable product/release rules start **80 NOT RUN,14 BLOCKED,0 PASS,0 FAIL,0 NOT APPLICABLE**. [Initial per-rule ledger](inventory/initial-status.csv) records one row each; it is a discovery snapshot, not an automatically updated execution ledger. Every future candidate uses separate result records. Confirmed static documentation findings remain open even though their containing product rules have not run in full.

## Requirement ambiguities / blockers

| ID | Decision/evidence missing and why release cannot assume it | Affected checks / next action |
| --- | --- | --- |
| REQ-01 | Exact release scope: supported Windows versions/CPU architecture/hardware/backends, portable package versus installer, required optional integrations. Only Windows x64 local build is evidenced. | R05/R07, hardware partitions; propose current compiled Windows product scope in README, obtain owner release envelope before claiming broader support. |
| REQ-02 | Independent real-music corpus/holdout size, acceptable BPM drift/beat error/downbeat class accuracy, coverage/abstention and octave-error policy not settled. Technical25/50ms stretch regressions do not define musical acceptability. | N02/N03/T03 general claims BLOCKED; use proposed synthetic metrics separately, preserve source-bound protected reviews and adjudicate disagreements. |
| REQ-03 | Key/tuning corpus accuracy, resample/stretch fidelity, non-deterministic plugin equivalence and stem/mix listening rubric need agreed limits. Numerical recombination is not perceived isolation or musical quality. | N05/A05/S06/X06 general acceptance BLOCKED; freeze independent references and rubric before candidate scores. |
| REQ-04 | Accessibility release promise/minimum keyboard/screen-reader behaviour beyond concrete current native actions unspecified; no blanket compliance standard inferred. | U03 native usability required now, explicit formal compliance claim BLOCKED until defined; inspect actual hierarchy/focus. |
| REQ-05 | Target host/workload/cancel/startup/import/analysis/export/RSS/disk bounds missing; user reports severe freezes. | R07/R08 and absolute responsiveness parts of D07/X04 BLOCKED. W profiles/durations proposed in RUNBOOK, no invented promise. No tests were launched to settle thresholds. |
| REQ-06 | Public package rights/entitlement/signing/update/distribution choices incomplete; JUCE/ASIO/SoundTouch and optional weight/transitive distribution evidence outstanding. | R10 BLOCKED; R05 package-specific requirements BLOCKED. Owner/licensing evidence needed, no agent agreement acceptance or release upload. |
| REQ-07 | Whether P42 full manual and which remaining finishing-plan items are mandatory release scope is unresolved. Roadmap is not all shipped functionality. | R11; current-facing claims must still be accurate. Establish scope without implementing unsolicited features. |
| ENV-01 | Physical FLkey/ASIO/private output, actual supported clean Windows environments and listening evidence unavailable in discovery; bridge/native fakes cannot establish these. | P03/P04/P09,A07,U04 physical partitions,R05,X06 BLOCKED where method unavailable. Arrange dedicated test opportunity; leave production alone. |
| POLICY-01 | Full gate/stress/capacity/benchmark forbidden in routine iterative work; complete gate needs explicit checkpoint authorization. | R08/final gate NOT RUN here. Future pasted execution prompt explicitly requests a bounded final checkpoint; absent that instruction keep BLOCKED and continue focused safe work. |

## Discovery findings — confirmed documentation/tooling evidence

These are static findings, not newly reproduced application defects. Severity does not permit unresolved in-scope defects at release.

| ID / status | Conclusive evidence, reproduction and impact | Rule / repair verification needed later |
| --- | --- | --- |
| DISC-D01 / confirmed, open / P2 | Read README's Local release0.55.46/session34/launcher path and “stem future”/private-performance unfinished claims, plus docs/README's current0.55.57/session38, compare CMake0.55.64, Model41, StemMixing/PerformanceCapture and current HANDOVER. Current front-page guidance conflicts with source/launcher. Other per-feature docs contain historical session values; distinguish dated history from current guidance. | R11/U08; reconcile current-facing claims, verify actual feature entry/version/format and links; retain dated history. This task only adds the audit index/continuity, not a product-documentation remediation. |
| DISC-D02 / confirmed, open / P2 | `scripts/build.ps1` directly invokes `cmake --build ... --parallel 3`; DEVELOPMENT-BRIDGE quick start also says3; CMake `/MP` can spawn additional workers. AGENTS/TESTING explicitly require1 and `/MP1` under user freeze override. Following unqualified quick start violates current operational policy. | R04/R07/R11; later fix or clearly guard instructions/launcher, inspect effective worker/priority flags and appropriate harness; RUNBOOK supplies safe method now without changing app/build tooling. |

No other current software defect is confirmed by this discovery pass. Do not interpret that as absence of defects. Future ledger must include candidate/fixture/steps/expected+actual/impact/scope/evidence/rule IDs and required post-fix regression, following RULES.

## Suspicions and historical findings needing investigation

| ID | Evidence/uncertainty | Exact next investigation |
| --- | --- | --- |
| SUS-01 | HANDOVER explicitly says old automatic-link assertions describe superseded defaults;22 named gate suites also do not inherently prove new takes/stems/retry paths. Existing tests may fail legitimately or omit key coverage; no current gate executed. | R09: map SelfTest selector→actual called functions/assertions, compare ordinary-independent placement contract, preserve red results and use independent requirements before adjusting tests. |
| SUS-02 | Engine callback and preview/private/take render descendants contain mutable containers/locks; source try-lock silences rather than waits. No fresh allocation/deadline profiling performed; can't declare real-time safe or defective solely from declarations. | A06/A11: static callback ownership/locking graph then smallest authorized representative trace. |
| SUS-03 | Prior user audio breakup and FLkey stale active-layout/root cause remain unproven; saved mapping hash did not prove active dispatch. | P03/A06/A07: isolated reproduction/raw input+active routing/device counters, no speculative fix or forced restart. |
| SUS-04 | UI lexical sites include declarations/factories and cannot establish all runtime accessibility, inert controls, disconnected handlers or unreachable states. | U01/U03/R12: expand C01–C23 native/control trace ledger, investigate unmatched sites and dynamic commands rather than assuming coverage. |
| SUS-05 | Generated/reconstructed Auftakt material and optional model/runtime transitive provenance need exact package reachability/rights classification. | R03/R10: follow compiled call graph and selected package manifest; don't infer distribution rights from local research/inference. |

Historical audits preserved: [product audit](../ALLOY_PRODUCT_AUDIT.md), [old coverage](../ALLOY_AUDIT_COVERAGE.md), [remediation](../ALLOY_REMEDIATION_PLAN.md), [editing audit](../EDITING-SYSTEM-AUDIT-20261022.md), [automation audit](../AUTOMATION-WORKFLOW-AUDIT.md), [finishing plan](../../FINISHING-PLAN.md). AUD-001/002 have recorded0.55.13 repairs and AUD-0030.55.23 admission repair; don't re-list old evidence as current confirmed failures or mark current candidate passed based on those notes. Recommendations/aesthetic preferences remain separate.

## Framework self-review

| Challenge | Resolution / remaining honest limit |
| --- | --- |
| Every first-party area? | Exact file registry, includes/consumer hints, source/CMake registration and fail-on-unclassified refresh. Ignored vendor/build/bundle roots have separate package manifests, not false line-review claims. |
| Every shipping journey/control? | J01–J10, C01–C23, lexical sites/CLI occurrences, dynamic expansion instructions and per-instance trace requirement. Actual live/control execution not claimed. |
| Boundaries/failure/cancel/recovery? | Dedicated handoff matrix; each rule has concrete cases and mandatory common post-failure retry/reopen/ownership steps. |
| Circular/weak oracles? | Analytic integration/source sample tags, independent decoder/null/corpus reviews/known native plugins; application `timing` and correction-seeded bridge smoke not detector truth. Listening/hardware explicit. |
| Vague/impossible criteria? | Numerical guards distinguished from provisional musical/performance thresholds; unresolved decisions BLOCKED, no retrospective threshold fitting. Plugin nondeterminism/history cannot be judged by inappropriate byte equality. |
| Unavailable hardware/tools disappear? | ENV-01/R05/P03/P09/X06 visibly BLOCKED; documented manual procedures rather than N/A or mock acceptance. |
| Historical/source drift? | Baseline/identity/invalidation contract, new-candidate result directories and explicit old-doc contradiction ledger. No test failure erased for obsolete expectations. |
| Release policy conflicts? | Future prompt makes explicit checkpoint request; discovery remains docs-only; ordinary repairs minimal/sequential; final stress/capacity never automatic without that explicit instruction. Public rights separate. |

## Exact continuation point

Framework source/semantic inventory and94 rule records are authored:380 repository files (358 baseline +22 framework items),1,082 lexical control sites and308 CLI-option occurrences. Fresh documentation checks:94 Markdown documents/515 local links/158 CMake source registrations, zero errors;94 unique rules have contract/cases/oracle/evidence-mode/severity fields, ledger membership/status validation succeeds and Python/JSON parse checks succeed. No application source/build/script/test/licence change against28cad13. Final check output/hashes and independent completion archive are retained under `.local/release-audit-20261024/framework-verification/` and `completion-checkpoint/`. The earlier incomplete authoring check (initial-status file not yet created) is retained separately. This is framework validation, not product R12/PASS or any application acceptance.

Next **authorized execution session**: paste RUN_AUDIT_PROMPT, read updated instructions/HANDOVER/STATUS, record clean/dirty source/candidate identities, refresh/compare inventory, establish a new `.local/raNN` run and full result ledger, then R09 static suite/requirement mapping plus U01 dynamic control expansion and DISC-D01/D02 reproductions. Fix only after that execution authorization. Resolve REQ/ENV blockers while continuing independent safe work. Do not resume572 production imports, attach production bridge, clear/reanalyse user data, overwrite protected reviews, restart app, build or run full gate merely because discovery is complete.

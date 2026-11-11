# Proportionate testing

11 November .96 passes **587 focused checks**: live103, bars34, ranges184,
direct-range69, Unlink19, native-effects31, export57 and seek90. Actual live
Duplicate and stopped range Crop/Undo/Redo/save/reopen also pass. Diagnostic .93,
interrupted .94 and the .95 stale expected-value failure remain separately
retained; final identities, counts and limits are in [Handover](../HANDOVER.md).

The original native waveform-click/Delete assertions also exposed retained group
scope after a click. The fix clears only transient selection on click; those
assertions remain unchanged. Group-drag fixtures use the actual title gesture.
The interrupted .94 compile produced no new executable; its source/log evidence
is retained separately from the .93 failures and final .95 candidate.

The group PCM comparison exposed a preexisting one-sample start-boundary defect
on both .92 and diagnostic .93. Source admission/output now share the existing
half-open sample-boundary policy. Exact first/ending samples and a real half-
sample placement are checked without relaxing the original PCM tolerance.
Diagnostic range failures, unchanged .92 provenance and obsolete gesture/link/
canonical-control fixture corrections are recorded in [Handover](../HANDOVER.md).
The Unlink fixture now explicitly creates links after Split, checks setup before
access and retains its PCM/Undo assertions; Split does not invent transitions.

Duplication/range extensions add independent callback PCM/clock observations at
Duplicate refresh and its processor-state capture boundary, plus stale-document
refusal. `--edit-selection-only` checks source-grid audition PCM during stopped
Crop, removal, Repeat, Paste, Replace and group-move refresh. The programme clock
correctly stays stopped while the audition source advances. `--split-effects-only`
uses the real native echo fixture for independent Duplicate processor/state and
Undo ownership, alongside its existing shared-effect PCM/tail/reopen coverage.
The fixture module and hash are frozen independently of the build cache. Current
candidate results and limits remain in [Handover](../HANDOVER.md).

11 November .92 passes live98, bars34, seek90 and export57: **279 focused native
checks**, serial retained runs, stress off. The exact bundle's actual move, split,
Undo/Redo, removal and save/reopen journey passed with muted device playback and
moving Studio pixels (6,754 callbacks, no added misses/waits/overruns). .91's three
take assertions failed because admission blocked default Latch in idle
Record/Perform; .92 corrects that product conflict and preserves the assertions.
See [Handover](../HANDOVER.md) for identities, evidence and remaining limits.

The structural-edit extension to `--live-transition-only` probes independent,
nonzero callback PCM and exact clock advancement at track-move, split, removal,
Undo/Redo refresh and redo-resource retirement. It also checks source-backed
save/reopen, failed source preflight, atomic admission (including queued launches)
and take events appended after a candidate snapshot. The held-lock negative
control remains. Large Workstation/Engine owners in this shared dispatcher use
heap allocation to avoid repeating the native export-test stack failure.
This bounded selector does not qualify all structural paths or elastic-window
preparation, physical devices or sustained listening. Current candidate results
and desktop evidence are recorded in [Handover](../HANDOVER.md).

10 November .90 repairs the shared native export-test stack frame using heap
owners (121,424 bytes, previously 876,416; the 1 MiB process stack is unchanged).
Final export57, destination111, export-target75, bars34, live74 and seek90 pass
serially through retained checks, stress off. `--bar-navigation-only` runs the
bounded native bar-menu/tempo/guard/history checks without the broader timeline
suite. The live selector now probes actual callback PCM at bar-jump UI refresh
as well as node editing, retaining its held-lock negative control. The .89 bar32/2
fixture failures survive separately; final setup explicitly uses the movement
lock and canonical effect controls. See [Handover](../HANDOVER.md).

10 November .88 focused checkpoint: menu11, seek90, live72, Studio26, composite94,
shaped105 and bridge39 passed, plus actual GL24 and live bridge45. Normal UI WAV
export/reopen/PCM and actual large-library menu response were demonstrated. The
separate native save/export fixture exhausts its stack identically on .88 and .86;
its failed reports are retained and no native export42 result is claimed. See
[Handover](../HANDOVER.md) for exact scope and remaining acceptance. Earlier
source-only and candidate-specific notes below are historical.

10 November seek/menu checkpoint: `--seek-continuity-only` is a bounded 90-check
entry returning before broader streaming checks. .87 passed it, live72, Studio26
and 24 actual-GL checks, plus owned live seek/scene/ruler observations. Source now
includes `--beat-menu-only` (11 metadata-only checks at the observed catalogue
count) and repaired native bridge hierarchy tests; those await the next linked
build at Jay's request. MSVC syntax-only checks are separate evidence, not an
executed native suite or new app build. See
[checkpoint](PHASE1-SEEK-CONTINUITY-20261110.md) and [HANDOVER](../HANDOVER.md).

10 November phase-1 direct fades: `--shaped-fades-only` is a bounded 105-check
branch returning before the broader preset suite. It uses one owned 24-second,
8 kHz source for direct gain/power/EQ/filter formulas, two-node counts, tempo
mapping, insertion/slicing, hard cuts, preset placement, source-backed reopen,
Undo and export-rendered PCM. 0.55.84 also passes `--live-transition-only`72/0.
Both used `verification.run_checks` serially, stress off, clean retention. One
temporary source copy was used for the subsequent isolated actual UI measurement,
then removed after save/close. Actual mouse endpoint movement retained two saved
nodes, and UI Undo restored the exact original curves. These results do not close
intermittent device playback or composite fades and are not a complete gate.
Evidence and failed-fixture provenance: `.local/phase1-fades-1110`; current scope
and launcher identity: [HANDOVER](../HANDOVER.md).

Current visuals: [glass/maintenance checkpoint](MAINTENANCE-20261103.md) and
[verification entry points](../VERIFICATION.md). The dated records below apply to
their named candidates; current status is in [HANDOVER](../HANDOVER.md).

Latest: [ua20 versioned activation](release-audit/UA20-VERSION-ACTIVATION-20261101.md)
passes waveform/version37/0 on `708a26477c5d` with actual UI and Windows metadata
both0.55.66. Source is identical to ua19 except CMake version; its109/0 remains
separate prior-candidate evidence. User-authorised local preview activated;
no full regression, stress, physical/device or real-track listening acceptance.

Latest: [ua19 Simple A/B/C workflow](release-audit/UA19-SIMPLE-ABC-GRID-20261101.md)
passes `--grid-alignment-only`109/0 on candidate `91f5a141564e` through
verification.run_checks. Ten native captures inspected; no stress/full gate or
physical/listening acceptance. Includes fixed-marker navigation, ordered marks,
bar-count confirmation, independent C mismatch, source save/reopen and Advanced
switching. Broader grid/key test callers select the secondary tools explicitly;
those broader suites remain unrun on this candidate.

Latest: [ua18 playback boundary repair](release-audit/UA18-PLAYBACK-BOUNDARY-20261101.md)
passes `--transition-boundary-only --boundary-fixture <absolute audio path>`5/0
and `--original-playback-only`42/0 on candidate `3c1e3f3eb943`, after reproducing
the MP3 EOF failure. The boundary selector copies its input into the owned report
root. Encoded MP3 copies need exact-path hash-journalled cleanup separately from
the shared generated-audio matcher. No full gate or hardware/listening acceptance.

Latest: [ua16 waveform/grid controls](release-audit/UA16-WAVEFORM-GRID-CONTROLS-20261101.md)
passes `--waveform-alignment-only`36/0 and `--grid-alignment-only`76/0 on candidate
`e1d1c5737f7b`. Both are bounded functional entries through `verification.run_checks`;
no detector/model/device or stress/performance work. The waveform selector returns
before the broader BeatGrid suite. Twelve native captures inspected. Shared stream/
model changes still need broader regression at a later authorised checkpoint;
these focused results do not qualify whole-product readiness. ua15's recovery67/0
remains separate evidence on its own candidate.

Latest: [ua13 combined focused checkpoint](release-audit/UA13-FOCUSED-VERIFICATION-20261101.md)
builds successfully and passes grid61/0, review23/0, destination111/0 and
save-export42/0 on candidate `3a8286068928`. Twelve native captures inspected;
all four runs used `verification.run_checks` serially with no retention errors.
This supersedes the ua09–ua11 pending notes below within the stated focused scope.
No complete gate or physical/listening acceptance is implied.

**1 November: RAM guard removed at Jay's request.** Available-memory thresholds
must no longer block or kill builds/tests. Older memory-stop records below are
historical, not current prerequisites. The active ua09-ua12 wrappers have no RAM
gate; their original guard scripts are preserved in
`.local/ua12/previous-memory-guards.zip`. Continue one-worker, below-normal-priority
builds and serial focused checks through `verification.run_checks`, with stress
off. Stop owned work for observed responsiveness problems, not a RAM reading.

ua11 extends `--grid-alignment-only` with guided page visibility/draft preservation,
whole-beat bar-accent shifts, rebuild confirmation cancellation/acceptance and
three guided page captures. Existing two-anchor arithmetic, Undo, bounds and
source-save/reopen checks remain. The initial1497MiB memory stop was superseded
by ua13's successful build and61-check pass; see
[ua11](release-audit/UA11-GUIDED-GRID-20261101.md). Use this bounded entry, not a
detector run or full grid-correction suite, at the next safe checkpoint.

ua10 extends `--review-sorting-only` with accepted-key score boundaries,
tentative/invalid/manual classifications and a low-score accepted key in the
native column capture. Build and run this bounded selector through
`verification.run_checks`, then inspect `independent-review-columns.png`.
ua13 passes23 checks and its native capture was inspected; do not run analysis
or a library scan to verify this display-only classification change.

`--destination-safety-only` adds a bounded companion to the save/export journey:
extension-corrected overwrite confirmation/cancel, existing destinations and
backups, arrangement-only source/hardlink protection, missing folders, Windows
locked-file failures followed by retry, and mid-write cancellation. It shares the
0.2-second fixture and returns before the broader take/export checks. Run through
`verification.run_checks`; do not substitute the broad export suite. ua13 passes
111 checks and its confirmation/error captures were inspected; see
[ua09](release-audit/UA09-DESTINATION-SAFETY-20261101.md).

`--save-export-workflow-only` adds a bounded native Save As/Save/reopen/normal WAV/
playlist/track-list/cancel journey. It uses one 0.2-second source, an independent
2,000-frame output oracle and four dialog captures, returning before broader take
tests. ua08 passes42/0 via verification.run_checks; see
[scope and limits](release-audit/UA08-SAVE-EXPORT-20261101.md). Do not substitute
`--export-only`, which includes a 90-minute callback simulation, during the current
minimal-check phase. No physical gestures or whole-export qualification implied.

1 November ua02 adds `--live-transition-only`:29 bounded native UI-callback,
sample-clock/PCM, history, source-backed save/reopen and export-renderer checks.
Run through `verification.run_checks`; it returns before broader navigation checks
and uses no device, detector, worker, timing/load/benchmark loop. Exact final and
failed-fixture evidence: `.local/ua02/`. This does not qualify physical gestures,
listening, complete file export or wider Record/Perform; see the
[live-transition checkpoint](release-audit/UA02-LIVE-TRANSITIONS-20261101.md).

## Current override: protect desktop responsiveness (23 October 2026)

The user reports severe freezing during tests and is making successive requests. During ongoing development, use only essential focused verification, batch related edits, and defer builds/checks until a useful checkpoint instead of rerunning after every request. This overrides automatic full-gate triggers in the table below. Do not automatically run broad regressions, stress probes, capacity fixtures or benchmarks. A complete gate is reserved for an explicitly requested release/verification checkpoint; deferred coverage must remain visible and focused checks must never be called complete regressions.

Build with `--parallel 1` and append `/MP1` to the Windows `_CL_` environment variable for that build (the project otherwise enables MSVC internal parallel compilation). Run the build below normal priority. Run one test process at a time, without overlapping builds. `verification.run_checks` launches native children at below-normal priority on Windows. It explicitly disables `MIXWORKS_TEST_STRESS` unless `include_stress=True` is supplied, even if the parent environment enabled it. The Alloy playback-budget probe (busy CPU worker, audio callbacks and full-window snapshots) is now opt-in. The full-gate CLI requires `--include-stress` when requesting a complete receipt. Older archived Alloy executables do not honour this new guard: avoid running them during interactive work. Interactive bridge drivers retain their own process-launch policy and should only run when essential.

Reduced process priority does not cap RAM or disk traffic and does not prove the freezes resolved. Stop owned testing if responsiveness degrades. Do not restart production apps, clear user caches or change system/audio drivers as part of this policy.

`scripts/build.ps1` now enforces one worker, `/MP1` and below-normal priority,
including dependency/configuration children, and restores caller settings on
success/failure. Disk-space checks remain; numeric RAM admission/kill guards are
disabled by Jay's1 November instruction. Current full
receipts require explicit Alloy stress evidence and matching native module
identities/arguments; absent optional plugin flags cannot silently omit required
checks. Retention errors make a run unsuccessful and block full/scoped receipts,
even when the native checks passed. Historical receipts are retained as historical
evidence and may not satisfy current validation.

The ra06 coverage repair raises the named gate to23 suites: split-effects is a
separate early-return branch and requires the finite-delay fixture rather than
the ordinary ownership gain fixture. Runner/receipt validation share fixture
roles and reject unreviewed options such as `--catalogue-reopen` under a complete
library-suite label. Historical22-suite receipts stay historical. No revised full
gate was run;31 harness tests and29 focused native checks are scoped evidence only.

The user requested this policy on 20 October 2026 because small visual edits were repeatedly running thousands of unrelated checks. Select the smallest verification that addresses the actual risk. An application rebuild alone does not require the full gate.

| Change | Required verification | Full gate? |
| --- | --- | --- |
| Documentation | Relevant links/source registration and review of changed claims | No |
| Colour, font, label, spacing, paint-only styling | Build; inspect affected UI at relevant sizes; retain a capture and brief review note | No |
| Local UI behaviour, selection, dragging, resizing, menus | Build; exercise affected workflow; relevant focused native suites | No, unless shared engine/data behaviour also changes |
| Verification/build/cleanup scripts | Relevant harness tests; small integration check when needed | No app-wide run for script-only changes |
| Local development bridge | Client/harness tests; native and live `scripts/verify-bridge.py`; inspect relevant captures | Also run full gate if startup isolation, engine/cache ownership or shared persistence changes |
| Audio/DSP, beat/grid/timing, analysis, engine/cache/ownership, persistence/migration, dependency/build configuration | Focused reproduction, then complete regression gate on the final candidate | Yes |
| Broad refactor, uncertain cross-cutting impact or unexplained regressions | Investigate and run complete gate before accepting | Yes |
| Explicit release milestone | Complete gate once on its final candidate; listening/hardware checks where relevant | Yes, once |

Presentation-only means the diff does not change events, gestures, model state, parameter conversion, persistence or audio routing. A UI filename can still contain those behaviours. Review semantics before selecting scope. Do not add a unit test just to assert that a colour constant changed. Combine related changes before verifying; stop once relevant checks pass unless new changes or concerns justify more work.

## Bridge-assisted workflows

The ra05 local staging checkpoint uses47 private-popup checks,29 take-UI checks,
87 performance/replay/export/parser checks with the finite-delay VST, and22 preview
checks:185 total, sequential `verification.run_checks`, stress disabled, all on
the staged `0.55.64-66ebcab993b3` candidate. Same source/version formats are retained.
This is an explicitly requested focused local stage, not a complete gate or public
package qualification. No tests were repeated after the accepted final checkpoint.
`--private-popup-only` returns from the existing performance friend-test entry;
owned worker gates and isolated PCM/analysis exercise late completion without a
model, production catalogue or physical device. Evidence: `.local/ra05/`.

The 25 October ra02 audit adds `--performance-take-parser-only` for the smallest
no-audio parser reproduction. Those checks also run under `--performance-take-only`.
Final 74-check performance acceptance (including the real finite-delay VST fixture)
belongs to the unstaged repaired candidate in `.local/ra02/final.json`, not the
normal r646 launcher, and is not a complete gate. Red and fixture-failure reports
remain retained. No bridge change or bridge suite was needed for this parser fix.

The next ra03 repair uses only `--performance-take-parser-only`:34 checks cover
unsupported-event refusal, all nine supported kinds and the earlier malformed
snapshot/counter cases. It does not repeat the older ra02 performance suite or
inherit its74-check result for the new executable. See `.local/ra03/final.json`.
Static coverage/conditional maps distinguish18 supplemental entries,2 nested
helpers and2 excluded production/research commands outside the named gate;
mapping does not authorize running these tests or establish runtime acceptance.

The ra04 `--performance-take-ui-only` branch runs29 targeted native take-popup/
modal checks inside the existing performance friend-test entry, then returns
without running its DSP/plugin suite. It uses Workstation(false), stops its timer,
opens no device/catalogue/production recovery, completes real owned modal callbacks
and exercises Undo plus Play with one tiny owned source. A test-only sentinel take
keeps the faulty red capture-index case in bounds. Final29/0 and inspected popup/
editor PNGs are retained in `.local/ra04/`; generated WAV retention removed48,104
bytes without errors. No physical gestures, MIDI, output or full seek-thread/native
file-picker journey is established, and ra02/ra03 results remain historical.

The 0.55.64 mode/retry checkpoint uses `--targeted-analysis-only`,
`--performance-take-only --plugin <finite-delay-fixture>` and
`--library-preview-only` through `verification.run_checks`, sequentially.
Accepted `.local/r646/evidence/focused.json` binds the exact source/bundle and
12 + 51 + 22 checks, with zero failures or retention/infrastructure errors.
Includes live/replay/export PCM, pause tails, ending-boundary hotcues, shared
split-track controls, independent private routing, writer refusal and per-field
retry preservation. Fake devices and inspected native captures do not establish
physical MIDI/ASIO, listening, long RF64 capacity or full regressions. The
production missing-import recovery is a separately checkpointed data operation,
not a regression test or evidence that all missing sources recovered.

Use the [development bridge guide](DEVELOPMENT-BRIDGE.md) for real isolated-app setup, state assertions and screenshots. `scripts/verify-bridge.py` runs native protocol checks and a live import/control/Undo/save/reopen/transport/restart scenario through `verification.run_checks`. Interactive drivers share standard reports and cleanup, but their `interactive-bridge` invocation marker prevents them being accepted as native full-gate suites. Bridge evidence supplements the affected workflow and does not replace UI gestures, hardware/listening acceptance or the complete gate when required. Normal builds and unrelated cosmetic edits do not automatically require this suite.

## Local UI deployment

`scripts/change_verification.py` produces a scoped receipt accepted by `scripts/deploy.ps1`. It freezes all Source/CMake inputs and binds the exact executable, model/runtime, visual capture, review note and focused reports. It compares against a previously accepted **full** baseline, rejects changes outside an explicit UI/test-file allowlist and permits only a version-number change in CMake. Changes accumulate against that full baseline: a scoped receipt cannot become a full baseline. Semantic review remains necessary; filenames cannot prove a cosmetic change. One narrow exception accepts only removal of the section-label question-mark suffix in EnergySections.cpp; any other diff in that file requires full verification.

Example after building and capturing the changed candidate:

```powershell
python scripts/change_verification.py --scope presentation --exe $candidateExe --baseline-receipt .local/cutoff-filter-0556/receipt.json --baseline-source .local/cutoff-filter-0556/candidate-source.json --baseline-archive .local/cutoff-filter-0556/candidate-source.zip --visual-proof $capturePng --note 'Button colour only; checked normal, hover and disabled states at compact and wide sizes.' --output .local/button-colour-verification
```

For interaction changes use `--scope ui-behaviour` and one or more `--suite` options: `window`, `alloy`, `tempo`, `tempo-browser`, `timeline-editing`. Select suites covering the changed path and exercise the actual workflow. Presentation receipts can include focused suites when justified. Unavailable/changed baseline reports or bundle assets prevent reuse. New source membership, engine changes or different analysis dependencies require [complete verification](../VERIFICATION.md).

Scoped acceptance covers the stated UI change and selected checks; it does **not** claim all regressions, physical MIDI/ASIO or musical listening were reverified. Deployment checks candidate hashes and retains immutable release/launcher backups. It does not restart the application.

StemUI.cpp has a single exact exception for the0.55.47 caption's UTF-8 bullet
to ASCII separator repair. Any additional change in that file still requires
full verification; it is not broadly classified as presentation-only. Harness
coverage rejects accompanying job changes and new source membership.

For an environmental failure, rerun only that failed suite on the identical bundle after resolving the cause. Preserve the original result and retry provenance in accepted combined evidence. Never relabel a failure or reuse checks from another executable.

The Escape appearance-restoration check permits at most one changed pixel with
each RGB channel differing by no more than 3/255. Alpha and image dimensions must
match exactly. The user authorised this narrow tolerance on 23 October 2026 to
avoid reruns for the recurring negligible curve-edge variation. Captures remain
available when pixels differ; this accepted variation needs no retry or escalation.
Larger visual differences still fail. Lighting state, audio, timing and saved-state
checks remain exact. Comparator boundary checks cover the observed variation and
reject multiple pixels, larger channel differences, alpha changes and missing or
resized captures. This is a test-only follow-up to the complete 0.55.48 baseline;
verify its build and affected Alloy suite, without repeating the full gate.

## Test data retention

Ordinary native runs use `verification.run_checks` and a new empty directory below this repository's `.local/`. Each directory has an ownership marker; only outputs created in that fresh owned run are eligible for automatic removal. Child `TEMP`/`TMP` point inside the run, containing transient decoded caches. After the child exits, generated WAV/PCM, related raw audio, their `.bak`/numbered backups and generated `.mixworks` test arrangements/backups are hashed, journalled and removed. The 24 October cleanup adds audio-backup matching while retaining report JSON, outside files and hardlinks. Reports, analysis JSON/measurements, screenshots, binaries and source archives remain. Production arrangements and saved analysis outside the owned test run are untouched.

Links/reparse points and hard links are excluded. Existing nonempty directories cannot acquire ownership; source-baselines/dependencies/archives and paths outside `.local/` are forbidden. Low free space stops the full gate before its capacity fixture is generated; it never clears user data to make room. Retention errors are reported separately from native results.

`run-real-benchmark.py` explicitly retains audio needed for subsequent measurement/research; its existing successful analysis path discards full decoded WAVs after extracting measured heads. Research, interrupted runs and output deliberately placed outside `.local/` are exceptions, not automatically swept. A hard interruption can leave audio behind: inspect ownership/journal and apply exact-path checks before later cleanup. Do not start recurring broad deletion jobs.

Historical cleanup preserves real arrangements and every discovered audio reference from those arrangements/library/AppData, research/reference material, all source checkpoints and compact verification evidence. The user subsequently authorised deleting generated test arrangements: exact names/dynamic scenarios are matched to native producer code, reports and UUID fixture directories; their references do not make test audio permanent. Unknown arrangements remain protected. Original music, production analysis/corrections, audio caches, MIDI profiles and recovery are outside cleanup targets.

Removal manifests live in `.local/test-data-retention-20261020/` and `.local/test-cleanup-final-20261020/`. The latter records exact duplicate build assets and a byte-identical retained counterpart for each. Current/previous accepted release bundles, dependencies and unique binaries are retained. Older bundles may require reconstructing retired duplicate assets with that folder's `restore_duplicate.py --path` command before rerunning historical receipts. Preserve the mapping and canonical copies; do not recreate hundreds of full model/runtime bundles for small changes.

The reviewed UI classification also includes the existing `PlaylistTransitions.cpp` cell/picker factory. This allows its native accessibility presentation change to use a scoped receipt; semantic review and source-membership rejection still apply.

`TimelineHeader.cpp` is also a reviewed UI classification for native ruler/label accessibility. Classification does not waive semantic review, engine/source-membership rejection or relevant native workflow checks.

`TempoStrip.cpp` joins this UI classification for native tempo-marker discovery/selection and existing-editor admission. Check the tempo workflow and actual native hierarchy; this does not classify tempo-map/model/DSP changes as UI-only.

The 0.55.46 plain-text review also classifies the existing GridCorrectionUI.cpp/.h, SourceSectionUI.cpp, AutoArrangementUI.cpp, TransitionUI.cpp, TransitionRehearsalUI.cpp, HotcueUI.cpp, SectionUI.cpp, MasteringUI.cpp and EditSelection.cpp. This increment changes their wording, hints and layout only. EditSelection.cpp also contains selection/edit logic: any future behavioural change there still requires its affected suites and semantic review; this classification does not waive full-gate triggers for shared state, persistence or audio.

The 0.55.49 arrangement UI update also classifies existing TimelineEditing.cpp for scoped UI verification. The change seeks the existing playhead after a successful paste and preserves exact consecutive paste positions; it does not modify the engine, grid conversion, audio routing or saved format. Run the affected timeline-editing and Alloy suites and inspect the actual shortcut/VST and header UI. This filename also contains substantive edit behavior: later shared engine, grid or format changes still require the full gate.

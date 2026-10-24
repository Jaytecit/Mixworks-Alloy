# Proportionate testing

## Current override: protect desktop responsiveness (23 October 2026)

The user reports severe freezing during tests and is making successive requests. During ongoing development, use only essential focused verification, batch related edits, and defer builds/checks until a useful checkpoint instead of rerunning after every request. This overrides automatic full-gate triggers in the table below. Do not automatically run broad regressions, stress probes, capacity fixtures or benchmarks. A complete gate is reserved for an explicitly requested release/verification checkpoint; deferred coverage must remain visible and focused checks must never be called complete regressions.

Build with `--parallel 1` and append `/MP1` to the Windows `_CL_` environment variable for that build (the project otherwise enables MSVC internal parallel compilation). Run the build below normal priority. Run one test process at a time, without overlapping builds. `verification.run_checks` launches native children at below-normal priority on Windows. It explicitly disables `MIXWORKS_TEST_STRESS` unless `include_stress=True` is supplied, even if the parent environment enabled it. The Alloy playback-budget probe (busy CPU worker, audio callbacks and full-window snapshots) is now opt-in. The full-gate CLI requires `--include-stress` when requesting a complete receipt. Older archived Alloy executables do not honour this new guard: avoid running them during interactive work. Interactive bridge drivers retain their own process-launch policy and should only run when essential.

Reduced process priority does not cap RAM or disk traffic and does not prove the freezes resolved. Stop owned testing if responsiveness degrades. Do not restart production apps, clear user caches or change system/audio drivers as part of this policy.

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

Ordinary native runs use `verification.run_checks` and a new empty directory below this repository's `.local/`. Each directory has an ownership marker; only outputs created in that fresh owned run are eligible for automatic removal. Child `TEMP`/`TMP` point inside the run, containing transient decoded caches. After the child exits, generated WAV/PCM, related raw audio and generated `.mixworks` test arrangements/backups are hashed, journalled and removed. Reports, analysis JSON/measurements, screenshots, binaries and source archives remain. Production arrangements and saved analysis outside the owned test run are untouched.

Links/reparse points and hard links are excluded. Existing nonempty directories cannot acquire ownership; source-baselines/dependencies/archives and paths outside `.local/` are forbidden. Low free space stops the full gate before its capacity fixture is generated; it never clears user data to make room. Retention errors are reported separately from native results.

`run-real-benchmark.py` explicitly retains audio needed for subsequent measurement/research; its existing successful analysis path discards full decoded WAVs after extracting measured heads. Research, interrupted runs and output deliberately placed outside `.local/` are exceptions, not automatically swept. A hard interruption can leave audio behind: inspect ownership/journal and apply exact-path checks before later cleanup. Do not start recurring broad deletion jobs.

Historical cleanup preserves real arrangements and every discovered audio reference from those arrangements/library/AppData, research/reference material, all source checkpoints and compact verification evidence. The user subsequently authorised deleting generated test arrangements: exact names/dynamic scenarios are matched to native producer code, reports and UUID fixture directories; their references do not make test audio permanent. Unknown arrangements remain protected. Original music, production analysis/corrections, audio caches, MIDI profiles and recovery are outside cleanup targets.

Removal manifests live in `.local/test-data-retention-20261020/` and `.local/test-cleanup-final-20261020/`. The latter records exact duplicate build assets and a byte-identical retained counterpart for each. Current/previous accepted release bundles, dependencies and unique binaries are retained. Older bundles may require reconstructing retired duplicate assets with that folder's `restore_duplicate.py --path` command before rerunning historical receipts. Preserve the mapping and canonical copies; do not recreate hundreds of full model/runtime bundles for small changes.

The reviewed UI classification also includes the existing `PlaylistTransitions.cpp` cell/picker factory. This allows its native accessibility presentation change to use a scoped receipt; semantic review and source-membership rejection still apply.

`TimelineHeader.cpp` is also a reviewed UI classification for native ruler/label accessibility. Classification does not waive semantic review, engine/source-membership rejection or relevant native workflow checks.

`TempoStrip.cpp` joins this UI classification for native tempo-marker discovery/selection and existing-editor admission. Check the tempo workflow and actual native hierarchy; this does not classify tempo-map/model/DSP changes as UI-only.

The 0.55.46 plain-text review also classifies the existing GridCorrectionUI.cpp/.h, SourceSectionUI.cpp, AutoArrangementUI.cpp, TransitionUI.cpp, TransitionRehearsalUI.cpp, HotcueUI.cpp, SectionUI.cpp, MasteringUI.cpp and EditSelection.cpp. This increment changes their wording, hints and layout only. EditSelection.cpp also contains selection/edit logic: any future behavioural change there still requires its affected suites and semantic review; this classification does not waive full-gate triggers for shared state, persistence or audio.

The 0.55.49 arrangement UI update also classifies existing TimelineEditing.cpp for scoped UI verification. The change seeks the existing playhead after a successful paste and preserves exact consecutive paste positions; it does not modify the engine, grid conversion, audio routing or saved format. Run the affected timeline-editing and Alloy suites and inspect the actual shortcut/VST and header UI. This filename also contains substantive edit behavior: later shared engine, grid or format changes still require the full gate.

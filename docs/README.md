# Mixworks documentation

Current state: [handover](../HANDOVER.md), [app guide](../README.md),
[visual direction](VISUAL-DIRECTION.md), [roadmap](../FINISHING-PLAN.md) and
[verification](../VERIFICATION.md). Alloy 0.55.133 bundles Visual Studio 0.1.33
and 1,000 Studio GLSL scenes. The handover records the exact local activation
and candidate hashes.
[The roadmap](../FINISHING-PLAN.md) is the single outstanding-work queue;
[the 4 November audit](DOCUMENTATION-AUDIT-20261104.md) records reconciliation.

The [3 November audit](MAINTENANCE-20261103.md) consolidated obsolete documents and
retired external-renderer/prototype checks. Historical research, requirement detail,
licensing records and dated failed/passing evidence remain preserved. Dated findings
apply to their original candidates; they are not instructions to resume old work.

## Current guides

The [13 November functional audit](AUDIT-20261113.md) traces the main workflows
and their remaining acceptance. The [GLSL control audit](GLSL-CONTROL-AUDIT.md)
records the scene-control work. The [14 November promo record](PROMO-20261114.md)
documents the locally reviewed film and its source provenance.

Product audit, 21 October 2026 (0.55.12; audit only): [findings](ALLOY_PRODUCT_AUDIT.md), [interface/workflow coverage](ALLOY_AUDIT_COVERAGE.md), [prioritised remediation](ALLOY_REMEDIATION_PLAN.md). The audit records runtime/source evidence and remaining acceptance gaps; it does not mark planned features implemented.

Editing comparison,22 October2026 (0.55.42; audit and plan only): [editing audit against DJ.Studio](EDITING-SYSTEM-AUDIT-20261022.md), [parity implementation plan](DJSTUDIO-EDITING-PARITY-PLAN.md). Existing editing/stem implementations are credited; remaining scope is in W02–W03 and W17–W21 of the roadmap.

| Task | Guide |
| --- | --- |
| Library, analysis retention and cached loading | [Loading and analysis](LOADING-AND-ANALYSIS.md) |
| Arrange master preview and Record/Perform private preparation | [Preview routing](LIBRARY-PREVIEW.md) |
| Import errors, cache warnings and durable per-file reports | [Import reports](IMPORT-SCAN.md) |
| Bounded per-field BPM/key retries and retained evidence | [Focused analysis](FOCUSED-ANALYSIS.md), [retry methods](ANALYSIS-RETRY-OPTIONS.md) |
| Create or extend an automatic arrangement | [Auto Arrange](AUTO-ARRANGEMENT.md) |
| Measured import and warm/cold performance | [Import timings](IMPORT-ANALYSIS-TIMINGS.md), [0.48 performance](LOADING-PERFORMANCE-048.md) |
| Detection findings and independent accuracy limits | [Rhythm timing review](RHYTHM-TIMING-REVIEW.md) |
| Timeline editing, slipping and audio cutoffs | [Timeline editing](TIMELINE-EDITING.md) |
| Direct audio ranges, multiple segment selection and scoped clipboard edits | [Range editing](RANGE-EDITING.md) |
| Reusable source section boundaries and independent hotcues | [Section flags](SECTION-FLAGS.md), [hotcues](HOTCUES.md) |
| Save a downbeat and preserve manual-versus-automatic comparisons | [Downbeat references](DOWNBEAT-REFERENCES.md) |
| Drag source-grid anchors, calculate BPM and make precise timing corrections | [Source grid alignment](SOURCE-GRID-ALIGNMENT.md) |
| Identify a source key and compare segment pitch by listening | [Key and harmony by ear](KEY-AND-HARMONY-BY-EAR.md) |
| Fixed ruler, overview, lane visibility and resizing | [Timeline editing](TIMELINE-EDITING.md), [layout background](TIMELINE-LAYOUT-049.md) |
| Studio playback, scene replacement, automatic schedules and fullscreen | [Visual scenes](VISUAL-SCENES.md), [Studio guide](../visual-studio/README.md) |
| Popup help and its saved switch | [Popup hints](POPUP-HINTS.md) |
| Concise messages, per-track retry results and failures in Source corrections | [Messages and retry review](UI-MESSAGES-AND-RETRY-REVIEW.md) |
| Bar jumps, linked movement and long seeks | [Bar navigation](BAR-NAVIGATION.md), [long navigation](LONG-NAVIGATION.md) |
| Transition modes, locks and rehearsal | [Presets](TRANSITION-PRESETS.md), [keyboard](TRANSITION-KEYBOARD.md) |
| Editable preset shapes, swaps and filter sweeps | [Preset nodes](PRESET-NODES.md), [swap controls](SWAP-HANDLES.md), [filters](TRANSITION-FILTERS.md) |
| Controller desk, automation and FLkey | [Live desk](LIVE-DESK.md) |
| Arrange / Record-Perform, editable takes, private headphones and live master WAV | [Current implementation contract](ARRANGE-RECORD-PERFORM-PLAN.md), [earlier planning context](PERFORMANCE-MODE-PLAN.md) |
| Optional separation, timeline stems, authored nodes and external asset limits | [Stem workflow](STEM-SEPARATION.md) |
| Mastering, export and plugin latency | [Mastering](MASTERING.md), [WAV export](EXPORTING.md), [latency](PLUGIN-LATENCY.md) |
| Recovery, long sources and verified capacity | [Recovery and playback diagnostics](RECOVERY-AND-MONITORING.md), [capacity](SOURCE-CAPACITY.md) |
| Approved visual language | [Design](DESIGN.md) |
| Folder ownership and future cleanup | [Project maintenance](PROJECT-MAINTENANCE.md) |
| Choosing focused/full checks and retaining test data | [Testing policy](TESTING.md) |
| Codex control, isolated sessions and live integration tests | [Development bridge](DEVELOPMENT-BRIDGE.md) |
| Outstanding work and documentation reconciliation | [Roadmap](../FINISHING-PLAN.md), [documentation audit](DOCUMENTATION-AUDIT-20261104.md) |
| Future visualiser video integration and test files | [Video integration](VIDEO-INTEGRATION.md) |

## Research and history

- [MixMeister and DJ.Studio consensus grid investigation plan](research/CONSENSUS-GRID-BASELINE-PLAN.md): planned seven-track reference qualification, precise measurement and subsequent Alloy diagnosis. No tracks are certified by the plan itself.

`research/` retains dated reference observations and measurements. A historical statement that something is implemented, missing or pending applies to that checkpoint; it is not the current release status or an instruction to repeat a destructive cleanup.

- [Auto Arrange capability review](research/AUTO-ARRANGER-CAPABILITY-REVIEW-20261112.md) and [mixing audit](research/AUTO-ARRANGER-MIXING-AUDIT-20261112.md) record the 12 November research behind the shared pair planner.
- [Engine reconstruction](research/ENGINE-RECONSTRUCTION.md) and [recovered-rhythm assessment](research/RECOVERED-RHYTHM-ASSESSMENT.md): reference reconstruction and opt-in experiments; automatic-control admission is excluded from the current build.
- [Detection v0.31](research/DETECTION-AND-ALIGNMENT.md), [v0.32](research/DETECTION-V032.md), [real-music comparison](research/DJSTUDIO-REAL-MUSIC-AUDIT.md) and [rhythm reliability](research/RHYTHM-RELIABILITY.md): historical detector evidence and limitations.
- [Better Off Alone](research/BETTER-OFF-ALONE-DOWNBEAT.md), [first downbeats](research/FIRST-DOWNBEAT-VALIDATION.md) and [real tracks](research/REAL-TRACK-VALIDATION.md): earlier bounded evaluations.
- [DJ.Studio transitions](research/DJSTUDIO-TRANSITION-AUDIT.md) and [MixMeister gaps](research/MIXMEISTER-GAPS.md): dated competitor/reference scope.
- [Library recovery](research/AUDIO-LIBRARY-RECOVERY.md), [timeline alignment](research/TIMELINE-ALIGNMENT.md) and [UI playtest](research/UI-PLAYTEST.md): completed historical investigations, with recorded verification limits.


Raw verification reports, receipts, source snapshots and reference evidence remain under `.local/` at their recorded paths. Original document bytes are also preserved in the cleanup's verified before-source archive.

[Stem separation status and proposed backend direction](research/STEM-DIRECTION-20261023.md),23 October2026: dated research, not the current timeline-stem implementation status.

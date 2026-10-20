# Mixworks documentation

Current accepted application: **0.55.0 / analysis revision 9 / session format 25**. Start with [the app guide](../README.md), [current handover](../HANDOVER.md), [staged implementation plan and acceptance gates](../FINISHING-PLAN.md) or [verification and recovery](../VERIFICATION.md).

## Current guides

| Task | Guide |
| --- | --- |
| Library, analysis retention and cached loading | [Loading and analysis](LOADING-AND-ANALYSIS.md) |
| Measured import and warm/cold performance | [Import timings](IMPORT-ANALYSIS-TIMINGS.md), [0.48 performance](LOADING-PERFORMANCE-048.md) |
| Detection findings and independent accuracy limits | [Rhythm timing review](RHYTHM-TIMING-REVIEW.md) |
| Timeline editing, slipping and audio cutoffs | [Timeline editing](TIMELINE-EDITING.md) |
| Fixed ruler, overview, four lanes and resizing | [Timeline layout](TIMELINE-LAYOUT-049.md) |
| Bar jumps, linked movement and long seeks | [Bar navigation](BAR-NAVIGATION.md), [long navigation](LONG-NAVIGATION.md) |
| Transition modes, locks and rehearsal | [Presets](TRANSITION-PRESETS.md), [keyboard](TRANSITION-KEYBOARD.md) |
| Paired swaps and filter sweeps | [Swap handles](SWAP-HANDLES.md), [filters](TRANSITION-FILTERS.md) |
| Controller desk, automation and FLkey | [Live desk](LIVE-DESK.md) |
| Mastering, export and plugin latency | [Mastering](MASTERING.md), [WAV export](EXPORTING.md), [latency](PLUGIN-LATENCY.md) |
| Recovery, long sources and verified capacity | [Recovery](RECOVERY-AND-MONITORING.md), [capacity](SOURCE-CAPACITY.md) |
| Approved visual language | [Design](DESIGN.md) |
| Folder ownership and future cleanup | [Project maintenance](PROJECT-MAINTENANCE.md) |
| Future requests | [Wishlist](../WISHLIST.md) |

## Research and history

- [MixMeister and DJ.Studio consensus grid investigation plan](research/CONSENSUS-GRID-BASELINE-PLAN.md): planned seven-track reference qualification, precise measurement and subsequent Alloy diagnosis. No tracks are certified by the plan itself.

`research/` retains dated reference observations and measurements. A historical statement that something is implemented, missing or pending applies to that checkpoint; it is not the current release status or an instruction to repeat a destructive cleanup.

- [Engine reconstruction](research/ENGINE-RECONSTRUCTION.md) and [recovered-rhythm assessment](research/RECOVERED-RHYTHM-ASSESSMENT.md): reference reconstruction and opt-in experiments; automatic-control admission is excluded from the current build.
- [Detection v0.31](research/DETECTION-AND-ALIGNMENT.md), [v0.32](research/DETECTION-V032.md), [real-music comparison](research/DJSTUDIO-REAL-MUSIC-AUDIT.md) and [rhythm reliability](research/RHYTHM-RELIABILITY.md): historical detector evidence and limitations.
- [Better Off Alone](research/BETTER-OFF-ALONE-DOWNBEAT.md), [first downbeats](research/FIRST-DOWNBEAT-VALIDATION.md) and [real tracks](research/REAL-TRACK-VALIDATION.md): earlier bounded evaluations.
- [DJ.Studio transitions](research/DJSTUDIO-TRANSITION-AUDIT.md) and [MixMeister gaps](research/MIXMEISTER-GAPS.md): dated competitor/reference scope.
- [Library recovery](research/AUDIO-LIBRARY-RECOVERY.md), [timeline alignment](research/TIMELINE-ALIGNMENT.md) and [UI playtest](research/UI-PLAYTEST.md): completed historical investigations, with recorded verification limits.
- Full pre-cleanup [handover](history/HANDOVER-PRE-CLEANUP.md), [README](history/README-PRE-CLEANUP.md), [plan](history/FINISHING-PLAN-PRE-CLEANUP.md) and [verification notes](history/VERIFICATION-PRE-CLEANUP.md).

Raw verification reports, receipts, source snapshots and reference evidence remain under `.local/` at their recorded paths. Original document bytes are also preserved in the cleanup's verified before-source archive.

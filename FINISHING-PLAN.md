# Mixworks outstanding roadmap

Updated 10 November 2026. This is the single work queue for fixes, feature requests,
upgrades and remaining acceptance. Completed implementation tasks and superseded
instructions have been removed. [Handover](HANDOVER.md) owns the current candidate;
[documentation audit](docs/DOCUMENTATION-AUDIT-20261104.md) records reconciliation
and the old P01–P56 mappings. Historical plans retain detail, not scheduling authority.

**Order:** follow the phases below. A reproduced continuity failure takes
priority. These are complete outcomes, not permission to implement every item
at once. A reported problem stays open until its actual workflow is demonstrated;
earlier focused passes do not overrule Jay's observations.

**Current checkpoint.** Alloy **0.55.90 / Studio 0.1.18** is selected by
`out/Launch.cmd`. The native export-test stack failure is fixed and the complete
save/export57, destination111 and stale export-target75 journeys pass. Mix-bar
jumps release the render lock before preparation and UI notification; callback
PCM/clock and actual forward/backward jumps with moving Studio frames passed.
[Handover](HANDOVER.md) owns exact identities, all 441 focused native checks,
the .89 fixture-failure provenance, retention and scope. The earlier analytical
fades, prepared seeks, Studio lookahead and Beat grids repair retain their scope.

**Phase 1 remains in acceptance.** The bounded 12.19-second actual-device passage
added no lock misses, source waits or callback overruns. It does not close Jay's
intermittent listening report or sustained delivery. Next address the remaining
cursor-dependent structural-edit transaction lock scopes (including track moves),
then wider W04/acute W14 refusal/recovery journeys, before phase 2. The old native
export-test blocker is closed; both newly exposed bar fixture errors are corrected
without weakening their refusal/atomicity assertions. Large-catalogue startup/
reopen remains phase 3 work. Evidence: `.local/phase1-export-final-1110`.

## Delivery rules for every phase

- Protect continuous audio and visual delivery. Treat pauses, glitches, clock
  movement, render stalls and resource bottlenecks as defects to prevent and
  investigate, with audio continuity taking priority. Preload/prepare work away
  from playback-critical paths and verify actual output during interaction.
- Complete each phase as one coherent delivery: batch related changes, exercise
  the whole affected workflow, preserve Undo/save/reopen and verify actual UI,
  audio or moving pixels as appropriate. Avoid repeated partial build/review passes.
- Use the standing single-worker, below-normal-priority, serial focused-check
  policy. A broad stress/performance gate is reserved for an agreed qualification
  checkpoint. Keep the running production session and retained data untouched.
- These phases order the work; they do not authorise implementing the entire
  backlog automatically. Conditional ideas retain their deferred/feasibility status.

## Ordered phases - the execution queue

| Phase | Complete outcome | Work included |
| --- | --- | --- |
| **1. Continuity and clean automatic fades** | Interaction and automatic transitions preserve uninterrupted playback; each generated fade has two endpoints and an explicitly shaped curve. | W01, W04, W33; acute preparation/recovery defects from W14. Diagnose pauses/glitches during zoom, seek, node edits, scene preparation and live interaction. Replace sampled multi-node fade construction with direct curve evaluation; preserve authored automation and verify saved/reopened and rendered results. |
| **2. Finish visual output and live control** | A clear output owner, reliable scene changes and a detachable native-resolution fullscreen preview. | W06, W15, W34, W35. Detach/attach toggle; use the second monitor when present, otherwise the available monitor. Restore the embedded preview on attach. Render at the target display's highest/native resolution without putting preparation or frame delivery on the audio-critical path. Add separate random-scene and random-MilkDrop actions only while Studio owns live generation; disable them while active Mixworks timeline elements drive the output, and never rewrite those clips. Check active/future replacement, Undo, show coverage, joins, output lifecycle and artistic results. |
| **3. Trustworthy analysis, library and assisted arrangement** | Reliable musical timing and retained library state support understandable automatic mixes. | W11, remaining W14 and W19. Constant-tempo 4/4 BPM/pulse/downbeat/key, confidence and source/project mapping; catalogue-scale startup/preparation/recovery acceptance; phrase/section/vocal annotations, clash warnings, cue/treatment variety, key audition, exact-duration sets and energy-curve arrangement. Preserve protected references and valid analysis. |
| **4. Complete editing, audio processing and stems** | The core edit/mix/record journey is direct, reversible and audibly correct. | W03, W13, W17, W18, W21, with W04 applied throughout. Contextual selection/editing; protected ranges, node groups and tempo retiming; reusable snippets, sample child lanes, live loops and processor-backed effect regions; plugin delay compensation/isolation, routing, mastering, recording and tails; advanced stem processing/extraction/drum replacement. Validate actual audio, gestures and persistence for each coherent workflow. |
| **5. Controllers, interface and guided learning** | Physical controls, on-screen controls and guidance agree, with readable configurable presentation. | W07, W08, W09, W10, W12. FLkey pickup/feedback/reconnect/profile lifecycle and performance workflows; first-run tutorials and task help; dark chrome, themes/skins/layouts and waveform options. Use Jay's attached FLkey 49 panel and tutorial screenshot as physical-layout and learning-navigation references for Mixworks, not as instructions to change sound routing to FL Studio. Final comprehensive manual writing stays in phase 9. |
| **6. Video, export and exchange** | Media and finished mixes move in and out with verified timing and files. | W16, W22, W23. Timed video/media, looping/seeking/pause/sync and audiovisual export; MP3/FLAC and label-split output; native MixMeister/DJ-library exchange, FL Studio send/return and Ableton exchange/live sync. Qualify each end-to-end path and destination safety. |
| **7. Music creation** | A complete usable creation workflow, rather than disconnected controls. | W24. Sampler/sequencer, fills/loops, editable note recording, MIDI export, built-in synth/effect creation and key-aware note/chord tools. Integrate with the established controller, audio and save/export paths. |
| **8. Deferred and conditional extensions** | Only clearly scoped, useful extensions move into implementation. | W20, W25, W26, W27 and the deferred external preset scan from W15. Crates/categories; opt-in personalised mix versions; variable-tempo sources/other meters after phase 3's constant-tempo 4/4 acceptance; API/music services, reactive dancer, touch controller and dedicated hardware after feasibility/rights/value decisions. External catalogue scanning needs the previously agreed window; do not resume it automatically. |
| **9. Whole-product acceptance, release and final documentation** | The accepted product is demonstrably usable, supportable and cleared for the intended distribution. | W28, W29, W30. Full newcomer/accessibility/keyboard and real hardware/listening journeys; sustained audio/visual performance and resource limits on agreed workloads; installation/update/uninstall and explicitly requested final regression gate; licensing/provenance/owner prerequisites; final quick start, full manual and matching screenshots last. Earlier phases still require their own relevant listening/physical/visual acceptance. |

## New requests - 10 November 2026

| ID | Outstanding outcome | Closure evidence |
| --- | --- | --- |
| W33 - phase 1 implementation verified locally | **Two-node automatic fade effects.** Stop generating hundreds of unnecessary nodes or dividing one fade into many pieces. Use exactly two endpoints per generated fade and store/evaluate its chosen curvature directly. | Inspect generated curves; compare actual audio/control values through the fade, after edits, Undo, tempo mapping, save/reopen and export. No sampled intermediate nodes may reappear. Independent fades/holds and manually authored automation remain intentional. |
| W34 - phase 2 | **Detach/attach preview to fullscreen output.** Add the toggle to the visual preview. On detach use fullscreen on the second monitor if available, at the highest/native display resolution; provide the single-display fallback and restore the embedded preview on attach. | Actual one/two-monitor and DPI checks, full-resolution moving pixels, attach/detach persistence and uninterrupted audio/render delivery. No duplicate visual control surface or competing clock. |
| W35 - phase 2 | **Random live scenes and MilkDrop presets.** Separate controls swap the currently live-generated scene/preset, with a broad shuffled selection. | Live mode swaps visibly and reliably. While active Mixworks timeline elements own output, controls are unavailable and authored clips/automation are unchanged. Verify handoff, rapid requests, preparation, output continuity and saved state. |

Reference attachments: `C:/Users/jayte/Desktop/FLKEYS/reference/panel-1.png` and
`C:/Users/jayte/Desktop/FLKEYS/reference/tutorial-preview.png`. Their pictured
labels/links are reference content, not additional instructions or evidence that
all depicted FL Studio features are requested for Mixworks.

## Prior checkpoint evidence

**Local milestones.** W02/W05, W31/W32, partial linked-range editing and seekable
Library title preview are in the
[core record](docs/history/FINISHING-COMPLETION-20261104-CORE.md).
Later checkpoints covered linked Library drops with tempo and crossfade, tempo-aware
track and IN/OUT movement, Auto Arrange admission and curve ownership, scene
variety and feedback, and expanded sprite treatments. Use the candidate-specific
receipts in [Handover](HANDOVER.md) for each.

**Resumed milestone — W03/W07 optional original demos.** On the selected build,
37 native checks and 14 isolated desktop checks passed. From Help, the guide
generated three demo tracks, kept its place on Skip, resumed, and advanced through
real Library placement, waveform highlighting, muted playback and save. Reopening
the mix reproduced the sampled rendered audio. See the
[demo record](docs/history/FINISHING-IN-PROGRESS-20261104-DEMO.md) for remaining
first-launch, chooser, export and newcomer acceptance.

**W01 bounded playback check.** The selected build passed all 76 live-transition
checks. Six callback blocks across zoom and scroll matched an independent
renderer and kept the sample clock and project state stable. Reopened transition
PCM matched. The lock-contention control produced silence as expected, and two
node-editor refreshes produced matching PCM. Jay's intermittent report stays open.

**Scheduling authority.** Use the ordered phases above. A reproduced continuity
failure takes priority. Keep the six older Library/arrangement failures under
W14/W28 until rechecked on current source; historical passes do not close them.

## Detailed acceptance: playback and understandable editing

| ID / status | Outstanding outcome | Evidence needed to close it |
| --- | --- | --- |
| W01 — intermittent report open | **Audio interruptions and apparent alignment movement during zoom/other controls.** The bounded synthetic callback and reopen checks passed. Actual concurrent interaction and device playback remain unqualified. Include earlier unexplained playback breakup. | Reproduce the report with an owned passage and compare output, callback diagnostics and source/project timing to distinguish drawing movement, clock changes and dropouts. Repeat after reopening. |
| W03 — implemented in part, usability open | **Direct selection and contextual editing.** Ordinary waveform drag highlights a range, title drag moves a track, and Ctrl-click toggles track selection. Right-click opens the available edits. Local Copy, trim and linked-range results are in the dated core record. The current demo confirms ordinary drag. Assess the discoverability of free/snap drag, trim, split, copy, delete and scope. Verify whether the existing **Read, Touch, Latch and Arm** guidance explains when and where controls write. Reassess Edit details and simplify redundant routes where keyboard access is preserved. | A newcomer completes a short edit from the waveform/context menu with clear selection, Undo and correct saved/audio results. Resolve observed conflicts with drag, node, loop and navigation gestures. |
| W04 — design and implement | **Explain unavailable actions.** Inventory refusals during playback, recording, export, preparation, locks, missing media/devices and stale dialogs. Give concise action-specific reasons and the next useful step; make disabled menu/button reasons discoverable and accessible. | Attempt representative invalid actions by menu, button, shortcut and MIDI. Each explains why without partial mutation, false success or repeated intrusive popups. Preserve useful status details and retry routes. |
| W06 — implemented, acceptance in progress | **Replace a selected visual scene directly.** The lane context route replaces a scene/preset in place and preserves timing, image and joins. An earlier live Original-to-Original swap kept playback and placement stable. | Replace future and active scenes across Original/MilkDrop routes; inspect actual output continuity, Undo and reopen. Keep programme audio/time stable and verify which scene-specific settings reset. |
| W07 — demo path verified locally, newcomer acceptance open | **First-run tutorial and helpers.** The skippable, resumable guide and saved helper switch exist. The optional three-track route was demonstrated from Help through save/reopen on the selected build. | Observe automatic first-launch opening, import a user's own track through the chooser, save and export through the normal dialogs, and observe a newcomer completing the mix. Check hint coverage. |
| W08 — dark chrome implemented, acceptance open | **Dark, coherent panel/window chrome.** App-owned panels use custom dark title bars. The Getting started window was inspected at normal DPI. Assess hiding chrome where usable while preserving moving, closing, redocking and keyboard access. | Inspect docked/detached panels and controllers at normal/high DPI, focus changes and window modes. Identify OS-owned or third-party frames separately. |

W02, W05, W31 and W32 requirements and evidence moved to the
[dated core completion record](docs/history/FINISHING-COMPLETION-20261104-CORE.md).

## Core correctness and remaining workflow acceptance

| ID / status | Outstanding outcome | Evidence needed to close it |
| --- | --- | --- |
| W09 — planned | **Themes and layout options.** Three light and three dark themes initially; then user-created/shareable skins covering colours, typography, icons, controls and layouts. Extend existing named layouts, panel collapse and console order with useful docking/layout choices. Further forge/fusion/attract refinement follows playback judgement. | Persist/restore preferences without musical changes; inspect waveform, automation, selection, focus, contrast and compact/wide/DPI layouts. Named workspaces already exist. |
| W10 — planned | **Waveform variants.** Horizontal overview/detail stacks, optional stationary-playhead scrolling, mirrored shape, single-colour, RGB/frequency and low/mid/high three-band choices. Preserve useful Energy/Detail/Peaks and real stem views. | Options are independent, readable across themes and retain accurate source/grid/automation alignment. Profile only at the later agreed performance checkpoint. No vertical waveform layouts were requested. |
| W11 — open validation and targeted improvements | **Trustworthy constant-tempo 4/4 BPM, pulse, downbeat and key.** Establish independent tuning/holdout recordings and musical thresholds. Check source/project mapping, snapping and waveform rendering separately. Calibrate confidence honestly and preserve protected reviews. The Library now subdues rows with a grid or key issue in place of the separate check columns. The hint and accessible text describe the issue, valid manual corrections resolve it, and a focused test confirmed preview remains available. BPM display rounds for readability and the source editor retains precise values. | Beginning/middle/end and quiet/syncopated/half-double-tempo cases, durable source save and audible alignment. Confirm subdued rows still support ordinary selection, placement and editing. Use current Simple A/B/C correction and qualify the separate draggable main-timeline alignment guide. Seven-track consensus comparison remains unqualified/paused. Validate on a subset before any coordinated full-library scan. |
| W12 — open | **Reliable controller workflow.** Diagnose original mapping/profile startup/outage reports; finish physical FLkey pickup, feedback, reconnect and mapping lifecycle, detected-controller invitation, guidance and next-track/fade/cue workflow. | Actual raw MIDI and active mapping agree, including save/reopen/remap/delete/reconnect; hear the intended output and explain unavailable actions. Historical mapping titles and port captures are insufficient. |
| W13 — partial | **Audio, plugins and performance recording.** Full plugin delay compensation and crash isolation; broader band routing/master VST chain; optional pre-export mastering presets, headroom/clipping and bypass comparison. Qualify private output, real audio/ASIO, take/replay/master WAV and long recording. | Real routing/listening and independent render comparisons; failure/retry and processor tails. Existing take capture, private preparation, master processing and WAV paths are foundations. Active reported-latency plugins remain guarded until compensation works. |
| W14 — open acceptance | **Recovery and preparation reliability.** The SQLite Library startup repair passed small owned migration, unchanged reopen, changed-file watch, stop/retry and scope checks. Prior recovery/relink fixes are complete locally. Genuine interruption/recovery, corrupt/missing assets, long sessions and catalogue-scale acceptance remain open. Preserve valid analysis and preparation data. | Measure startup and verify retained identities/corrections and new durable rows at representative scale using owned data. Recheck the five prior background hydration/admission failures in `.local/scan05/native/checks.txt` with a focused current fixture. Historical 572-import recovery stays deferred. |
| W15 — open acceptance / selective refinement | **Complete visual-show experience.** The 10 November checkpoint implements direct Studio access, independent layer schedules, shuffled family selection, blend control, beat-driven sprite motion and the full-height preview. W34/W35 remain new phase-2 work. Confirm automatic coverage through the final track in a representative 44-track mix, deliberate variety, understandable joins, full output lifecycle and artistic acceptance. Least-used scene selection, retained MilkDrop state through slow frames and sprite treatments in 33 Originals have scoped evidence. Review motif spacing where repetition is observed. Keep the external NestDrop scan deferred to phase 8 and an agreed window. | Beginning/middle/end schedule and actual moving pixels after save/reopen, representative scene joins, images/sprites and fullscreen/display handling. The 204-scene expansion, punch/coverage work, generative sprite evolution and transition sharpness repair are complete locally. Sustained performance and artistic acceptance remain open. |

## Extensions after the core workflow

| ID / status | Outstanding work |
| --- | --- |
| W16 — deferred implementation | **Video integration with the visualiser.** Timed video/media on the visual timeline, audio-reactive scene/video composition and transitions as appropriate. Resolve fit, looping, seek/pause/sync, clip sound versus programme sound, media failure and export scope before implementation. Jay's four MP4s in `C:\Users\jayte\Desktop\REA\Mixworks\visuals\original\Video` are preserved test inputs; see [fixture inventory](docs/VIDEO-INTEGRATION.md). Product audiovisual export is a separate capability and needs its own verified timing/audio/encoding output. |
| W17 — partial | **Advanced editing:** protected ranges, automation-node groups and changing-tempo group retiming. Partial linked Cut/Crop/Replace implementation and evidence moved to the dated core completion record; broader source/plugin/gesture acceptance remains. Broader source-section occurrences and meaningful annotations remain W19. Existing range/group/Repeat/Crop/Replace are delivered; check gesture acceptance under W03 rather than rebuilding them. |
| W18 — planned | **Reusable creation/editing:** portable snippets, sample child lanes, live loop capture/release, movable effect regions with real processors and measured tails; broader noise/echo/reverb/gater/brake/scratch/sidechain/pitch effects and explicit pair tempo policies. No placeholder metadata or paint-only effect implementation. |
| W19 — partial | **Musical metadata and assisted arrangement:** richer phrase/section/vocal annotations, clash warnings, section-aware cue/treatment search, technique/genre variety, key shifting/key-sync audition and complete musical listening acceptance. Exact-duration sets and drawn energy curves remain unimplemented. The [musical-map idea](docs/ideas/MUSICAL-MAP-ARRANGER.md) is an optional design input, not adopted model/dependency scope. |
| W20 — explicitly deferred | **Custom libraries/crates and categories.** Plan later, implement later as requested 2 November. Resolve music versus visual collections, manual membership versus saved rules and simple building tools without duplicating media or disrupting the main library. Rekordbox is an interaction reference. |
| W21 — partial | **Advanced stems:** independent stem EQ/VST/effects and controller modes, per-stem transition treatments, interval extraction with linked counterpart mute, extraction editing and drum replacement. Local separation/aligned timeline stems and lane-only Solo exist. Remaining listening quality, asset lifecycle and routing need demonstrated output. |
| W22 — planned extensions | **Export:** MP3/FLAC choice and label-split output, then publishing/CD if still useful. Existing mix/take/live WAV and playlist/track-list outputs remain. Validate actual files and destination safety for each new format. |
| W23 — planned | **Exchange:** native MixMeister `.mxm`/DJ-library import/export, FL Studio send/return and Ableton exchange/live sync. Existing playlists do not establish arrangement compatibility. No general legacy mix compatibility project is requested. |
| W24 — planned | **Music creation:** fills/loops, sampler/sequencer, editable note recording, MIDI export, built-in synth/effect creator and key-aware note/chord tools. Instrument input/control automation and live WAV are distinct existing capabilities. |
| W25 — planned, opt-in | **Personalised versions:** explicit consent, preference learning and related mix alternatives; evaluate useful audible results rather than learning counters. |
| W26 — feasibility / deferred | **Other integrations:** AI/music platform APIs, audio-reactive dancer, mobile/tablet touch controller and dedicated hardware. Resolve platform, latency, rights and user value before implementation. Video has its own W16 item. |
| W27 — deferred | **Variable-tempo source maps and other meters.** Constant-tempo 4/4 accuracy comes first. Existing project tempo ramps do not fulfil variable-tempo detection. |

## Qualification and final help

| ID / status | Outstanding outcome |
| --- | --- |
| W28 — in progress, unqualified | **Whole-app functional/usability audit.** Continue existing rules/control inventories, unfinished take/export/chooser and independent-oracle cases. Reproduce or resolve the earlier chronological-order fixture's volume-fade refusal from `.local/scan05/native/checks.txt` on current source. Walk library → review → arrange → edit/mix/visuals → save/export/reopen, including invalid states, accessibility/keyboard, newcomer trial and real hardware/listening. Whole-product acceptance remains open. |
| W29 — planned, final manual last | **Help:** classify essential/useful/advanced content, write final plain-language quick start and full manual, then verify instructions/screenshots/newcomer journey against the accepted candidate. Refresh affected task help alongside workflow changes; the early tutorial is W07. |
| W30 — blocked on evidence/owner actions | **Release/performance/commercial qualification.** After interface acceptance and computer availability, agree workload/thresholds; qualify long playback/resources/frame pacing and clean installation/update/uninstall. Run one explicitly requested final gate. Resolve JUCE seller/entitlement, ASIO agreement, SoundTouch replacement/relink materials, commercial research boundary, visual/native/runtime allowlists, optional model/tonal-data rights, provenance, notices and owner/legal/brand/customer terms from [licensing](docs/LICENSING.md) and [commercial register](docs/commercial/README.md). No agent purchase/signature/public package. |

Use one build worker, below-normal priority, serial essential focused checks through
`verification.run_checks`, stress off. No numeric RAM guard, routine full gate,
benchmark, production restart, old scan resumption or data cleanup follows from this
roadmap. Preserve media, current sessions/caches, protected references, source
checkpoints and failed evidence. Close an item only with its stated observable
outcome; carry any genuinely remaining part forward explicitly.

# Mixworks outstanding roadmap

Updated 14 November 2026. This is the single work queue for fixes, feature requests,
upgrades and remaining acceptance. Completed implementation tasks and superseded
instructions have been removed. [Handover](HANDOVER.md) owns the current candidate;
[documentation audit](docs/DOCUMENTATION-AUDIT-20261104.md) records reconciliation
and the old P01–P56 mappings. Historical plans retain detail, not scheduling authority.

Follow the execution queue below. A reproduced continuity failure takes
priority. A reported problem stays open until its actual workflow is demonstrated.

**Current checkpoint.** The handover records the launcher identity. Phase 3's
constant-tempo 4/4 placement checkpoint passed 180 checks in the grid, rhythm
reliability and original waveform suites on 0.55.113. The detector baseline on
three reviewed recordings is retained for W11.
[Handover](HANDOVER.md)

**Phase 1 is complete.** Jay accepted playback in his own use on 11 November.
The two-point automatic fades have local verification. Jay deferred the track
movement report while he continues making mixes and looks for a repeatable case.

**Phase 2 has verified local checkpoints on 0.55.133 and 0.55.134.** The desktop workflow
covered active MilkDrop replacement, live random choices and fullscreen return
with playback continuing. Studio startup and **Swap GLSL** were verified on
0.55.134. A 44-placement fixture covered the entire show and
rendered sampled sections after save/reopen. Complete desktop GLSL replacement
next, then judge variety and joins on a representative mix.

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
| **1. Continuity and clean automatic fades — complete** | Jay accepted playback, and the two-point automatic fades passed local verification. | Focused continuity, refusal and recovery work landed. Track movement is deferred pending a repeatable case. |
| **2. Finish visual output and live control** | Alloy's four visual lanes open the full Studio. Studio follows the visual timeline or reacts to the mix's audio when the lanes are empty. | Finish desktop GLSL replacement (W06), then review a representative show (W15). Live random controls passed the local walkthrough (W35). |
| **Manual timeline checkpoint — locally verified** | DJs can see and edit a manual transition at every zoom and choose the number of audio lanes they need. | .110 verifies clock recovery (W04), separate lock and handle targets (W38), manual transition areas (W36), the lane selector (W37) and queued hotcue commit (W05). |
| **3. Trustworthy analysis, library and assisted arrangement** | Reliable musical timing and retained library state support understandable automatic mixes. | Establish constant-tempo 4/4 accuracy and source/project mapping (W11). Verify catalogue-scale startup, retained analysis and recovery (W14) before expanding assisted arrangement and judging musical results (W19). |
| **4. Complete editing, audio processing and stems** | The core edit/mix/record journey is direct, reversible and audibly correct. | Start with direct editing, linked edits and remaining playback-sensitive edit paths (W03, W04, W17). Then cover reusable edits and effect regions (W18), plugin delay compensation, isolation, routing, mastering and recording (W13), and advanced stem processing (W21). |
| **5. Controllers, interface and guided learning** | Physical controls, on-screen controls and guidance agree, with readable configurable presentation. | W07, W08, W09, W10, W12. FLkey pickup/feedback/reconnect/profile lifecycle and performance workflows; first-run tutorials and task help; dark chrome, themes/skins/layouts and waveform options. Use Jay's attached FLkey 49 panel and tutorial screenshot as physical-layout and learning-navigation references for Mixworks, not as instructions to change sound routing to FL Studio. Final comprehensive manual writing stays in phase 9. |
| **6. Video, export and exchange** | Media and finished mixes move in and out with verified timing and files. | W16, W22, W23. Timed video/media, looping/seeking/pause/sync and audiovisual export; MP3/FLAC and label-split output; native MixMeister/DJ-library exchange, FL Studio send/return and Ableton exchange/live sync. Qualify each end-to-end path and destination safety. |
| **7. Music creation** | A complete usable creation workflow, rather than disconnected controls. | W24. Sampler/sequencer, fills/loops, editable note recording, MIDI export, built-in synth/effect creation and key-aware note/chord tools. Integrate with the established controller, audio and save/export paths. |
| **8. Deferred and conditional extensions** | Only clearly scoped, useful extensions move into implementation. | W20, W25, W26, W27 and the deferred external preset scan from W15. Crates/categories; opt-in personalised mix versions; variable-tempo sources/other meters after phase 3's constant-tempo 4/4 acceptance; API/music services, reactive dancer, touch controller and dedicated hardware after feasibility/rights/value decisions. External catalogue scanning needs the previously agreed window; do not resume it automatically. |
| **9. Whole-product acceptance, release and final documentation** | The accepted product is demonstrably usable, supportable and cleared for the intended distribution. | W28–W30, with remaining W34 display qualification. Full newcomer/accessibility/keyboard and real hardware/listening journeys; sustained audio/visual performance and resource limits on agreed workloads; installation/update/uninstall and explicitly requested final regression gate; licensing/provenance/owner prerequisites; final quick start, full manual and matching screenshots last. Earlier phases still require their own relevant listening/physical/visual acceptance. |

## Phase 2 acceptance

| ID / status | Outstanding outcome | Evidence needed to close it |
| --- | --- | --- |
| W06 — MilkDrop picker and GLSL swap verified | **Replace a selected visual scene directly.** On 0.55.133, active MilkDrop replacement via the context menu rendered during playback and survived source-backed save/reopen. On 0.55.134, Studio’s Swap GLSL changed the active scene, kept its clip timing and preserved the edit on reopen. Native checks cover active and future replacement, joins and Undo. | Complete selection of a named bundled scene or local shader in the direct GLSL picker. Desktop automation reached both controls and failed at selection. |
| W35 — locally verified | **Random live scenes and MilkDrop presets.** Both Live buttons rendered changing frames during playback on 0.55.133. The saved audio-only copy kept the same clips, links and empty visual lanes. Reopening the authored mix returned Studio to Timeline mode. | Local walkthrough and harness checks passed. |
| W15 — fullscreen verified, artistic acceptance open | **Complete visual-show experience.** The 44-placement fixture verified continuous GLSL coverage and sampled GPU renders. On 0.55.133, fullscreen output at 1920 × 1080 returned via F11 while playback and animation continued. | Judge visual variety, joins and motif spacing on a real mix. Sustained performance belongs to W30 and the external NestDrop scan stays deferred to Phase 8. |

## Manual timeline editing checkpoint

| ID / status | Verified outcome | Remaining acceptance |
| --- | --- | --- |
| W04 — focused checks passed | **IN marker recovery.** Matching local clocks permit the edit. For a genuine mismatch, **Sync playback clocks** repairs the stopped pair while keeping the join. The fixture passed Undo, Save and Open, then accepted the IN edit. | The exact sequence that created the original .96 mismatch is unknown. |
| W38 — focused checks passed | **Separate pair toggle and IN/OUT targets.** Native checks operated the toggle at minimum, intermediate and maximum zoom. The IN/OUT gesture checks and locked group checks passed. | Use the controls with a real mix at the reported zoom. |
| W36 — focused checks passed | **Manual transition area.** OUT marks its tail and the incoming clip receives the corresponding area. IN/OUT gestures, Undo and source-backed Open passed. | Inspect on a longer manual overlap in a real mix. |
| W37 — focused checks passed | **Two, three or four visible audio lanes.** The native selector restored all three views with the same placements and admitted placement only on a visible lane. | Exercise the selector during a real mix. |
| W05 — focused checks passed | **Whole-mix timeline hotcues.** The prepared cue kept the programme clock, view and document while queued, then moved the clock and view at commit. The all-lane callback PCM and seek checks passed. | Device playback and physical controllers remain under wider acceptance. |

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

**Phase 1 automatic fades (W33).** The two-endpoint shaped fades were verified in
.84. The .86 checkpoint also verified mixed and overlapping gain/EQ curves with
authored nodes and real boundaries, including save/reopen and export-rendered PCM.

**Phase 2 fullscreen output (W34).** The .102 checkpoint verified real dual-display
detach/return at native resolution and simulated single-display fallback. Physical
single-display and mixed-DPI checks remain under W30.

**W03/W07 optional original demos.** The earlier demo checkpoint passed 37 native
checks and 14 isolated desktop checks. From Help, the guide
generated three demo tracks, kept its place on Skip, resumed, and advanced through
real Library placement, waveform highlighting, muted playback and save. Reopening
the mix reproduced the sampled rendered audio. See the
[demo record](docs/history/FINISHING-IN-PROGRESS-20261104-DEMO.md) for remaining
first-launch, chooser, export and newcomer acceptance.

**Earlier W01 bounded playback check.** That checkpoint passed all 76 live-transition
checks. Six callback blocks across zoom and scroll matched an independent
renderer and kept the sample clock and project state stable. Reopened transition
PCM matched. The lock-contention control produced silence as expected, and two
node-editor refreshes produced matching PCM. Jay subsequently accepted playback
in his own use on 11 November.

## Detailed acceptance: playback and understandable editing

| ID / status | Outstanding outcome | Evidence needed to close it |
| --- | --- | --- |
| W01 — playback accepted, track movement deferred | **Playback interruptions and apparent track movement.** Jay reports excellent playback and signed off on 11 November. He will report clearer details if track movement recurs while making mixes. | If it recurs, capture the gesture, zoom, Snap setting, status message and observed movement. Use those details to distinguish display movement, timing changes and a refused edit. |
| W03 — implemented in part, usability open | **Direct selection and contextual editing.** Ordinary waveform drag highlights a range, title drag moves a track, and Ctrl-click toggles track selection. Right-click opens the available edits. Local Copy, trim and linked-range results are in the dated core record. Assess the discoverability of free/snap drag, trim, split, copy, delete and scope. Verify whether the existing **Read, Touch, Latch and Arm** guidance explains when and where controls write. Reassess Edit details and simplify redundant routes where keyboard access is preserved. | A newcomer completes a short edit from the waveform/context menu with clear selection, Undo and correct saved/audio results. Resolve observed conflicts with drag, node, loop and navigation gestures. Verify the physical Ripple Delete shortcut alongside the already demonstrated Edit menu route. |
| W04 — focused fixes delivered, wider coverage open | **Safe editing and clear refusals.** Recheck the transition inspector, reverse and loop edit paths for playback continuity and atomic updates. Cover refusals during playback, recording, export, preparation, locks, missing media/devices and stale dialogs. Give action-specific reasons and a useful next step. Make disabled-control reasons discoverable and accessible. | Exercise valid edits during playback where supported. Use representative invalid actions by menu, button, shortcut and MIDI and verify consistent feedback, intact state and a working retry route. |
| W07 — demo path verified locally, newcomer acceptance open | **First-run tutorial and helpers.** The skippable, resumable guide and saved helper switch exist. The optional three-track route was demonstrated from Help through save/reopen at the demo checkpoint. | Observe automatic first-launch opening, import a user's own track through the chooser, save and export through the normal dialogs, and observe a newcomer completing the mix. Check hint coverage. |
| W08 — dark chrome implemented, acceptance open | **Dark, coherent panel/window chrome.** App-owned panels use custom dark title bars. The Getting started window was inspected at normal DPI. Assess hiding chrome where usable while preserving moving, closing, redocking and keyboard access. | Inspect docked/detached panels and controllers at normal/high DPI, focus changes and window modes. Identify OS-owned or third-party frames separately. |

W02, W05, W31 and W32 implementation evidence is in the
[dated core completion record](docs/history/FINISHING-COMPLETION-20261104-CORE.md).

## Core correctness and remaining workflow acceptance

| ID / status | Outstanding outcome | Evidence needed to close it |
| --- | --- | --- |
| W09 — planned | **Themes and layout options.** Three light and three dark themes initially; then user-created/shareable skins covering colours, typography, icons, controls and layouts. Extend existing named layouts, panel collapse and console order with useful docking/layout choices. Further forge/fusion/attract refinement follows playback judgement. | Persist/restore preferences without musical changes; inspect waveform, automation, selection, focus, contrast and compact/wide/DPI layouts. Named workspaces already exist. |
| W10 — planned | **Waveform variants.** Horizontal overview/detail stacks, optional stationary-playhead scrolling, mirrored shape, single-colour, RGB/frequency and low/mid/high three-band choices. Preserve useful Energy/Detail/Peaks and real stem views. | Options are independent, readable across themes and retain accurate source/grid/automation alignment. Profile only at the later agreed performance checkpoint. No vertical waveform layouts were requested. |
| W11 — open validation and targeted improvements | **Trustworthy constant-tempo 4/4 BPM, pulse, downbeat and key.** Establish independent tuning/holdout recordings and musical thresholds. Check source/project mapping, snapping and waveform rendering separately. Calibrate confidence honestly and preserve protected reviews. The Library now subdues rows with a grid or key issue in place of the separate check columns. The hint and accessible text describe the issue, valid manual corrections resolve it, and a focused test confirmed preview remains available. BPM display rounds for readability and the source editor retains precise values. | Beginning/middle/end and quiet/syncopated/half-double-tempo cases, durable source save and audible alignment. Confirm subdued rows still support ordinary selection, placement and editing. Use current Simple A/B/C correction and qualify the separate draggable main-timeline alignment guide. Seven-track consensus comparison remains unqualified/paused. Validate on a subset before any coordinated full-library scan. |
| W12 — simulated Studio routing verified, hardware acceptance open | **Reliable controller workflow.** Alloy 0.55.104 and Studio 0.1.23 verified simulated FLkey pot, wheel and pad routing. Diagnose original mapping/profile startup/outage reports and finish physical FLkey pickup, feedback, reconnect, mapping lifecycle, detected-controller invitation, guidance and next-track/fade/cue workflow. | Verify actual raw MIDI and active mapping agree across save/reopen/remap/delete/reconnect. Hear the intended output and explain unavailable actions. |
| W13 — partial | **Audio, plugins and performance recording.** Full plugin delay compensation and crash isolation; broader band routing/master VST chain; optional pre-export mastering presets, headroom/clipping and bypass comparison. Qualify private output, real audio/ASIO, take/replay/master WAV and long recording. | Real routing/listening and independent render comparisons; failure/retry and processor tails. Existing take capture, private preparation, master processing and WAV paths are foundations. Active reported-latency plugins remain guarded until compensation works. |
| W14 — open acceptance | **Recovery and preparation reliability.** The SQLite Library startup repair passed small owned migration, unchanged reopen, changed-file watch, stop/retry and scope checks. Prior recovery/relink fixes are complete locally. The .88 Beat grids menu check at 5,527 entries demonstrated menu response. Catalogue-scale startup and mix reopen, interrupted work, corrupt/missing assets and long sessions remain open. Preserve valid analysis and preparation data. | Measure startup and reopening, and verify retained identities/corrections and new durable rows at representative scale using owned data. Recheck the five prior background hydration/admission failures in `.local/scan05/native/checks.txt` with a focused current fixture. Historical 572-import recovery stays deferred. |

## Extensions after the core workflow

| ID / status | Outstanding work |
| --- | --- |
| W16 — deferred implementation | **Video integration with the visualiser.** Timed video/media on the visual timeline, audio-reactive scene/video composition and transitions as appropriate. Resolve fit, looping, seek/pause/sync, clip sound versus programme sound, media failure and export scope before implementation. Jay's four MP4s in `C:\Users\jayte\Desktop\REA\Mixworks\visuals\original\Video` are preserved test inputs; see [fixture inventory](docs/VIDEO-INTEGRATION.md). Product audiovisual export is a separate capability and needs its own verified timing/audio/encoding output. |
| W17 — partial | **Advanced editing:** protected ranges, automation-node groups and changing-tempo group retiming. Partial linked Cut/Crop/Replace implementation and evidence moved to the dated core completion record; broader source/plugin/gesture acceptance remains. Broader source-section occurrences and meaningful annotations remain W19. Existing range/group/Repeat/Crop/Replace are delivered; check gesture acceptance under W03 rather than rebuilding them. |
| W18 — planned | **Reusable creation/editing:** portable snippets, sample child lanes, live loop capture/release, movable effect regions with real processors and measured tails; broader noise/echo/reverb/gater/brake/scratch/sidechain/pitch effects and explicit pair tempo policies. No placeholder metadata or paint-only effect implementation. |
| W19 — partial | **Musical metadata and assisted arrangement:** audition the shared section-guided planner on short edits and long mixes, with up to 64 bars available for a suitable overlap. Evaluate a source-bound first/last-64-bar profile with beat confidence, vocal and melodic occupancy, bass and percussion activity, local tonality and section changes. Retain reviewed cues, confidence and source seconds so the evidence can be mapped onto an edited clip. Add richer treatments where measurements and listening show a benefit. The .118 stems receipt rechecks the historical chronological-order fixture. [Musical-map idea](docs/ideas/MUSICAL-MAP-ARRANGER.md) · [Functional audit](docs/AUDIT-20261113.md) |
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
| W28 — in progress, whole-app acceptance open | **Whole-app functional/usability audit.** The 13 November audit traces the major workflows and rechecks save/export, recovery, grid correction, take export targeting and Studio audio on .118. Reproduce the source findings, then walk library → review → arrange → edit/mix/visuals → save/export/reopen, including invalid states, accessibility/keyboard, newcomer trial and real hardware/listening. [Functional audit](docs/AUDIT-20261113.md) |
| W29 — planned, final manual last | **Help:** classify essential/useful/advanced content, write final plain-language quick start and full manual, then verify instructions/screenshots/newcomer journey against the accepted candidate. Refresh affected task help alongside workflow changes; the early tutorial is W07. |
| W30 — final qualification and owner decisions open | **Release/performance/commercial qualification.** Agree workload and thresholds after interface acceptance. Qualify sustained audio/visual delivery, snapshot and elastic preparation costs, physical single-display and mixed-DPI fullscreen (W34), and clean installation/update/uninstall. Run one explicitly requested final gate. Resolve JUCE seller/entitlement, ASIO agreement, SoundTouch replacement/relink materials, commercial research boundary, visual/native/runtime allowlists, optional model/tonal-data rights, provenance, notices and owner/legal/brand/customer terms from [licensing](docs/LICENSING.md) and [commercial register](docs/commercial/README.md). |

Use one build worker, below-normal priority, serial essential focused checks through
`verification.run_checks`, stress off. No numeric RAM guard, routine full gate,
benchmark, production restart, old scan resumption or data cleanup follows from this
roadmap. Preserve media, current sessions/caches, protected references, source
checkpoints and failed evidence. Close an item only with its stated observable
outcome; carry any genuinely remaining part forward explicitly.

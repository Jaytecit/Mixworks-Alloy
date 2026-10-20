# Mixworks implementation plan

Reconciled **20 October 2026**, against accepted **0.55.1**, analysis revision **9**, session format **26**. This combines every existing wishlist area with all 15 items from the user's `To Do.txt`. [WISHLIST.md](WISHLIST.md) retains requests and historical priorities; this document owns the current order. [HANDOVER.md](HANDOVER.md) and [VERIFICATION.md](VERIFICATION.md) own release identity and evidence.

**Goal:** progress from small fixes and adjustments to complete editing, dependable mixing and larger creative additions, then finish with a full plain-language user manual.

The user authorised beginning this plan on **20 October 2026** and selected [Jaytecit/Mixworks-Alloy](https://github.com/Jaytecit/Mixworks-Alloy) as the project repository. The first increment is **P01**, compact key labels with matching search/sort and explanatory hints; completed and locally delivered in **0.55.1**, with 2244 checks passing across 22 suites. P02–P05 and the new P44 remain planned. During that work the user reported Auto Arrange stacking treatments over the default volume fade and requested energy direction, no-dip blends and hard cuts. Those directly requested P22 additions are included in this increment; they do not mark all of P22 complete. Subsequent items remain ordered below. Preserve existing analysis, corrections, arrangements, caches, library snapshots and recovery. Leave the running app open. Stopped reference investigations and the eight untouched reserved recordings remain protected.

The [Personal Beta Licence](LICENSE) permits free personal, non-commercial testing and reserves commercial/distribution rights in the original application. [Dependency clearance](docs/LICENSING.md) is a prerequisite for public binary distribution and is part of P39 packaging acceptance. No public binary release is authorised by a local regression receipt.

## How to use the order

Work down the stages, delivering one useful increment at a time. Effort labels are relative: **small** means a local behaviour/presentation adjustment, **medium** crosses a few existing owners, and **large** introduces a substantial workflow, representation or integration. These are not time estimates. A reproduced audio-breakup or data-loss defect takes priority over cosmetic work regardless of stage.

Extend existing source/grid, project/transition, MIDI-profile, workspace, prepared-audio and renderer owners. Do not build a second automation, controller or audio engine. Keep musical edits undoable, workspace preferences outside musical Undo, and playback/export/session interpretation consistent. At implementation time, confirm each substantial feature's producer/consumers and narrow its acceptance cases into a focused change.

## Existing work to retain

| Area | Current foundation | Remaining work |
| --- | --- | --- |
| Library and analysis | Remembered library/artwork, background scan, revision-9 analysis/corrections, cache reuse and measured loading improvements | Better cold preparation/prefetch, independent musical evaluation and richer library preparation |
| Timeline | **Four** real mixer lanes, even overall resizing, local lane sizing, docking, slip, split/duplicate/remove, finite loops, linked movement and bar jumps | Direct range/ripple/repeat editing and a coherent editing console |
| Transitions | Frames/locks, V/B/M/H automation, curve visibility/isolation, selected-node percentages, paired swaps, filters, presets and A/B/C rehearsal | Easier local selection/grabbing, more treatments and private headphone audition |
| Controllers | FLkey setup and 47-binding profile; persistent input/profile diagnostics and transactional reload; sampled physical controls confirmed working | Named-profile display, complete profile management, reconnection diagnosis, pickup/modes/feedback |
| Arrangement | Auto Arrange with chosen/random start, count/approximate duration, harmonic/BPM matching and Balanced/Smooth/Bold/custom blends capped at 32 bars | Listening acceptance, explainable ordering, protected edits, exact duration and target energy |
| Audio and presentation | Live VST instruments/effects, mastering/spectrum/monitor mute, peak display, forge/attract effects and shared Alloy menu/dialog styling | Breakup diagnosis, full plugin delay compensation/isolation and presentation refinements |
| Recovery/export | Explicit snapshots, cancellable WAV export/tail choice and historical 60/90-minute numerical checks | Continuous recovery, current physical/listening/dialog/package acceptance and broader output choices |

The current 0.55.1 gate has **2,244 passing checks across 22 suites**, with the documented musical/hardware acceptance limits. Existing features should be extended; a passing regression gate does not prove musical accuracy.

## Stage 1 — Small visible fixes and adjustments

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P01 | **Delivered 0.55.1.** Display compact keys such as `Gbm` and `Gb`, preserving underlying identity and unknowns. | Library/playlist display and sorting agree; hints clarify major/minor. | Small |
| P02 | Add orange borders to application-owned popup panels and menus, extending 0.55 styling. | Inspect menus, alerts, pickers and detached panels at compact/wide sizes; text/focus remain readable. Plugin contents retain their owner. | Small |
| P03 | Start playback on a ruler double-click at the clicked position. | Single-click seeking, dragging, snapping and busy/export guards remain coherent; one double-click gives one transport action. | Small |
| P44 | Add a visible **Follow playhead** on/off toggle. While enabled, playback and explicit seeks keep the current position within the visible timeline; enabling it brings an off-screen playhead into view. | Pan only as needed; retain zoom, selection, fixed ruler/overview and musical state. Default off, persist in workspace preferences outside musical Undo. Manual timeline panning turns follow off so inspection is possible; the toggle reflects that state. Check playback, pause/resume, forward/back seeks, changing tempo, long timelines, zoom, resize and docking. | Small–medium |
| P04 | Display the **active mapping title** on the live desk, with an explicit unnamed/default state. | Reflect loaded assignments after startup/reload/profile changes, rather than merely a saved filename. | Small |
| P05 | Combine the duplicate controller and Audio / MIDI toolbar entry into one button. Show controller state/name; use Audio / MIDI when absent. | Same settings destination, accurate available/open/receiving distinctions, clear long-name/multiple-device handling. | Small–medium |

**Finish when:** requested adjustments are visible and checked without changing musical data or controller assignments. The confidence column follows in Stage 2 because its semantics need separate attention.

## Stage 2 — Editing usability, controller fixes and playback diagnosis

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P06 | Put automation selection directly in the selected transition: visible volume/bass/mid/high/filter controls, clear active curve, easier node targets and unobstructed selected-node text. Reuse existing isolation. | Edit each overlapping curve without hunting through menus; check mouse/keyboard, crowded nodes, narrow views, locks and Undo. | Medium |
| P07 | Complete popup hints for all controls/features with a persistent on/off setting. | Inventory controls and disabled states; explain action/result plainly without covering the edited node. Preserve focus and reduced-motion preferences. | Medium |
| P08 | Audit/fix MIDI detection, mapping, remapping, naming, save/save-as, reload, import/export and deletion. Preserve working FLkey routing. | Physical reconnect, conflicting assignments, cancellation, malformed files, unwritable saves, active-profile deletion and reopening. Distinguish active assignments from disk contents. | Medium–large |
| P09 | Add the adjacent key-confidence column: **1 no key found, 2 low, 3 good, 4 excellent**, with colour plus text/number. | Retain source score/unknowns; define and validate thresholds before using strength labels. Distinguish manual overrides. No assumed 99% probability from spot checks. | Medium |
| P10 | Investigate playback breakup before intensifying visuals. Compare forge/attract off/on and reduced/full motion under matched driver/buffer, plugin, scan/cache and background-load conditions. | Measure callback timing/underruns and UI/CPU/GPU/load; correlate audible failures, fix the producing bottleneck if reproduced and repeat the failing scenario. Otherwise retain conditions and uncertainty. | Medium; fixes depend on cause |

The user's positive key results are useful feedback; raw detector margins are currently uncalibrated. Preserve the original “Bias toward 99% positive” wish, but do not manufacture a probability or silently change detection. P10 establishes whether UI drawing contributes; other processes remain a hypothesis.

**Finish when:** transitions are easy to target, profile lifecycle survives real use, confidence labels are defensible, and breakup has a verified fix or bounded diagnostic record. Escalate reproducible audio faults immediately.

## Stage 3 — Full track-segment editing and workspace changes

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P11 | Build a lower-section **Editing** tab/console, detachable like the other consoles. Move Clip editing there; keep VST instruments/effects at the far right and remove the old Clip/MIDI tabs from that location. Relocate keyboard and setup access so instrument use stays reachable. | Current Clip contains start, source-in, length, fades, gain, reverse and loop controls. Demonstrate these survive the move, including selection, docking, resizing and workspace restoration. | Medium |
| P12 | Extend editing to direct segment/range selection, trim/split, repeat, remove, insert/replace, move/copy, explicit ripple/gap-close and protected ranges. | Non-destructive sources; automation, effects, tempo maps, labels and transition locks survive admitted edits, Undo/Redo, save/reopen and export. Reject conflicting edits explicitly. | Large |
| P13 | Add named reusable song treatments, linked overlays/snippets and sample launching, then live loop capture/release. | Supported treatments transfer correctly; overlays retain parent/source identity; quantised launch/loop/release agree in playback and saved projects. | Large |
| P14 | Extend resizable/detachable panels into rearrangeable, named workspace layouts. | Essential controls stay reachable at supported sizes; failed/unknown layout restoration has a usable fallback. Reuse workspace ownership/backups. | Medium–large |

**Finish when:** users can edit a song segment and transition without losing existing Clip or MIDI keyboard capabilities. The console is a complete editing workflow, not just a renamed tab.

## Stage 4 — Larger audio, monitoring and session foundations

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P15 | Add independent library/transition headphone preview. Existing private-cue drafts remain unverified starting material. | Explicit private output; cue never reaches master/export, handles supported device changes and fails safely without private output. Audition while the main mix continues. | Large |
| P16 | Complete plugin delay compensation and failure/isolation handling, then general master VST processing and frequency-targeted effects. Extend current mastering/spectrum/live desk. | Measured delayed-impulse alignment, bypass/removal, tails, seeking, chain changes and playback/export parity with real plugins/devices. Band routing and separated stems remain distinct. | Large |
| P17 | Add continuous recovery with bounded storage and clear restore choices; improve cold preparation/prefetch where measurements justify it. | Genuine interrupted-session recovery, incomplete/corrupt snapshots, missing files, disk pressure, cancellation and long-session memory/I/O preserve the active mix/source identities. | Large |
| P18 | Finish live next-track/fade/pause/cue-next controls and FLkey polish: safe pickup, clear modes, on-screen diagram and mapping invitation on detection. Add verified pad feedback afterward. | No parameter jumps/accidental remaps; actual device messages, reconnects and held/repeated presses. Broader notification is distinct from delivered status. | Medium–large |

**Finish when:** monitoring, plugin timing and recovery support dependable long sessions. Private routing, actual underruns and physical ASIO latency need direct evidence.

## Stage 5 — Musical preparation and library tools

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P19 | Evaluate constant-tempo **4/4** BPM, pulse, downbeat phase and key against independently reviewed references; improve demonstrated failures. | A reliable downbeat anywhere anchors bars backwards/forwards; kick pulse alone does not prove beat one. Separate ambiguity and source/placement/render failures. | Large |
| P20 | Extend tentative energy hints into editable phrase/section/vocal annotations and explanations. Add automatic suggestions only after independent evaluation. | Manual correction, unknowns, bars to changes and overlapping-lead-vocal warnings. RMS changes are not proven verse/chorus/vocal detections. | Large |
| P21 | Add library categories, metadata/tags, smart crates/rule playlists and audio semitone shifting/key-sync audition. | Preserve originals/source identity; distinguish audio transposition from metadata correction; keep key uncertainty visible. | Medium–large |

The [consensus investigation plan](docs/research/CONSENSUS-GRID-BASELINE-PLAN.md), seven-track qualification and guided MixMeister/Alloy walkthrough remain **paused**. If separately resumed, freeze measurement rules before screening and retain rejected/inconclusive cases. Reference-app agreement is not independent musical truth. Variable-tempo maps/multiple anchors and other meters follow under P27, not as a prerequisite for every UI improvement.

**Finish when:** suggestions expose evidence/limits, users can correct them, and dependable constant-tempo workflows support assisted arrangement. Only an actual verified detector update invokes the separately authorised owned-data reset after exact path/process validation; this plan and display-only changes do not.

## Stage 6 — Assisted arrangement and project exchange

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P22 | Extend Auto Arrange into library/selection ordering with reasons, “make these two tracks work” alternatives and richer transition generators. | Preview through A/B/C/preset owners; preserve chosen starts, locks and protected moments. Explain actual BPM/key/section evidence. Listen to generated mixes. | Large |
| P23 | Add exact-duration mixes and a user-drawn target energy curve, including usual 60–90-minute sets. | Only allowed cuts/loops/extensions, favourites/locks protected, edit preview and explicit unsatisfiable target. Existing approximate duration is not exact. | Large |
| P24 | Add local opt-in preference learning/personalised cues and related radio/extended/instrumental/performance mix versions. | Learn only explicit approvals; rationale/edit/reset/export. Versions retain shared edits/differences without overwriting originals. | Large |
| P25 | Import MixMeister `.mxm` arrangements and inspect the user's actual DJ-library cue/grid/tag exchange formats. | Representative permitted files, source identities/unsupported fields retained, omissions reported, conversion preview and placement/audio comparison. No assumed format parity. | Large |
| P26 | Send transitions, then whole mixes, to FL Studio as aligned track WAVs, tempo/position metadata and reference render; return edits as undoable alternatives. Then investigate editable Ableton exchange and FL live sync/plugin hosting. | Preserve ramps, placement, tails and prior treatments. Measure actual formats/transport/multi-output/latency. Native FL project creation is unnecessary for the first useful exchange. | Large |

**Finish when:** assisted sets are editable/explainable and exchanged edits return to the correct position without discarding prior work.

## Stage 7 — Advanced audio and creation tools

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P27 | Add source-owned variable-tempo beat maps/multiple anchors, followed by other meters. | One canonical map drives editing, placement, playback, export and persistence; evaluate drifting sources independently and retain constant-tempo support. | Large |
| P28 | Add aligned stem separation/recombination, then stem effects, controller stem modes and drum capture/replacement. | Preserve original audio; measure alignment/recombination/artifacts; handle partial jobs/cancellation and stem/source identities through edits. | Large |
| P29 | Add tempo-aligned fills/risers/FL-created loops first, then percussion sequencer/drum machine, keyboard sampler, quantised MIDI-note/audio recording and MIDI export. | Reuse transport/sample/recording owners; verify quantisation, latency, velocity/pitch, capture, Undo and reopening. VST input/control-automation recording already exist. | Large |
| P30 | Add a built-in synth/effects creator and key-aware note/chord suggestions. | Saved patches, automation, CPU/latency and playback/export agree; suggestions preserve key uncertainty. Build on P29 recording/sample ownership. | Large |

**Finish when:** creative tools produce reusable aligned material and stay stable in a full mix. Keep stem isolation, band processing and metadata correction distinct.

## Stage 8 — Visuals, personalisation and external integrations

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P31 | Intensify forge/attract lighting, smooth the waveform weld-heat leading edge and finish the timeline-scrollbar fusion effect. | Meet P10's measured playback budget with motion off/reduced/full, dense waveforms and long playback; keep intensity/motion controls and readable editing. Existing activation is delivered. | Medium |
| P32 | Add the small audio-reactive dancer panel, then integrate the existing Synesthesia reconstruction as an automatically populated, editable visualiser/video timeline lane. | Read Synesthesia continuity before integration; verify scene/audio sync, cue ownership, edits, disabled states, save/reopen and visual/video export without disturbing audio. | Large |
| P33 | Add full preset/user skins for colours, type, icons, controls, layouts, dimensions, visibility and toolbars; portable sharing/import, starter templates and creation guidance. | Validate packages, retain defaults/rollback and test readable/focus/motion states. Reuse P14 workspace ownership. | Large |
| P34 | Assess API-key AI mixing and Beatport/SoundCloud/other platform integrations individually, starting with useful authorised exchange. | Verify actual APIs, rights/offline/cache constraints, cost/cancellation; protect credentials, preview edits and preserve local operation. | Large; feasibility first |
| P35 | Assess dedicated Mixworks control hardware after mappings/workflows settle. | Prototype with existing hardware first; verify protocol, reconnect, pickup, feedback and drivers before custom hardware commitments. | Large; feasibility first |
| P36 | Add compressed output and division at labels, then optional publishing/CD production. | Verify encoder/tail/label boundaries and files; service actions/physical discs have separate acceptance. Depend on reliable local WAV export. | Medium–large |

P31 follows playback diagnosis despite being a visual change. External integrations retain feasibility gates rather than promises of unavailable capabilities.

## Stage 9 — Guided learning, gap audit and app acceptance

| ID | Work | Acceptance | Effort |
| --- | --- | --- | --- |
| P37 | Add first-run/revisitable tutor: highlight controls, add tracks, explain analysis/compatibility, place two compatible tracks and create a transition with automatic/manual choices. | Start/skip/pause/revisit safely; handle missing audio/devices/analysis and changed layouts. Use stable controls/hints from earlier stages. | Medium–large |
| P38 | Audit every function, control, workflow and background process for gaps/desirable additions. Maintain the capability inventory as stages land; finish the exhaustive audit here. | Classify implemented/partial/broken/absent/deferred with evidence. Recommend justified additions; return accepted gaps to earlier stages before finalising the manual. | Medium–large |
| P39 | Complete end-to-end acceptance and intended distributable package. | User completes a mix; current native import/drop/save/open/export/Save–Discard–Cancel, genuine recovery, 60/90-minute listening, physical FLkey/ASIO/underruns, missing audio/plugins, non-ASCII/unwritable paths and standalone runtime/model/licences. | Large |

Keep the manual's source inventory current during development; write/finalise the full manual after agreed scope/workflows pass acceptance. Optional infeasible integrations can remain explicitly deferred without blocking a manual for the accepted app.

## Stage 10 — Full user manual and quick start, last

- [ ] **P40: Classify the complete P38 inventory** into essential setup/safety/recovery knowledge, useful optional detail, advanced troubleshooting and internal engineering material outside the manual. Do not hide limits, destructive consequences, data handling or costs users need to understand.
- [ ] **P41: Write the plain-language quick start**: setup/audio/controller, add music, understand/check analysis, choose two compatible tracks, arrange/edit a transition, listen, save/reopen and export a first mix.
- [ ] **P42: Write the full manual** for the accepted version: library/preparation, grid/key correction, timeline/segment editing, transitions/automation, live desk/profiles, instruments/effects, monitoring, assisted mixes, creation, visuals/layouts/skins, supported exchange/integrations, export, recovery and troubleshooting. Include shortcuts, disabled states, background work and limits when they affect users; exclude deferred features from available-control instructions.
- [ ] **P43: Verify every instruction/screenshot** against the packaged app, align tutor/hint terminology, link detailed references and have a new user complete the quick start. Record version/update ownership.

**Finish when:** a newcomer can set up Mixworks and save, reopen and export a mix using the quick start, then find accurate help for every available feature. This is the final planned deliverable once the app is settled enough to document.

## Verification and continuity

For each later code increment: retain a before-source checkpoint and relevant data inventory; verify the changed workflow and relevant failure/Undo/persistence cases; build/freeze the candidate and run the receipt gate in [VERIFICATION.md](VERIFICATION.md). Keep musical/listening/hardware evidence separate from fixtures. Generic REA/npm checks do not replace this native prototype's gate.

For this planning update, check request coverage, document links, current release consistency and preservation of the original To Do file; no application tests are needed because runtime files do not change. Update the handover each session. [The historical plan](docs/history/FINISHING-PLAN-PRE-CLEANUP.md) and [historical handover](docs/history/HANDOVER-PRE-CLEANUP.md) retain earlier results; old unchecked tasks do not authorise resets or resumed experiments.

Method: focused increments, ownership and concrete acceptance, informed by the existing writing-plans skill and its [skills.sh listing](https://skills.sh/obra/superpowers/writing-plans). Repository preferences govern this roadmap: no subagents, speculative code or extra planning/review cycle is needed to consolidate requests.

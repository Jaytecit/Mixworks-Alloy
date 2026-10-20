> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# DJ.Studio transition editor: Alloy gaps

Investigated 18 October 2026 at the user's request, using installed DJ.Studio, REA, and the running Alloy release. This is an investigation checkpoint; no Alloy executable or source implementation was changed.

Implementation follow-up, 19 October: selected-pair length/presets, locks and independent Volume/Bass/Mid/High modes with strength, beat gaps and swap position are now implemented. See [the current Alloy guide](../TRANSITION-PRESETS.md). The findings/status tables below retain the original audit's historical state and reference limitations; they are not the current delivery inventory.

The main missing feature is a **selected-transition editor**. Alloy has the frame and several useful underlying operations, but its toolbar presets act on insertion or a whole-arrangement rebuild. DJ.Studio edits the selected pair through a panel of independently composable settings.

## What was examined

- Installed DJ.Studio shell 4.2.15, downloaded frontend `1823b9d5ce41ab66810c7263a2f75f017a48bbd5`. Opened `Seventy Seven` and inspected its transition panel. All experimental edits were made in a new local duplicate named **Alloy Transition Audit - test copy**. That copy is retained for follow-up; no transition edit was made to the original.
- Running Alloy `out/releases/0.32.0-35ce9a01919a/Mixworks.exe`, plus its transition model, gesture handlers, preset producer, automation interpolation and toolbar wiring. Its existing unsaved session was left in place without editing it.
- REA analyzed an exact byte-for-byte copy of the installed `simple-studio-B215MvWv.js`, SHA-256 `5a60a5db34cf16a3e3447460268982f091861ac62a1421a0434522304b7aaf23`. Additional hashed source observations cover preset construction and saving in the installed shared frontend.
- [Official Transition Editor guide](https://help.dj.studio/en/articles/8106059-transition-editor). Documentation describes behavior beyond the runtime checks below; those descriptions are not treated as observed audio results.

## Direct checks in DJ.Studio

| Check | Observed result |
| --- | --- |
| Select first transition | Focused the pair at about 5:21; the frame displays its number, key relationship, lock and zoom affordances. Opening Transition exposes pair-specific controls. |
| Lock, then unlock | Blue frame becomes green; panel displays `Transition is locked`, editing controls are obscured, tempo/length controls dim, and the command becomes Unlock. Unlock restores the blue frame and editor. |
| Attempt to drag locked header | The viewport moved, while the frame remained aligned to the same ruler time/source bar. This supports a movement guard for this gesture; Harmonize and tempo-lane protection were not exercised. |
| Change 16 bars to 8 | Frame width halved on the same time scale, left boundary remained around 5:21, right boundary moved earlier, outgoing audio beyond it dimmed, and volume/bass curves rescaled. |
| Bass Swap gap 0 to 1 | The displayed gap became 1 and the two yellow handoff edges separated. This is a deliberate gap between outgoing and incoming bass, distinct from Alloy's existing paired ramp. |
| Volume Crossfade 50% to 75% | The selected percentage and the visible volume curves changed; Bass remained Swap with gap 1. |
| Solo visibility for Bass | Bass S highlighted and the volume overlay dimmed. This isolates curve editing/visibility, separately from audio solo. |
| Next transition | Focus moved to pair 2 at about 9:18, Lonely VIP to Lonely Radio. That pair retained Volume Crossfade 50% and Bass Swap 0, rather than inheriting pair 1's edits. |
| Audition solo | The audition button cycles through numbered channels, including Solo 1/2 for decks and Solo 3 for Sample 1; the next click returned to both. Corresponding channels highlight and other waveforms dim. This is channel solo, not evidence of pair-specific or private-headphone routing. Playback was stopped; audio routing was not measured. |

Attempts to drag the bass handoff and a song header did **not** establish their intended movement: they produced seeking/range selection or viewport changes. Do not claim independent-song or handoff-drag parity from these attempts. The guide describes those operations, and the shipped code contains related handlers, but a reliable successful gesture remains to be captured.

## Controls present in the installed panel

| Area | Installed choices | Alloy status |
| --- | --- | --- |
| Volume, Bass, Mid, High | Independently choose None, Crossfade, Fade in, Fade out, Swap, Manual. Bass additionally has Swap fade. Active curves expose strength or swap-gap controls. | Three-band EQ and manual automation exist. Independent per-transition mode/strength/gap settings are absent. |
| Whole presets | Bass swap, Crossfade, High to low, Low to high, Echo out, Echo Scratch; Save and My presets. | Align IN/OUT, Bass swap, Crossfade only; no saved user transition presets. |
| Filter | None, High to low, Low to high, Manual. | No first-class filter automation target or built-in transition filter. VST parameter automation is an existing foundation, not this control. |
| Noise | None, Riser, Downsweep, Swoosh, Manual. | No equivalent transition generator. |
| Effect in | None, Loop in, Scratch in, CUE play start, Key swap, Manual key. | Finite clip looping exists; these transition-local effect modes do not. |
| Effect out | None, Loop out, Vinyl brake, Reverb, Echo. | VST hosting exists; these built-in transition modes do not. |
| Side-chain | None, Crossfade, Swap, Manual. | No equivalent transition-local side-chain control. |
| Timing | Selected transition 4/8/16/32 bars; BPM Off/Free/Half/Full; lock. | Toolbar length is a future insertion/rebuild setting. No selected-pair length action, tempo mode, or persistent lock. |
| Rehearsal | Previous/next, repeat transition/sample, numbered channel solo, metronome. | Lane solo and source-grid audition exist. No selected-transition repeat/navigation/audition workflow. |
| Stems | Category and header affordance are visible. | Separation and stem-specific transitions are absent. Separation was not activated or tested in DJ.Studio. |

Presence in the UI does not establish sound quality, effect DSP, extension availability or export behavior. Save-preset round trips, all strength/gap values, effect processing, stem separation, looping audio and manual curved-line gestures remain unverified here. The official guide describes strengths of 25/50/75/100 and additional swap-gap choices; only the changes above were exercised.

## Specific Alloy workflow defects and producing boundaries

1. **Changing the visible bars selector does not resize the selected frame.** `Source/Workstation.cpp` only updates `transitionIndex` in `transitionLength.onChange`. The menu does the same in `Source/WorkspaceUI.cpp`. A user familiar with DJ.Studio can change 8 bars and see no local edit. Add a selected-transition command that calls the geometry owner; keep the new-track default separately labelled.
2. **An existing pair has no local preset-apply command.** The preset tooltip advertises insertion/Beatmatch. Beatmatch invokes `syncMix(true)`; `prepareTempoMap(..., align=true)` in `Source/Beatmatch.cpp` clears links on a full rebuild, resets automatic bass, fades and anchors, and reconstructs placement. That is a whole-mix action. It cannot safely stand in for editing one transition while preserving surrounding manual decisions.
3. **Transition identity has no editable settings record or lock.** `Project::TransitionLink` in `Source/Model.h` owns outgoing/incoming IDs and optional bass endpoints only. `Timeline::mouseDown` selects the incoming clip and begins a geometry gesture. There is no transition inspector, persistent lock, per-band mode or local tempo policy. Add these at the transition owner and make UI, persistence and rebuild consumers use that owner.
4. **Curve vocabulary is smaller than the reference.** `AutomationPoint`/`AutomationCurve` and `AutomationCurve::interpolatedValue` provide linear point interpolation. There is no shape/tension or preset-strength representation. Reuse the source-clock and paired-point foundations; avoid manufacturing a second unrelated curve system inside a panel.
5. **The blue frame alone does not communicate the whole operation.** Alloy draws a header and OUTGOING/INCOMING handles, but provides no selected-pair zoom/navigation, key relationship or local sound controls. Its 5-pixel resize hit zones are confined to the 18-pixel header. This is a discoverability risk, not a newly proven geometry corruption bug.

Alloy Bass swap intentionally adds bass EQ without volume fades, as requested in the previous task. DJ.Studio's selected Bass swap visibly combines Volume Crossfade with Bass Swap. That is a semantic difference to expose through independent settings; do not silently add volume fades to Alloy's existing preset or change the alignment-only default.

The current frame range derives from OUT/fade geometry, not the toolbar default. In alignment-only mode it can therefore extend far beyond the displayed 8-bar default. This is another reason the new-track setting and selected-transition length need distinct roles.

## Implementation order informed by this audit

1. Add a stable selected-transition identity and inspector, local 4/8/16/32-bar actions, focus/previous/next, and a clear distinction between selected-pair edits and insertion defaults. Preserve existing frame geometry and source-clock ownership.
2. Add persistent locks enforced in geometry, song/marker moves, automation, tempo changes and arrangement rebuilds. Specify propagation at both neighbours of a locked pair; a green paint state alone is insufficient.
3. Compose Volume/Bass/Mid/High modes per pair, including swap gap, independent strength, paired handoff editing and curve visibility solo. Apply changes atomically with one Undo; verify saved/reopened state and unchanged other pairs.
4. Add transition rehearsal with lead-in/out and both/each-track audition, followed by saved user presets. Rehearsal on the main output does not satisfy independent headphone routing. A/B/C alternatives remain a separate requested enhancement.
5. Add filter and effect modes through real DSP and matching render tests. Noise, scratch, braking, side-chain and stems require explicit processing work, not only new menu labels.

For each batch, verify with real public UI gestures, source-clock/tempo-map changes, Undo/Redo, save/reopen and an audio render. Retain existing malformed-session, pending-load, manual-automation and neighbouring-transition coverage. No new detector work is implied by a transition-editor change.

## Evidence and limitations

Local evidence directory: `.local/djstudio-transition-audit-15/`.

- `rea-evidence-with-trace.json`: complete retained REA ledger, seven records. Parent application Evidence `ev_9ac93d047268e38cc3e754df978e3c93e8d4c13d9e4b9a2046fa7bbdcfb7cc33`; summary view `ev_31aba56223508a502b7d9fad8bd7c15a097bae48c071b29f9ffa1c8a497ff7ac`.
- `rea-lock-functions.json`: six exact source-range-linked lock/edit guards recovered in the REA semantic graph. Main transition `handleDown` is node `jsrg_node_10ee81e1f3a17cc43874eebe3de1de2567d6ebbd3fd063ef539879c8f0c50633`, source line 719, column 19217.
- Semantic backward trace `ev_d92ab2f169d1af2b7d6576a43b5df3daeaea95178d4e52fd3121c34dad62af29` matched that exact node: three nodes/two relations, partial coverage. Its oversized transport response was recovered through complete ledger export; analysis was not rerun. The earlier literal `locked` feature trace had no match; that does not establish absence.
- REA's 100,000-node semantic ceiling leaves partial semantics and 22,602 unknowns in the parent. Static handler guards support interpretation but do not establish runtime calls, complete state propagation or DSP parity.
- `source-observations.json`: original paths, SHA-256 identities, byte sizes and bounded character-offset observations from installed chunks. These are separate analyst observations, not extra REA-derived conclusions.
- `dj-*-ui.txt` and `alloy-initial-ui.txt`: saved accessibility observations. Screenshots were inspected during computer use. Initial Alloy screenshot capture was occluded by DJ.Studio; Alloy was then explicitly activated and visually inspected. Do not use the occluded capture to judge Alloy appearance.

No audio listening, rendered effect comparison, private headphone routing or comprehensive native gesture pass is claimed. The existing 1,012-check release gate was not rerun: no native code changed. The original DJ.Studio mix, Alloy unsaved arrangement, launchers and executables were not edited. A local DJ.Studio audit duplicate was created and edited, and remains available.


### Overnight recovery reference scope

REA CLI `trace-application-feature` on the authenticated preserved transition frontend returned no string-seed matches for `recover`, evidence `ev_5c4fb6d69627b6f98a9531807dbda9688ed7a268128bf0b65c7c6b802fea8587`. This result applies to that module graph only; the Electron main process and other frontend modules were not covered. It does not establish whether DJ.Studio supports recovery. The query and raw result are retained in `.local/overnight-21/`. MixMeister Fusion's shipped x86 binary separately contains the abnormal-shutdown recover-playlist question at `0x5f5394` (`ev_993cb1ec55d6bbd50872f149f9e7fe1a08e71adbfff4dd7996aae4e02875c268`); REA returned no direct xrefs for that string. Its persistence policy is unresolved. Mixworks recovery behavior is specified independently in RECOVERY-AND-MONITORING.md.

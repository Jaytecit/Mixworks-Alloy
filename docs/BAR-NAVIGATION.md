# Bar jumps and linked-track moves

The verified 0.43 build adds two menu groups:

- **Mix → Jump backward / forward → 4 / 8 / 16 / 32 bars** seeks by mix beats from the current cursor. It clamps at session start and the selected audible end. Ordinary playback retains its running state; arriving at the ending permits the normal device callback to stop there. Jumping changes no musical data, dirty state or Undo history.
- **Edit → Move linked track backward / forward → 4 / 8 / 16 / 32 bars** moves the selected track's start by that many integrated mix beats. Existing linked tracks follow through the model's normal relationship rules. Source cues, effect owners and ordinary playhead position are retained. A successful changed command has one Undo.

These commands use the existing tempo map, including ramps. They do not infer a new source beat grid or claim new automatic beatmatching. Pending placement/tempo work and busy workflows defer the commands. Musical locks and protected neighbours remain authoritative. A move that would push any linked predecessor before the mix is rejected as a whole; it does not silently shorten the requested step.

Duplicate is also prepared before committing: an unavailable owned effect/automation mapping reports its failure while retaining the current project, selection, ownership, history and dirty state. Successful duplication still creates an independent effect owner with one Undo. Preparation and validation happen before recording the edit.

This was the 0.43 bar navigation/movement foundation. Direct range selection, bar repeat/remove and independent private preview remain planned work; the separate global ripple commands below extend this foundation. Frozen candidate01 passed **1,705 checks across 22 suites, zero failures/errors** (`.local/b33-g1/summary.json`), with all 162 code/harness hashes unchanged. Focused timeline checks passed477; the full timeline suite passed481 with real plugins. VerifyOnly and deployment accepted the exact executable SHA256 `a7590dc7bf1e562808f31e45851e94b8fb9b3c496df02c28d31ec81af74e8195`. See HANDOVER for the current launcher and candidate.

The 29 new native-menu and failure-atomicity checks discover actual commands by visible group/count labels and cover every count/direction, tempo ramps, bounds, protected chains, preparation/busy rejection, live flags, Undo/Redo and failed/successful clone ownership. The malformed owned automation fixture establishes rejection safety; it is not a native-provider equivalence claim. Existing paired swap handles and adjustable rehearsal context remain included. The user application was not restarted.

## Global ripple and gap closing

The 0.55.30 implementation adds two commands in Editing, Edit and the timeline context menu:

- **Ripple delete segment** / **Ctrl+Shift+Delete** removes an isolated, unpaired selected segment and its entire elapsed time range across all lanes.
- **Close gap before segment** / **Ctrl+Shift+G** closes globally empty time from the latest earlier segment end to the selected segment start. For the first group of segments it closes initial silence from engine time zero. Another lane covering that time prevents a global gap.

Both commands move every later segment across all lanes by the removed elapsed time, preserving complete linked pairs, source audio, effects, source-owned automation, fades, manual alignment and audio slip. Session labels inside the half-open removed range are deleted; later labels, tempo markers and mixer-lane/master/FX automation move with the timeline. Tempo ramps and automation bends/steps retain their values outside the removed range. The playhead moves with later content or clamps to the seam if it was inside the removed range. One Undo/Redo restores/reapplies the document. Ordinary Cut and Delete keep leaving a gap.

An overlap with another segment, a pair crossing the range, removal of a paired segment, an affected protected transition, preparation/recording, or an incompatible captured source clock rejects the entire operation. Later elastic playback currently requires Beatmatch preparation first; the operation never silently regenerates or changes its renderer. These are explicit whole-segment/empty-gap commands; arbitrary selected-range surgery and authoring protected ranges remain open. Saved format30 and analysis11 stay compatible. HANDOVER owns exact final verification and local staging status.

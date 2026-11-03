# Range editing and multiple segment selection

From 0.55.59, ordinary additions and splits do not create transitions. Use **Add Transition** to connect overlapping tracks, or Auto Arrange. Unlink retains current automation and preserves hard cutoffs only when the removed transition's outside-bounds mute was enabled. See [timeline controls](TIMELINE-EDITING.md).

From 0.55.62, split pieces of one placed track share their VST effects and effect-parameter automation. Select any piece to add, remove or adjust an effect for that original track. Its effect curve spans the surviving pieces, deleted gaps and the effect ending. Separately added tracks, including another placement of the same source, have independent effects. Per-piece volume, EQ, source fades and source edits remain available.

Effects receive the combined audio of the pieces once per audio block, then continue processing silence through gaps and their reported tail duration. Cutting or deleting source audio therefore does not cut off an echo. The **Effect ending** setting supplies a minimum duration for effects that do not report a usable tail; 0 uses automatic reported endings. Export can include that tail or deliberately stop at the last segment. If pieces move between lanes, the shared effect return follows the latest started piece's lane.

A plain click on one member of a selected group selects just that piece; dragging still moves the group. **Delete this piece** in the timeline menu removes the piece under the pointer, even if a group was previously selected. **Delete selected audio** remains available for intentional group/range removal. Deletion leaves gaps and retains other pieces and their shared effects. Undo restores the edit. Session format40 stores shared effect ownership and the original track automation clock.

The0.55.43 editing increment adds direct audio ranges and segment groups to the existing timeline. This guide describes the implemented scope; current acceptance and exact candidate identity remain in [HANDOVER](../HANDOVER.md).

## Select audio

Open **Editing → Audio edits**. **Select audio** starts active and **Free drag** is the default: drag over a waveform to highlight any length, without global Snap changing the endpoints. Selection dragging does not seek, change audio or add Undo. Drag backwards if convenient. A single waveform click places the cut/paste cursor; press **S** or **Split at playhead** to slice there after clearing any range/group.

Choose **Snap drag to beats** to use the existing beat subdivision for selection; Alt bypasses it. In **Move tracks**, Shift-drag on the waveform temporarily selects audio. Shift at the distinct header movement handle retains audio slip. Ordinary header dragging moves the track in either tool.

Choose **1 beat**, **1 bar**, **8 bars**, **16 bars** or **32 bars** to select that musical length with a waveform click. Bar choices use the current constant-tempo4/4 project grid. **Exact range...** accepts start/end project seconds relative to musical zero, including available preroll. The summary identifies segment count, interval and duration.

Ranges belong to the selected segments. They do not silently include other simultaneous lanes. To select the same range on several segments, select that group first, enable **Select audio**, then drag on a member. Audio outside each source's available/audible span is excluded from Copy.

## Select several segments

With **Move tracks** selected, **Ctrl-click** adds/removes a segment. **More edits → Select all tracks** or **Ctrl+A** on the timeline selects every placed segment. A highlighted member remains the active owner for explicit individual-parameter tools. **Clear selection** or **Escape** clears the group/range; Escape during an active range drag restores the preceding selection. Text editors retain their own Ctrl+A.

Selection is transient: it is absent from saved mixes and musical Undo. Undo/Redo, another owner or a new document clears stale selection. Group movement retains the moved selection; other edits may clear or replace it. Selecting a node switches to that node's owner; node Delete remains a node action.

## Edit the selected scope

- **Copy / Ctrl+C** copies the selected audio range or segment group, with independent effect snapshots and shared original media. It changes no source file or mix data.
- **Cut / Ctrl+X** copies and removes that scope with one Undo, leaving a gap. **Delete** or **Remove** removes it without changing the clipboard. Ordinary range deletion keeps surviving source slices and all other positions.
- **Paste at playhead / Ctrl+V** places the fragment at the snapped cursor. Its earliest selected lane maps to the chosen destination mixer lane; other lane offsets and project-time offsets stay together. Effects get separate ownership. Existing project tempo remains the destination clock; beatmatched source duration follows that clock.
- Paste refuses occupied destination lanes/times, lane overflow, unavailable sources and incompatible fixed/changing source clocks. Choose an empty lane/time. It does not silently overwrite or move existing audio.

Range/group clipboard and audio edits require stopped playback/rehearsal, finished automation recording and Read mode. Preparation, export and transition locks protect edits. Refusal preserves the mix, selection and clipboard. Each successful Cut/Delete/Paste has one Undo/Redo transaction. Selection itself remains available during playback.

## Unlink pieces for independent movement (0.55.58)

Connected segments show the same **[LINK n]** prefix in their headers. The number identifies the current transition chain, using its first segment's arrangement position; it can change as the arrangement changes. Hover a waveform for movement guidance.

Select a whole segment, then choose **Unlink / move independently** in its right-click menu, or **Unlink** in Editing. Ctrl-select several segments to unlink their connections together. The action removes all transitions touching the selected segments, including their links to unselected neighbours. It clears the temporary group selection, so the next waveform drag moves just the piece under the pointer. Undo restores the complete linked state in one step.

Source spans, positions, effects and existing audible fades/automation stay intact. Transition-only hard playback boundaries become source-owned zero-length fade boundaries, keeping previously silent heads/tails silent. Current node-based fades and automation are retained; exact legacy procedural fade compatibility is not a development requirement. The detached IN/OUT alignment anchors are cleared so ordinary dragging cannot silently rebuild the removed relationships. Source audio and analysis are unchanged. The existing saved format is retained.

Stop playback/rehearsal and finish preparation/automation in Read mode first. Clear any audio-range selection; unlink acts on whole segments. Locked connected transitions or unavailable/preparing partners refuse atomically. Existing incompatible legacy automation can also refuse conversion with an explanation.

To move independent pieces together later, Ctrl-click them with Range off and drag a selected waveform. Escape clears that group. The hexagonal header arrow still moves the segment and following arrangement; use the waveform body for individual positioning. Splitting and ordinary insertion retain their existing transition behaviour in this update. Explicit-only transition creation is a separate placement change.

## Move a selected group (0.55.54)

With **Move tracks** selected, Ctrl-click the segments to select them, then drag the waveform body of a selected member. The outlined destination previews the whole group; release applies one move and one Undo. Escape cancels the preview and keeps the selection. Returning to the starting position makes no edit. Existing node, cue, transition and header handles keep their own actions.

All members move by the same time and lane displacement. Snap rounds that displacement from the grabbed member's original alignment; Alt bypasses Snap. Original source spans, slip, reverse, loops, source automation and complete internal transition links travel together. The playhead, project tempo, session markers and lane/master automation stay in place. Moving to different mixer lanes uses those destination lanes' processing.

Movement requires stopped playback and completed preparation/automation in Read. Select both ends of every connected transition. Locked links, unavailable sources, partial audio-range selections, lane overflow, negative starts and collisions with unselected segments refuse the move. A document/source change during the preview also refuses the release. No partial move or Undo entry is created on refusal.

This is a rigid move: captured source clocks must have the same tempo shape at the destination, including slipped audio. Moves across incompatible tempo ramps refuse instead of stretching members independently or rewriting the project tempo. Changing-tempo group retiming remains later work.

## Repeat and Crop (0.55.55)

**Crop to range** in Editing, or **Crop selected segments to range** in the timeline menu, keeps the chosen range on the selected segments. It removes their material outside that range and leaves other segments and timeline positions in place. Selected segments with no intersection are removed only when other selected audio remains inside the range. An entirely empty range refuses. Trimming or removing linked-transition participants also refuses; use unlinked material or copy the range instead.

**Repeat...** in Editing, or **Repeat selected audio...** in the timeline menu, adds copies immediately after the selected range or whole-segment group. With no range/group selected it repeats the active segment. Enter the number of **additional copies**, from 1 to 32, with at most 128 new segments per operation. The preview shows the destination interval and segment count, or explains an occupied destination and disables Repeat. Cancel leaves the mix unchanged.

Repeats preserve the selected interval, including empty portions within a range/group, relative lanes and source timing. Each new segment has its own ID and effects; whole selected groups carry their complete internal transition links. Existing source audio is shared. A range inside a longer segment usually has occupied audio immediately after it: Crop first when you intend to replace that remainder with repetitions. Repeat does not overwrite existing audio. Destinations with incompatible tempo curves refuse.

Each successful Crop or Repeat creates one Undo. Both keep the playhead and clipboard unchanged, require stopped playback and finished preparation/automation in Read, and revalidate current state before committing. Repeat refuses while transitions are locked. A stale Repeat dialog must be opened again. Changing-tempo repetition and partial linked-transition cropping remain outside this increment.

## Replace selected audio (0.55.57)

Copy a segment, range or group, then select the destination segments and optional range. Choose **Replace...** in Editing or **Replace selected audio...** in the timeline menu. The preview shows the destination interval and copied segment count. Cancel keeps the mix unchanged; Replace commits once with one Undo. Without a range/group, the active segment is the destination.

The clipboard starts at the destination range start (or the earliest selected segment start). Its earliest lane maps to the earliest selected lane, keeping relative time and lane offsets. Every copied lane must map to a selected destination lane. At most 128 copied segments are accepted, all fitting inside the destination interval with matching source timing. No stretching or truncation is applied; shorter clipboard audio leaves a gap in the rest of the interval. Empty portions within a copied range/group remain gaps.

Only selected destination segments are removed or sliced inside that interval. Their exterior audio stays at its original position and source phase. Other segments stay intact, and collisions with them refuse the edit. Lane/master automation, labels and project tempo stay in place; orphaned tempo-marker ownership is detached without changing the clock. The clipboard and playhead stay unchanged. Copies get fresh IDs and independent effects; surviving destination slices retain their original shared effects.

Complete selected transition pairs can be replaced together, and whole copied pairs retain fresh internal links. Partial linked-transition replacement refuses. Playback/rehearsal, automation recording, preparation/export, locked transitions, unavailable sources and incompatible source clocks also refuse. An intervening document, selection or clipboard change invalidates the preview; acceptance rechecks readiness. This increment does not provide transition splicing or arbitrary tempo retiming.

## Remaining work

Partial deletion of a segment participating in a linked transition refuses rather than changing a transition implicitly. Copying a range from linked material is supported: effective preset curves are captured in a private copy, with the original's hard audible boundaries. Complete internal pair relationships can travel with a whole-segment group; external relationships are not attached to pasted copies.

Node-group copy/move, saved protected ranges and general ripple splicing remain later work. Single-segment Split/Reverse/Loop/settings and existing global ripple/gap-close controls are unavailable for an active range/group; clear it first. This avoids applying a single-segment command to one hidden member.

Original audio, retained analysis/corrections, caches, arrangements and downbeat references are preserved. The initial range increment retained session34; group movement retains the current session38 and analysis11 formats. See [parity plan](DJSTUDIO-EDITING-PARITY-PLAN.md) and [testing policy](TESTING.md).

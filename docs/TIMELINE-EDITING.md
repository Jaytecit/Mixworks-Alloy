# Timeline editing

Current workflow for 0.55.64, analysis11/session41. Earlier implementation notes and their exact measurements survive in the [dated timeline guide (archived)](MAINTENANCE-20261103.md). They do not define current formats, transition defaults or automation ownership.

## Add and select audio

Library/file drops use the requested time/lane with the selected snapping policy. Ordinary add/drop, insertion, split and movement create independent segments. Auto Arrange intentionally creates links. Multi-row Library dragging captures displayed order at drag start and commits one batch/Undo; later sorting does not change the payload.

Editing opens on **Audio edits**. In **Select audio**, click the waveform to place the cut/paste cursor or drag to highlight a passage. **Free drag** ignores global Snap; **Snap drag to beats** and fixed musical lengths are explicit alternatives. In **Move tracks**, Ctrl-click builds a set and Shift-drag temporarily selects audio. Track headers remain movement handles. Drag selection does not seek or alter audio/history. See [range editing](RANGE-EDITING.md).

## Edit segments and groups

Audio edits keeps Split at playhead, Cut, Copy, Paste and Delete visible. The selection summary names the track, lane, bounds and duration. **More edits** groups selection, Repeat/Crop/Replace/Unlink and explicitly all-lane time-closing actions. **Fades & timing** contains per-track source-in/length/fades/gain and reverse/loop; opening it clears the transient range. **Automation** contains owner and curve controls. Ordinary deletion leaves a gap. Delete this piece addresses the waveform under its menu; Delete selected audio intentionally applies to the selected group/range. Successful paste moves the cursor to the pasted end; consecutive pastes retain that exact end.

Repeat previews1–32 additional copies with a128-new-segment bound; Crop retains the selected interval; Replace keeps exterior slices and unaffected lanes. Complete internal pairs and source timing are preserved where supported. Invalid collisions, locks, partial linked surgery, unavailable/stale content and incompatible captured clocks refuse before committing. Group/range structural edits require stopped/Read admission. Each admitted edit has one Undo. Partial linked-transition splicing, node groups, protected ranges and changing-tempo group retiming remain open.

**Ripple delete segment** and **Close gap before segment** explicitly move all later lanes and complete linked pairs, with tempo/labels/session automation. Ordinary Cut/Delete does not ripple. Before/after insertion is separate and creates no ordinary join. See [bar navigation](BAR-NAVIGATION.md) and [insertion](ARRANGEMENT-INSERTION.md).

## Movement and source alignment

With Snap enabled, existing-segment movement snaps displacement from its current phase, preserving manual alignment. Alt/disabled Snap allows free alignment. Hold Shift at the header movement handle to slip source audio while retaining geometry; double-click resets the audio-only offset. Source/trim/loop/reverse mapping remains distinct from programme time.

The header following-track handle moves the selected segment and the full following arrangement suffix, preserving its incoming frame and earlier material. This has a larger scope than a body drag. I/O footer handles address the named incoming/outgoing song within an existing frame. Transition locks and bounds still apply.

IN/OUT anchors have reachable edge tabs when zoomed off-screen. Drag displacement starts from the true anchor to avoid jumps. Reusable source S sections and placed HC cues remain independently editable; see [sections](SECTION-FLAGS.md), [hotcues](HOTCUES.md) and [protected downbeat records](DOWNBEAT-REFERENCES.md).

## Explicit transitions

Choose **Add Transition** for two overlapping tracks, or select one and choose its overlapping partner. Existing preset/length defaults apply without moving either song. Ordinary insertion makes no join. Frame edges are source-relative and independent of fades. Frames may overlap across all four lanes and exceed64bars within the shared audio. Length accepts typed positive bars.

**Mute outside bounds** is per-transition, saved/undoable and off by default. When enabled it cuts the incoming head before the frame and outgoing tail at/after its end; otherwise exterior audio follows its own fades/controls. Unlink / move independently removes touching links in one Undo without manufacturing another relationship. See [transition controls](TRANSITION-PRESETS.md).

## Automation and effects

Current development preview: choose Volume/Bass/Mid/Treble/Pan/Filter from the track footer or Editing to show one shared lane curve. Neutral handles mark each track start, first eight bars, last eight bars and end; short tracks merge inner handles at the midpoint and coincident handles merge. They do not create automatic fades. Untouched handles follow geometry/tempo; authored nodes remain at project times, including across moves and deletion. New tracks inherit the existing lane curve. Only the chosen parameter is shown. Per-track transition fades and FX parameters remain available through explicit owners. See [ua05 verification and limits](release-audit/UA05-SHARED-LANE-AUTOMATION-20261101.md); the normal launcher is not yet updated.

Editing exposes owner/parameter, Add here/at playhead, previous/next, exact time/value, Delete, Read/manual state and FX selection. Browsing is view-only; node RMB offers Edit/Delete/Add. Drag nodes in time/value, bend between nodes, or enter exact values. Gain has a100% movement detent and130% upper scale; selected node/cut captions use percentages. Parameter names/physical units remain in precise tools and hints.

Horizontal paired handles move a swap's timing; vertical amount handles retain its time. D/Shift+D cycle amounts, Up/Down changes one percent (Shift ten), Enter/double-click/RMB opens exact entry. Escape/Cancel preserves the previous state. Locks, writes, preparation and stale-dialog checks protect mutations.

Ordinary built-in controls, MIDI and performance faders use the mixer lane, including when a track is selected. Explicit per-track transition curves and VST controls retain their original-track owner across surviving split pieces and deleted gaps. Separate placements remain independent for those per-track controls. Piece geometry/source fades remain local. Shared processors continue through gaps and finite tails; Effect ending supplies a manual minimum/fallback for unreported tails. See [ordinary preset nodes](PRESET-NODES.md) and [Arrange/Record-Perform](ARRANGE-RECORD-PERFORM-PLAN.md).

## Grid and tempo

Beat/Half beat/Quarter beat selects1/2/4 divisions; Alt bypasses supported snapping. Fine guides appear with sufficient pixel spacing. BPM/key/source-downbeat drafts save durably before dismissal. Save source only retains existing captured playback clocks; Save + rebuild explicitly reapplies timeline planning and can move tracks/replace transitions. Correction saves remain separate from musical Undo.

New automatic changed-BPM targets require64beats/16bars between neighbours. If there is insufficient source/ramp room, placement follows the existing map. Live additions preserve established audio/manual markers before adding a future target. Deliberate Cut may switch immediately. Manual/existing maps are not retroactively spread. Delete removes owned automatic markers; New invalidates pending tempo jobs.

View / Pre-roll before bar1 adds musical headroom while retaining quiet intros. Musical origin rebases the coordinated clocks; Home/Stop reaches the intro. Removing pre-roll rejects if retained material would be cut. Maximum zoom is1,600 px/s; overview edges and Follow are described in [layout](TIMELINE-LAYOUT-049.md).

## Workspace and access

Editing collapses/detaches beside Controller/Transitions; VST/Shortcuts remain at right. Named layouts and console order are implemented. Native musical objects expose selection, values and guarded precise editors, including tempo markers and cut amounts. Focus reveals without seeking; dense traversal is bounded with previous/next access. Complete physical gesture, screen-reader/DPI and listening acceptance remain open in the [request register](REQUEST-STATUS-20261024.md).

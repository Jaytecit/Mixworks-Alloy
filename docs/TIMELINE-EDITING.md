# Timeline editing

Workflow reference; exact current candidate and verification limits are in [HANDOVER](../HANDOVER.md). Earlier implementation notes and their exact measurements survive in the [dated timeline guide (archived)](MAINTENANCE-20261103.md). They do not define current formats, transition defaults or automation ownership.

## Add and select audio

Use the timeline toolbar's audio lane selector to show two, three or four lanes.
Hidden lanes continue playback. Restore a lane to edit its contents.

Library/file drops use the requested lane and normally place audio at the drop time with the selected snapping policy. With a manually set, unpaired OUT, drop the next track in another lane anywhere across the outgoing track to align its IN to that OUT. The transition starts there and uses the selected default preset across the remaining shared audio. The timeline holds its existing tempo through the join, then ramps to the incoming BPM when there is enough audio. A shorter track gets an editable marker at the current tempo. The pair starts unlocked. Click **Unlocked** on the incoming track's header to move both tracks together. Unlock it to adjust either alignment marker. During playback the incoming track must start ahead of the playhead to join the OUT.

Appending after a manually set, unpaired OUT also aligns the next track. Other ordinary placements create independent segments. A reliable source downbeat aligns to a project bar. An append uses the next valid position after the last clip, and a drop uses the closest valid position to the drop. Sources with a reliable beat and unknown downbeat align to a beat. Auto Arrange creates links. Multi-row Library dragging captures the displayed order at drag start and commits one batch with one Undo.

Drag across the waveform to highlight audio, then right-click the highlight for applicable tools. Drag the title/header to move the placement; drag either narrow outer edge handle to trim visible and audible content. Ctrl-click builds a group. Selection follows the project Snap by default; Alt bypasses it. Right-click a segment for **Segment settings…** or press **Ctrl+T**. The ruler's **Range** control offers snap overrides and fixed selection lengths. **Shift+F10** opens the focused timeline object's menu. Selection dragging keeps the playhead, audio and musical history unchanged. See [range editing](RANGE-EDITING.md).

## Edit segments and groups

If an edit is unavailable, its disabled context-menu row explains the next step.
Core editing buttons expose the same reason in hover help and their accessible
description. A shortcut or a menu opened before the state changed checks again
and reports the reason in the status area. Finish/cancel export, wait for source
preparation, unlock the transition, or stop playback and choose Read for a
range/group edit as indicated. An empty Paste asks you to copy audio first.
These explanations do not change the mix or seek playback.

Audio edits keeps Split at playhead, Cut, Copy, Paste and Delete visible. The selection summary names the track, lane, bounds and duration. **More edits** groups selection, Repeat/Crop/Replace/Unlink and explicitly all-lane time-closing actions. **Fades & timing** contains per-track source-in/length/fades/gain and reverse/loop; opening it clears the transient range. **Automation** contains owner and curve controls. Ordinary deletion leaves a gap. Delete this piece addresses the waveform under its menu; Delete selected audio intentionally applies to the selected group/range. Successful paste moves the cursor to the pasted end; consecutive pastes retain that exact end.

Repeat previews1–32 additional copies with a128-new-segment bound; Crop retains the selected interval; Replace keeps exterior slices and unaffected lanes. Complete internal pairs and source timing are preserved where supported. Invalid collisions, locks, unavailable/stale content and incompatible captured clocks refuse before committing. Group/range structural edits require stopped/Read admission. Each admitted edit has one Undo. Partial linked Cut/Crop/Replace converts affected treatments to ordinary nodes and unlinks those frames before slicing. Menus and replacement preview explain this; unselected audio keeps its heard treatment and exterior cuts. Node groups, protected ranges and changing-tempo group retiming remain open.

**Ripple delete segment** and **Close gap before segment** explicitly move all later lanes and complete linked pairs, with tempo/labels/session automation. Ordinary Cut/Delete does not ripple. Before/after insertion is separate and creates no ordinary join. See [bar navigation](BAR-NAVIGATION.md) and [insertion](ARRANGEMENT-INSERTION.md).

## Movement and source alignment

With Snap enabled, the track's pulse snaps to the timeline grid. Alt or disabled Snap allows free alignment. Moving a single track or a movement-locked pair follows the destination tempo curve with the same source span. Tempo markers stay at their timeline positions. Ctrl-selected groups retain rigid movement. Hold Shift at the header movement handle to slip source audio while retaining geometry. Double-click resets the audio-only offset.

The header following-track handle moves the selected segment and the full following arrangement suffix, preserving its incoming frame and earlier material. This has a larger scope than a body drag. I/O footer handles address the named incoming/outgoing song within an existing frame. Transition locks and bounds still apply.

IN/OUT anchors have reachable edge tabs when zoomed off-screen. Drag displacement starts from the true anchor to avoid jumps. Reusable source S sections and placed HC cues remain independently editable; see [sections](SECTION-FLAGS.md), [hotcues](HOTCUES.md) and [protected downbeat records](DOWNBEAT-REFERENCES.md).

## Explicit transitions

Choose **Add Transition** for two overlapping tracks, or select one and choose its overlapping partner. Existing preset/length defaults apply without moving either song. Ordinary insertion makes no join. Right-click a frame header for pair-specific actions or use **Edit transition…** to open the dockable editor. Frame edges are source-relative and independent of fades. Frames may overlap across the audio lanes and exceed64bars within the shared audio. Length accepts typed positive bars.

**Mute outside bounds** is per-transition, saved/undoable and off by default. When enabled it cuts the incoming head before the frame and outgoing tail at/after its end; otherwise exterior audio follows its own fades/controls. Unlink / move independently removes touching links in one Undo without manufacturing another relationship. See [transition controls](TRANSITION-PRESETS.md).

## Automation and effects

Choose Volume, Bass, Mid, Treble or Filter beneath a selected waveform to edit that track’s curve. The same curve contains nodes inside and outside transition frames, including those placed by presets and Auto Arrange. Editing’s Track owner exposes the same curves and includes Pan and effect parameters.

Mixer-lane Volume, EQ, Pan and Filter are manual controls. In Arrange, Touch and Latch record into the playing track’s curve on the armed lane. If tracks overlap on one lane, the selected playing track takes priority, followed by the most recently started track.

Editing exposes owner/parameter, Add here/at playhead, previous/next, exact time/value, Delete, Read/manual state and FX selection. Browsing is view-only; node RMB offers Edit/Delete/Add. Drag nodes in time/value, bend between nodes, or enter exact values. Gain has a100% movement detent and130% upper scale; selected node/cut captions use percentages. Parameter names/physical units remain in precise tools and hints.

Horizontal paired handles move a swap's timing; vertical amount handles retain its time. D/Shift+D cycle amounts, Up/Down changes one percent (Shift ten), Enter/double-click/RMB opens exact entry. Escape/Cancel preserves the previous state. Locks, writes, preparation and stale-dialog checks protect mutations.

Track curves and VST controls retain their original-track owner across surviving split pieces and deleted gaps. Separate placements have independent track curves. Shared processors continue through gaps and finite tails. Effect ending supplies a manual fallback for unreported tails. [Preset nodes](PRESET-NODES.md) · [Arrange and Record/Perform](ARRANGE-RECORD-PERFORM-PLAN.md)

## Grid and tempo

Beat/Half beat/Quarter beat selects1/2/4 divisions; Alt bypasses supported snapping. Fine guides appear with sufficient pixel spacing. BPM/key/source-downbeat drafts save durably before dismissal. Save source only retains existing captured playback clocks; Save + rebuild explicitly reapplies timeline planning and can move tracks/replace transitions. Correction saves remain separate from musical Undo.

New automatic changed-BPM targets require64beats/16bars between neighbours. If there is insufficient source/ramp room, placement follows the existing map. Live additions preserve established audio/manual markers before adding a future target. Deliberate Cut may switch immediately. Manual/existing maps are not retroactively spread. Delete removes owned automatic markers; New invalidates pending tempo jobs.

View / Pre-roll before bar1 adds musical headroom while retaining quiet intros. Musical origin rebases the coordinated clocks; Home/Stop reaches the intro. Removing pre-roll rejects if retained material would be cut. Maximum zoom is1,600 px/s; overview edges and Follow are described in [layout](TIMELINE-LAYOUT-049.md).

## Workspace and access

Editing collapses/detaches beside Controller/Transitions; VST/Shortcuts remain at right. Named layouts and console order are implemented. Native musical objects expose selection, values and guarded precise editors, including tempo markers and cut amounts. Focus reveals without seeking; dense traversal is bounded with previous/next access. Complete physical gesture, screen-reader/DPI and listening acceptance remain open in the [request register](REQUEST-STATUS-20261024.md).


## Audio edges and transition markers

The two narrow bars at a waveform's outer edges trim this placement. Audio beyond
the retained range is hidden and excluded from playback and offline rendering.
Extend an edge to restore available source content. The original file is untouched.
The title, transition frame, IN/OUT markers and fades have separate handles.

Trimming keeps source alignment and other placement positions. Transition markers
can stay outside the retained waveform; the status explains that their excluded
audio is silent. Extend an edge or change the treatment to resolve that conflict.
Shared effects returns retain their original mixer-lane clock when a trim crosses
another piece. Undo/Redo and session format48 preserve these choices.

Locks, unfinished source/tempo work, exports and active recording can make a trim
unavailable. A prepared passage cannot extend beyond its prepared PCM until audio
is rebuilt; an older paired-bass treatment must be replaced by a current preset.
These refusals explain the next step and keep the placement unchanged.

## Automation in plain language

Read plays the saved curve without recording movements. Touch records a moved
control on its armed lane during playback, then returns after half a second without
input. Latch keeps writing its last value until Stop or Read. Arm permits a lane
or master to record; it does not start playback or writing. Curve Read only enables
that particular curve. Lane curves affect all tracks on that lane and combine
with each track's treatment.

Applying a transition over existing track nodes shows the target pair and range
before replacing them. Exterior points and mixer-lane curves remain; shared
transition overlaps are identified. Playback continues while the question is open.

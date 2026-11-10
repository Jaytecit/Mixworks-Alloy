# Transition controls and presets

Ordinary placement and OUT/IN alignment leave the tracks' treatment alone. A marked pair shows **OUT/IN | No preset** until a preset is applied. [Ordinary preset nodes](PRESET-NODES.md) covers curve editing and the automated arrangement workflows.

## Create or select a transition

Ordinary tracks are independent. Use Add Transition for an overlapping pair, or Auto Arrange/Auto Arrange timeline for deliberate automatic relationships. Right-click the frame header for the pair's presets, length, locks, mute and removal. **Edit transition…** opens the optional dock for detailed treatment and **Rehearsal and presets…** opens comparison controls. Previous/Next or **Alt+Left/Right** navigate across pairs. Frame selection and direct editing preserve the playhead.

An applied preset is labelled in the timeline header. Drag the header to move its two sides together within the shared audio, or drag its right edge to change duration. The tracks and their OUT/IN markers stay fixed. **Remove preset** in the header menu removes the generated nodes and keeps the alignment and manual nodes. An older frame with unlabelled automation can be unlinked while keeping its curves. Mute outside bounds is optional, off by default and saved with the pair.

Deleting or replacing a whole track breaks its pairs and clears their preset and manual automation inside the former frames on surviving tracks. Manual nodes elsewhere and the surviving OUT/IN marker stay available. Undo restores the original tracks and treatment.

## Apply and edit a treatment

Explicit Crossfade, Bass swap and saved presets place editable source nodes at the aligned OUT/IN join. Both Crossfade start nodes sit on that join. Manual nodes remain on their original tracks and may shape the curve within a preset. If a generated point would coincide with an existing point, Alloy keeps the existing curve and reports the conflict. Reapplying replaces the pair's previous owned nodes. The Playlist shows the applied preset by name. Independent band and Filter controls edit their selected curve separately.

Volume/Bass/Mid/Treble rows expose supported None/Crossfade/Fade in/Fade out/Swap/Manual choices; Bass also has Bass fade handover. Strength controls attenuation depth; EQ full cut and Volume silence have distinct DSP semantics. Gap and crossing position use mix beats, including project ramps. Bass/Mid/Treble sharp swaps use sample-timed steps; Volume retains anti-click treatment. Native bends and sparse approximation points preserve continuous shapes where required.

Click a line/footer parameter to reveal nodes; Add inserts the requested point. Drag timing/value, edit exact values or bend between nodes. Horizontal paired handles move an identifiable swap together; vertical amount handles change depth at fixed time. RMB Swaps distinguishes placing a swap from pairing an existing bass cut. Ambiguous shapes refuse guessed handles. See [node controls](PRESET-NODES.md), [swap controls](SWAP-HANDLES.md) and [filters](TRANSITION-FILTERS.md).

## Section and new-track defaults

Use **Defaults for new transitions…** from the segment or Mix menu to choose a treatment and length for future joins. The Add Transition command shows the current choices. Ordinary placements stay independent. Section planning keeps paired source boundary identities, prefers usable changes on both tracks then a one-sided/grid fallback, and can exceed the usual preference where shared audio permits. A measured shared boundary can position Bass swap away from the midpoint. Energy hints are not semantic verse/chorus/vocal truth.

Auto Arrange offers seven styles, soft energy/genre preferences and Extend. New changed-BPM targets require64beat spacing; short ramp room falls back to the existing map. [Auto Arrange](AUTO-ARRANGEMENT.md) owns limits and fallback behavior.

## A B C and reusable presets

Store A/B/C captures the selected treatment; Apply deliberately changes the frame with one Undo. Named session presets can be saved/deleted and imported/exported as `.alloypreset`. Portable ordinary templates retain normalized two-side parameter shapes; they do not transfer arbitrary source/plugin state or move songs. Save named choices before a whole-arrangement rebuild that replaces relationships.

In **Arrange**, Hear/Prepare rehearsal uses a snapshot and the master output, with context before/after and complete-window RMS attenuation matching for non-silent alternatives. End returns the previous cursor paused. Preparation/history-dependent plugins can differ from uninterrupted playback; after plugin edits prepare again. Temporary rehearsal buffers retain a256 MiB limit. This main-output rehearsal does not establish private routing.

In **Record/Perform**, private whole-transition A/B/C uses independent transport/DSP while audience playback continues. Apply/quantized Launch commits the named target; stale targets reject. Private actions/audio remain outside programme capture until explicit commit. See [mode contract](ARRANGE-RECORD-PERFORM-PLAN.md) and [preview routing](LIBRARY-PREVIEW.md).

## Locks and acceptance

Locks guard participating/connected geometry and protected control edits. Global tempo/rebuild, partial pair edits, unavailable preparation and stale dialogs can refuse rather than guessing. Mutations use one Undo; selection/audition alone does not commit. Native third-party plugin state has its own limits.

Session format 50 saves preset ownership and names. Analysis stays at revision 11. Full real-music section/phrasing, physical controls/private routing and release acceptance remain open. See the [request register](REQUEST-STATUS-20261024.md).

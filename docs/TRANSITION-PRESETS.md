# Transition controls and presets

Current guide for 0.55.64. [Ordinary preset nodes](PRESET-NODES.md) owns exact replacement/capture semantics. [Earlier transition notes (archived)](MAINTENANCE-20261103.md) preserve dated generators/formats and measurements; old procedural layering and automatic-on-add descriptions are superseded.

## Create or select a transition

Ordinary tracks are independent. Use Add Transition for an overlapping pair, or Auto Arrange/Auto Arrange timeline for deliberate automatic relationships. Select the frame or navigate Previous/Next. Editing a frame/header/width does not seek; explicit navigation can reveal/seek its start.

Frame movement/edge resizing leaves songs fixed, within shared audio. Frames can overlap fully across four lanes without a64bar ceiling. Length accepts positive typed bars. Mute outside bounds is optional, off by default and saved/undoable. Unlink removes the relationship; see [timeline editing](TIMELINE-EDITING.md).

## Apply and edit a treatment

None/alignment-only, Crossfade, Bass swap, independent bands, Filter sweeps, named/portable templates and A/B/C place ordinary source-owned nodes. Applying replaces the selected frame/parameter shape; exterior points, clocks, locks and unrelated bands stay protected. Manual keeps the editable shape. Reapplying replaces rather than stacks hidden generators. The Playlist derives built-in labels from actual shapes; edits can show Custom, and exact named presets take priority.

Volume/Bass/Mid/Treble rows expose supported None/Crossfade/Fade in/Fade out/Swap/Manual choices; Bass also has Bass fade handover. Strength controls attenuation depth; EQ full cut and Volume silence have distinct DSP semantics. Gap and crossing position use mix beats, including project ramps. Bass/Mid/Treble sharp swaps use sample-timed steps; Volume retains anti-click treatment. Native bends and sparse approximation points preserve continuous shapes where required.

Click a line/footer parameter to reveal nodes; Add inserts the requested point. Drag timing/value, edit exact values or bend between nodes. Horizontal paired handles move an identifiable swap together; vertical amount handles change depth at fixed time. RMB Swaps distinguishes placing a swap from pairing an existing bass cut. Ambiguous shapes refuse guessed handles. See [node controls](PRESET-NODES.md), [swap controls](SWAP-HANDLES.md) and [filters](TRANSITION-FILTERS.md).

## Section and new-track defaults

New-track preset/length preferences apply to explicit planning/joins, not every ordinary placement. Section planning keeps paired source boundary identities, prefers usable changes on both tracks then a one-sided/grid fallback, and can exceed the usual preference where shared audio permits. A measured shared boundary can position Bass swap away from the midpoint. Energy hints are not semantic verse/chorus/vocal truth.

Auto Arrange offers seven styles, soft energy/genre preferences and Extend. New changed-BPM targets require64beat spacing; short ramp room falls back to the existing map. [Auto Arrange](AUTO-ARRANGEMENT.md) owns limits and fallback behavior.

## A B C and reusable presets

Store A/B/C captures the selected treatment; Apply deliberately changes the frame with one Undo. Named session presets can be saved/deleted and imported/exported as `.alloypreset`. Portable ordinary templates retain normalized two-side parameter shapes; they do not transfer arbitrary source/plugin state or move songs. Save named choices before a whole-arrangement rebuild that replaces relationships.

In **Arrange**, Hear/Prepare rehearsal uses a snapshot and the master output, with context before/after and complete-window RMS attenuation matching for non-silent alternatives. End returns the previous cursor paused. Preparation/history-dependent plugins can differ from uninterrupted playback; after plugin edits prepare again. Temporary rehearsal buffers retain a256 MiB limit. This main-output rehearsal does not establish private routing.

In **Record/Perform**, private whole-transition A/B/C uses independent transport/DSP while audience playback continues. Apply/quantized Launch commits the named target; stale targets reject. Private actions/audio remain outside programme capture until explicit commit. See [mode contract](ARRANGE-RECORD-PERFORM-PLAN.md) and [preview routing](LIBRARY-PREVIEW.md).

## Locks and acceptance

Locks guard participating/connected geometry and protected control edits. Global tempo/rebuild, partial pair edits, unavailable preparation and stale dialogs can refuse rather than guessing. Mutations use one Undo; selection/audition alone does not commit. Native third-party plugin state has its own limits.

Current session format is41 and analysis11. Legacy compatibility is not a development requirement. Full real-music section/phrasing, physical controls/private routing and latest-build release acceptance remain open; consult the [request register](REQUEST-STATUS-20261024.md).

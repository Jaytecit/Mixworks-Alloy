# Mixworks Alloy design

Current specification for 0.55.64. The approved direction is subtle studio hardware: graphite, brushed metal and copper, rounded hexagonal dials and restrained daily-workspace lighting. Source tokens remain in `Source/AlloyLookAndFeel.h`; [the archived design narrative (archived)](MAINTENANCE-20261103.md) preserves previous version notes and the approved concept references.

## Palette and type

Graphite surfaces establish depth; copper marks active controls/tempo, with blue/sage/mauve lane accents. Near-white focus, outlines, labels and names distinguish state without hue alone. Red is reserved for faults/clipping. Segoe UI uses compact readable controls, ellipsized source names with hints and larger important values. The header carries the build-derived running version.

## Workspace

Four mixer lanes share the timeline. Overall resizing equalizes height within one pixel; individual changes survive width-only layout. Ruler/overview/labels and tempo footer share one horizontal clock. The stereo PEAK panel follows their height. Both Library/Playlist and the right VST/Shortcuts inspector collapse/restore. Editing/Controller/Transitions dock, collapse, reorder or detach; named visual layouts are saved outside musical Undo. See [layout](TIMELINE-LAYOUT-049.md).

Timeline tools stay below their own waveform and never overlap another lane. Compact segments use a selector fallback. Use Bass/Mid/Treble, source/song for library audio, segment for placed audio, lane for mixer row and explicit original-track/private/master ownership. Selected node/amount captions show percentages; precise tools retain units. Popups, alerts, callouts and app effect-editor frames share the orange edge.

## Editing and transport

Ordinary placements remain independent; Add Transition/arranging deliberately creates links. Four-lane overlap and independent frames are allowed within available audio. Presets create ordinary nodes, empty curves invent none, and swap timing/amount handles are distinct. Selection preserves musical state; each committed gesture has one Undo. Locked/busy/unavailable/stale targets explain refusal. See [timeline editing](TIMELINE-EDITING.md).

Arrange and Record/Perform are the two top-level modes. Private preparation has explicit identity/status, independent transport/level and Apply/Launch. Missing headphones never use audience output. Keep take/master WAV outcomes and incomplete capture notices clear. Details belong in context hints and [the current mode contract](ARRANGE-RECORD-PERFORM-PLAN.md).

## Motion and access

Attract lighting is optional; Escape exits. Forge motion follows Windows or explicit Animated/Steady. The lane emblem's explicitly requested animation defaults on with Steady available. Grid brightness/Follow are independent preferences. Further forge intensity and skins depend on playback/readability acceptance. Native control/object names, focus and precise keyboard paths supplement mouse editing; full screen-reader/DPI/multiple-monitor qualification remains open.

# Four lane timeline layout

Workflow reference; exact current candidate and verification limits are in [HANDOVER](../HANDOVER.md). All four lanes remain available; eight-lane and early-format instructions are historical. Current session format is41; legacy compatibility is not a development requirement.

## Resize and navigation

The stretch grip below the tempo footer changes timeline height. Drag, use Up/Down (Shift for one pixel), or Home/double-click to reset. Overall height changes distribute space evenly across four lanes within one pixel. Individual lane-edge adjustments remain within the current height and survive width-only changes; the next overall resize equalizes shares.

The ruler, tempo footer and stereo PEAK panel follow the same geometry. Lower Editing/Controller/Transitions panels may collapse, resize or detach. Expanded stem rows share the available lane space. Docked compact Editing can reduce the timeline minimum to fit its controls; smaller/detached panels can scroll.

Drag the overview body to pan, or either edge to zoom while anchoring the opposite edge. Maximum zoom is1,600 px/s. Double-click fits the mix. Follow playhead is optional and saved outside musical Undo; manual panning disables it. Grid brightness is separate from zoom.

## Panels and saved layouts

Both Library/Playlist and right inspector can fully collapse, with an edge grip/keyboard restore and remembered width. Named layouts save visual geometry, console order and dock/detached bounds. They do not alter audio, musical Undo or MIDI profiles. See [workspace layouts](WORKSPACE-LAYOUTS.md), [timeline editing](TIMELINE-EDITING.md) and [stem controls](STEM-SEPARATION.md).

## Historical evidence

[Earlier layout narrative (archived)](MAINTENANCE-20261103.md) preserves 0.49/0.53/0.54.6 measurements. Current behavior supersedes its six-pixel/eight-lane/old-session guidance. Current qualification and remaining human display acceptance are in the [request register](REQUEST-STATUS-20261024.md).

# Four-lane timeline layout (0.54.6)



All four mixer lanes stay visible with selection, mute, solo, LPF, pan and level controls. The bottom stretch grip beneath the tempo footer is always available. Drag it or use Up/Down; Shift adjusts one pixel. Home or double-click restores the default height. With a tool panel docked, expansion stops before the reserved panel area. With both panels detached, the timeline can fill the centre.



Expanding or compressing the overall timeline distributes its height equally across all four lanes, including window-height changes and the bottom grip's mouse, keyboard and reset actions. Any leftover pixels are spread across the lanes, so their heights differ by at most one pixel. Individual lane-edge adjustments still work within the current timeline height; the next overall resize returns to equal shares. Width-only changes retain those individual adjustments.

Each lane retains at least 100 logical pixels in supported window sizes. Lane-edge resizing shares the fixed area with the other lanes; it cannot hide controls or create vertical scrolling. The fixed ruler, tempo footer and stereo peak panel follow the resized geometry. Layout preferences remain outside musical Undo.



New sessions use format 25 with four exact lane records. Formats 1–23 still open. Format 24 opens if lanes 5–8 have no clips or non-default state. Otherwise Alloy leaves the saved mix intact and asks you to move that work into lanes 1–4 using retained 0.54.1. Analysis revision 9, source audio, library data, corrections and audio caches are retained.



## Historical 0.49 implementation



Transport controls now sit between Waveform and Audio / MIDI. Less frequent file/edit buttons move into their existing menus as space narrows.



The freed row shows the whole mix with a draggable copper preview of the current timeline view. Drag or click it to pan; double-click fits the whole mix. Arrow keys pan, Home/End reach the ends, and +/- zoom. The detailed ruler and labels remain fixed above the lanes while following horizontal scrolling and zoom. Tempo markers retain their fixed footer.



Drag a lane boundary or the bottom edge of its control strip to resize that lane. Each lane ranges from a six-pixel line to twice its responsive default height. Double-click restores its default. With the edge focused, Up/Down resize (Shift makes one-pixel changes), Home resets and End doubles it. Sizes are saved in workspace preferences. Four default lanes fit with no timeline scrollbar; a vertical scrollbar appears when expanded lane content exceeds the available space. Controller and Transitions panels retain their independent scrolling.



A rendering mismatch sampled a waveform interval at its left edge while grid ticks used their nearest pixel. Waveform intervals and path vertices now share the tick's pixel centre. A native impulse fixture verifies the fractional-position case without changing musical state. This corrects a possible one-pixel display bias; it does not establish that every visible peak marks a musical downbeat or prove the complete offset in the supplied Terra Luna screenshot is eliminated.



Detection, BPM, downbeat anchors, playback, tempo preparation and session format are unchanged. Retained analysis, manual corrections, audio caches, library and arrangements are preserved. A read-only look at Terra Luna's retained revision-9 facts and an eight-second diagnostic decode were used for this reported example; they are not independent musical annotations or detector tuning. No other reserved recording was opened.



Verification and deployed binary identity are recorded in HANDOVER.md and .local/timeline-layout-41/.



The final immutable release passed **1,885 checks across all 22 required suites**, with zero failures or errors, including 253 interface checks, 564 timeline checks and native label gestures through the fixed ruler. Default and expanded-lane renders were inspected, including 200% scale.



In 0.53 the dropdown extends to eight lanes. Both panels undocked exposes a bottom timeline grip beneath the fixed tempo footer; drag to resize, double-click or Home to fill the available height. See [current operation and compatibility](../README.md#workspace-and-audio).


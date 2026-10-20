# Transition keys and keyboard editing

Version0.41 is available through the launcher.

Select a frame and open **Transition**. Its source keys appear as separate **Out** and **In** lines, using the selected Note/Camelot display. Known keys show signed movement around the12-position wheel, with wraparound, same-key or relative major/minor labels. Unknown keys stay Unknown; an uncertain automatic key retains `?` and does not produce a derived movement. Manual overrides take precedence. This display does not transpose audio or certify a musical match.

| Shortcut | Action |
| --- | --- |
| Alt + Left / Right | Select previous / next transition, bounded at the first/last pair. Like the panel buttons, selection seeks to the frame and reveals it. No arrangement Undo entry. |
| Ctrl + Alt + Left / Right | Move the selected frame one integrated mix beat earlier / later. Like dragging the frame, this keeps the outgoing song in place and carries the aligned incoming song and its following chain, retaining their source IN cues. |
| Shift + Alt + Left / Right | Resize only the frame's end by one integrated mix beat. |
| Alt + L | Lock / unlock the selected pair. |

The existing frame geometry, source cues, generated treatments and lock model own these commands. Tempo ramps use the integrated beat map; they do not assume a fixed seconds-per-beat. An edit outside shared audio, during pending preparation or affecting a protected neighbour is rejected without adding Undo. Successful changed edits have one Undo and keep the playhead. Ordinary arrows remain available to focused controls, and text entry and busy workflows retain the existing keyboard guard. Combining Ctrl+Shift+Alt with arrows is left unassigned.

The lower scrollable Transitions panel contains the key movement and shortcut help at narrow sizes. Its existing visible lock state, waveforms, tempo strip and molten orange pulse remain available. Frozen candidate03 passes **372 timeline checks**, including29 new key/keyboard cases. They cover source-key wheel wraparound/relative/unknown/uncertain states, the shared confidence boundary, native navigation, one-beat edits and linked source-IN retention, invalid resize and busy/pending guards, direct and neighbour locks, Undo/Redo, tempo-ramp integration and narrow-window layout. Both key and shortcut captures were inspected; initial clipped help was corrected. Candidate02 retained four failures from the incorrect assumption that an existing frame move leaves its incoming chain fixed; the tests and description now follow the existing geometry owner. The same frozen bundle passes **1,600 checks across22 suites**, with all156 code/CMake/harness hashes unchanged, zero failures/errors and accepted VerifyOnly/deployment. Launcher release **0.41.0-6279604d1d82**, SHA256 `6279604d1d8208918fb34e9f21fa7cfa4108746a0619e4c0fa57ddf6acaf618c`. The running user app was retained. No operating-system shortcut or physical controller acceptance is claimed.

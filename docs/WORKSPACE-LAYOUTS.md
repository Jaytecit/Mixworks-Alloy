# Named workspace layouts

The P14 increment in0.55.32 adds **View → Workspace layouts...** and **View → Arrange console tabs...**. HANDOVER records the accepted candidate and exact verification; implementation alone is not a staging claim.

Save current creates a new named local layout. Restore chooses a saved layout. Names are1–64 characters; duplicate names and existing files are kept. The first-use list explains that no layouts have been saved and disables Restore. Records live beside the local workspace preferences in `workspace-layouts/`, with generated filenames and the `.alloyworkspace` extension. The manager lists up to128 valid records; unsupported files remain on disk.

Layouts capture panel widths/collapse, lane proportions/timeline height, grid/visual preferences, current console/Library/Playlist/inspector views, zoom/scroll, and docking/window positions for Controller, Transitions and Editing. Arrange console tabs accepts every permutation of those three consoles. Their commands and MIDI identities keep their existing numeric owners. Console order also persists in ordinary workspace preferences, with the original order as the older-file default.

From 0.55.42, both side panels collapse independently. Drag a divider outward or focus it and press End. Double-click, Enter or Home restores its prior expanded width. Both can stay collapsed together; the dividers remain available. Library/Playlist controls hide with the left panel, and workspace preferences/named layouts retain the collapse state. Older records open the library expanded.

Restore preserves the arrangement, musical selection, Undo/Redo/dirty state, playback position/running state, monitoring mute and MIDI assignments. Saved offscreen console windows are constrained to a currently available display. An unavailable docked page falls back to an attached console; independently detached windows remain available through View. Follow playhead keeps its normal behavior, so an enabled Follow can adjust a saved view to the current cursor.

Named records reuse the bounded `mixworks-workspace` version1 codec with a `namedLayoutVersion` extension. They exclude monitoring mute and the default save folder. Restoring a layout preserves the current save-folder preference. Unknown/malformed/future data cannot replace the current workspace, and saves create fresh files. Named layouts are restored explicitly; they are not automatically opened at startup. Existing local workspace startup behavior remains in force. Free-form relocation of every application pane is still outside this increment.

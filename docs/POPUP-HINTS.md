# Popup hints

Hover over a control for about a second to see its action and result. **View > Popup hints** turns help on or off across the main window and detached app panels. The choice is saved in workspace preferences; older workspaces start with hints enabled.

Help covers the toolbar and transport, library and playlist, lane controls, Editing console, effects and keyboard controls, live desk and MIDI settings, transitions and rehearsal, master panel and source-grid review. Menu commands also offer hover help. Existing key, controller-state and source-file details remain available.

The timeline explains the object under the pointer: segment movement/trimming, source slip, transition frames and anchors, paired swaps, automation lines and editable nodes. Disabled controls show a specific recovery action where available, without appending generic possible causes.

Hints appear beside the pointer, with room around the hovered node. They dismiss when mouse editing or scrolling begins, do not take keyboard focus, and do not animate. Turning hints off dismisses any visible popup immediately. Motion settings, musical Undo and the arrangement are independent of this workspace choice.

Third-party plugin editor contents retain their own help. Their controls and operating-system dialogs are outside Mixworks' hint inventory. To understand the app's plugin chain, hover over its Open, Bypass, Remove and MIDI controls buttons.

Native accessibility now includes Library/Playlist row and individual cell names, musical metadata and cell/table relationships. Footer queries expose current feedback, filename, unsaved state and selected owner; native title-change events are coalesced. This supplements, and does not establish, a complete Windows screen-reader journey; musical-object access remains in progress.

## Plain interface text — 0.55.46

Persistent text names the control or reports a short result, such as **Downbeat saved**. Instructions belong in popup hints and these guides. Errors retain the problem and the action needed to continue. Destructive choices still name what they replace. Original filenames, plugin names and user labels remain intact.

Timeline curve labels use at most three characters: **VOL**, **BAS**, **MID**, **TRE**, **FLT**, **PAN** and **FX**. Selected-node badges show the short name; hover hints and Editing retain precise values and times. Numerical controls keep their units. IN/OUT, section and hotcue markers keep their existing short identities. This changes presentation, not the saved curves or sound.

**LOP** marks a loop start. A transition frame's number is its rounded length in bars; its hint and editor retain the precise length.

Editing groups automation commands above segment tools at compact widths and beside them at wide widths. Drag the divider above the console upward to expand it. Fully expanded docked Editing fits its controls without scrolling at the tested 1250×720, 1250×800 and 1920×1080 sizes. A smaller dock or detached window may still scroll so controls remain reachable.

**Save downbeat** stores a reliable beat one anywhere in the original track; it need not be the earliest downbeat. It replaces the stored downbeat, preserves BPM/key corrections and archives each changed reviewed choice separately from the detection. An archived choice alone does not prove a source-save succeeded. Prepared audio retains its current clock until Beatmatch or **Save + update mix**. IN/OUT, section flags and hotcues remain separate. See [downbeat references](DOWNBEAT-REFERENCES.md) and [range editing](RANGE-EDITING.md).

In track timing review, blank BPM/beat-one fields use the available detected values. Tap three or more steady beats, then check the beginning, middle and end. **Save source only** keeps current placements; **Apply to arrangement** saves corrections and rebuilds the timeline, which can replace transition placement. Grid click accents beat one only when a downbeat is known. Detector diagnostics are available through **Analysis details**.

The 25 October clarity audit limits app-owned hover help to two sentences,35 words and240 characters. Routine results use the status area, with longer reports available explicitly. See [messages and retry review](UI-MESSAGES-AND-RETRY-REVIEW.md) for per-track notifications and the Source corrections failure list.

Effects follow the selected segment, or the lane when no segment is selected. The master spectrum measures processed audio before monitor mute. EQ, width and saturation are saved settings; master gain keeps its automation. dBFS measures signal level, not perceived loudness. These explanations are available in the associated popup hints.

## Searchable keyboard reference

The right inspector has **VST** and **Shortcuts** tabs. Shortcuts opens an everyday arrangement list; **All** shows the complete application keyboard reference, including commands whose meaning depends on the focused control. Search always covers the full reference by command, key or context, even in Everyday mode. Enter in search focuses the results; Escape clears search and returns focus to the timeline. The list is reference text, not a command launcher. Master Output remains visible, and switching tabs preserves the VST/MIDI view.

After a successful segment, range or group paste, the playhead moves to the latest pasted end. Repeating Paste at that cursor uses the exact end, including non-grid ends; the first paste still follows Snap. Copy/Cut begins a new paste sequence. Failed pastes leave the cursor alone.

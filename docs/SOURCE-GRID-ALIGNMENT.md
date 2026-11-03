# Manual source grid alignment

Use **Source beat grid / corrections** for constant-tempo 4/4 sources. Changes
remain a draft until saved; the original recording is never edited.

The first screen is **Simple: A / B / C**. Its white/copper centre marker stays
fixed while you drag the audio beneath it. Dragging navigates the source and
pauses audition; it never moves an existing mark or edits the grid. **Shift**
gives one-tenth movement; left/right arrows move the selection by 1 ms. A click
centres the clicked audio, and **Space** plays/pauses from the selected position.

1. Find a clear musical ONE under the centre marker, then **Set A**. Any reliable
   downbeat in the track works. Set pauses playback and captures its current clock.
2. Navigate to a later ONE and **Set B**. **Bars A to B** is the number of complete
   4/4 bars between the marks: 16 means B starts bar 17 relative to A. The app
   suggests a count from the current tempo; check/correct it by counting the music.
   A and B calculate one constant tempo, with A pinned. A longer interval improves
   precision. No detected tempo is required if you supply the count.
3. Navigate further and **Set C**. Check **Bars B to C** too. C independently checks
   the A/B grid; the readout shows both interval tempos and C's offset in ms. It
   does not bend the grid to hide a mismatch. Confirm both bar counts explicitly.
4. Use **Hear / go to A/B/C**, Beginning/Middle/End, zoom and **Grid click** to check
   other parts of the track. Simple Save is enabled only with confirmed counts,
   all three marks in order and C within 30 ms of the A/B grid. This tolerance is
   a practical check, not proof of musical accuracy. A consistently wrong bar
   count can still agree: count the bars and listen before saving.

Pressing Set again replaces that point; replacing A or B clears later marks.
Mark edits, counts and confirmation have Undo/Redo. Navigation creates no grid
Undo entries; **Escape** cancels a drag. Marks are editor-only measurement aids;
the existing saved correction stores the calculated BPM and downbeat, not an
editable three-point tempo map. Opening a saved correction displays its grid and
starts a fresh measurement. Source bytes, analysis and saved formats are unchanged.

**More tools / Advanced** opens the secondary exact controls and exposes the
Tempo, Beat one and Check drift pages. These share the same unsaved correction.
Advanced timing edits that no longer match the marks invalidate the Simple result.

Zoom ranges from **120 seconds** down to **250 ms**, **50 ms** and **10 ms**. Zoomed
waveforms resolve actual source samples; wait for **Loading waveform detail...**
to finish before judging alignment. No new analysis or rescan is needed for this
display repair. Opening an existing correction retains its timing precision.

Orange lines mark beat one and bar boundaries; blue lines mark the other beats.
Labels such as **3.1**, **3.2**, **3.3**, **3.4** count bars relative to the chosen
beat-one anchor. The footer distinguishes detected, saved and draft timing, or
pulse-only timing without a bar anchor. Blue-to-violet waveform colour shows
sound density/energy; it does not identify instruments or confirm musical beat one.

The secondary guided pages show one task at a time. You can switch pages freely without
changing the draft. **Advanced** contains exact seconds, A/B modes, fine BPM steps,
key corrections, retry results and analysis details.

1. **Tempo:** play a steady section. Keep the detected BPM if the clicks keep time.
   Otherwise tap along or enter a known BPM. Half/Double fixes a tempo counted at
   twice or half the musical beat rate. Tapping gives a starting estimate.
2. **Beat one:** count ONE, two, three, four with the music. Click a clear ONE in
   the waveform and choose **This is beat 1**. Any reliable bar works, including
   one in the middle of the track. This aligns the pulse and bar origin together.
   If every click is equally early/late, drag the ruler or use Earlier/Later.
   If the clicks align but the bar accent is wrong, use **Beat 1 back/forward**:
   these move only the four-beat grouping, preserving the pulse and BPM.
3. **Check drift:** count a known number of complete bars from your marked beat
   one. Choose that count, then **Go to check beat**. Click the actual later ONE
   and **Match check beat here**, or drag its handle. The first mark stays pinned
   while BPM is calculated. Sixteen bars from8s to38s mean128 BPM. Do not blindly
   choose the nearest peak: the chosen bar count must be right.
4. Listen with **Grid click** at Beginning, Middle and End. A growing offset means
   tempo/spacing needs correction; the same offset everywhere means move the grid.
   If only a middle passage drifts, reconsider the marks or whether tempo is steady.
   The tool does not create a variable-tempo map, and waveform peaks do not prove
   beat one. All changes are draft edits with Undo/Redo until saved.

**Save for this track** stores the correction while leaving the current mix's
placement and captured playback clock unchanged. **Save + rebuild mix...** asks
for confirmation before saving and rebuilding Beatmatch across the whole mix.
That can move tracks and replace manual transition placement. Undo restores the
mix, while saved source corrections stay saved. Rebuild is unavailable for a
source not in the mix or while transitions are locked. Cancel keeps the draft
when declining rebuild; the editor's Cancel discards unsaved corrections.

## Advanced dragging and precision

- On the secondary pages, dragging the waveform changes grid phase at fixed BPM,
  rather than navigating as it does on the Simple screen. Hold Shift for fine
  adjustment. A click seeks source audition; Escape cancels a drag.
- **Move grid**: drag A or the waveform ruler to slide the whole grid at fixed BPM.
- **Adjust spacing**: drag B around pinned A. Dragging another later downbeat in the
  ruler selects that bar as B; its identity appears in the bar-count readout.
- After B has been aligned, changing **Bars A to B** keeps its measured source time
  and recalculates BPM. Check this count carefully to avoid half/double-time errors.
- Choose **1 ms steps** or **10 ms steps**. **Earlier / Later** moves the whole grid;
  **Narrower / Wider** adjusts spacing at B. Left/right arrows follow the selected
  editing mode. Text fields retain their normal keyboard editing.
- For exact positioning, enter A's source seconds directly. For B, enter the source
  cursor time and press **Set B at cursor** with the intended bar count selected.
  This also works with no detected BPM: set A, choose the counted bars, then set B.
- **Go to A / Go to B** and the zoom selector allow close inspection of either
  handle. Click the waveform below the ruler to seek the independent source audition.
- **Undo grid / Redo grid** includes one entry per completed drag. Escape cancels
  an active drag. **Revert grid** restores the timing present when the editor opened;
  **Detected grid** restores detected timing. Both retain key corrections and are
  undoable. **Cancel** discards all unsaved corrections.

B must remain after A, inside the source, with a calculated BPM of 20–400. Invalid
positions and bar recounts retain the previous grid and explain how to recover.
Longer measured intervals reduce the effect of small positioning errors. A kick
or waveform peak alone does not identify beat one; listen to the musical phrasing.

## Verification scope

The focused native entry is `--grid-alignment-only`, run through
`scripts/verification.py:run_checks`. It exercises the real editor handlers with
small authored PCM, calculated-BPM oracles, bounds, draft Undo/Redo, precision,
source-record save/reopen and screenshots. It does not open a physical audio
device, run a detector or access production data. These cases also run within
the existing `--grid-correction-only` suite. Native event injection is not physical
mouse/keyboard or musical listening acceptance.

`--waveform-alignment-only` separately checks streamed original-file detail against
authored sample positions, silence and native source/timeline pixels, including
streaming block boundaries. The ua16 checkpoint passes36 waveform and76 grid-tool
checks on one candidate, with12 native captures reviewed. Physical gestures and
independent real-music timing/listening remain open; see the
[checkpoint](release-audit/UA16-WAVEFORM-GRID-CONTROLS-20261101.md).

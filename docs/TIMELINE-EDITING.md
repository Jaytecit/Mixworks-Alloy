# Timeline editing — v0.27.0

## Automatic tempo spacing (0.55.56)

New automatic BPM changes use at least **16 bars (64 beats in 4/4)** between tempo targets, independently of the crossfade length. A target can finish after the incoming IN point so a short transition does not force a sudden speed change. Track placement, source spans and beat alignment are retained. This applies to new stopped additions, Auto Arrange/rebuilding and future ramps for live additions.

If the incoming audio cannot accommodate that spacing, or an existing later marker prevents it, the track follows the existing tempo map without adding a new BPM target. Live additions also preserve the clock through all established audio. Same-BPM ownership markers may remain close together because they introduce no BPM change. Deliberate Cut transitions retain their immediate tempo switch.

Existing saved maps and manually positioned markers are not automatically spread or rewritten. New defaults take effect when adding tracks or explicitly rebuilding the arrangement. Independent tempo editing remains available for a deliberately faster change.


**0.55.43 range/group increment:** see [range editing](RANGE-EDITING.md) for the Range tool, exact intervals, Ctrl-click selection and scoped Copy/Cut/Delete/Paste. The historical sections below retain their original version context. Current behavior uses ordinary preset nodes, no invented empty-curve endpoints and1600px/s maximum zoom; [preset nodes](PRESET-NODES.md) and the handover supersede older procedural/800px/s descriptions.

**0.55.39 update:** presets place ordinary editable nodes, including the swap itself and Filter sweeps. Older generated-treatment descriptions below document compatibility behavior. See [preset nodes](PRESET-NODES.md) for replacement scope, controls and older-session conversion.

## Maximum zoom (0.55.40)

Maximum timeline zoom is1600 pixels/second. Zoom around the pointer or view centre; use the overview edges to resize the view, or drag its body to pan. Named workspace layouts retain this zoom. Musical data and timing are unchanged.

## Automatic shared-section transitions (0.55.22)

Choose **Sections 8-64+ bars** and the new-track **Bass swap** preset before adding tracks. Placement keeps the identities of the outgoing/incoming source changes and maps those changes to the same musical time. The automatic frame uses complete bars inside both segments. Bass swap occurs at a shared section boundary within that frame; it can be away from 50%. Applying Bass swap after an automatic Crossfade uses the same shared change without repositioning either track. Auto Arrange's generated bass treatment uses this shared-boundary rule too.

The usual 64-bar preference (32 bars for Auto Arrange) can extend when a genuine paired match needs a longer overlap and both sources contain it. Fixed lengths remain available. Missing or one-sided section evidence retains the existing bounded grid fallback; it does not establish a shared section. Sections come from retained source energy: four-bar changes plus strongly corroborated two-bar breaks/returns, with spacing suppression. These are structural suggestions, not verse/chorus/vocal identification; equal-energy musical changes may be missed. No track names, specific source bars or grey four-bar shading are used as musical evidence.

Track movement, manual alignment, transition locks, Undo and saved geometry retain their existing owners. Source trimming, audio slip and captured tempo clocks are included when mapping a measured section to the timeline. Analysis revision is **11**; session format remains **30**.

## Preserve manual alignment when moving (0.55.21)

With Snap enabled, moving an existing segment quantises its **displacement** by the selected beat subdivision. The current placement, including a manual timing correction, stays the reference. Segment-body, title-handle/following-track, transition-frame/edge, I/O song and IN/OUT anchor moves use that rule. Moving a corrected song no longer reapplies its detector phase or attracts it to a partner's old pulse positions. Locks, valid transition bounds and Undo still apply.

Hold **Alt** or disable Snap to change the alignment itself, including a fractional-beat pre-roll correction. Subsequent snapped moves retain that correction. New insertion/paste still establishes placement from the source grid. Shift audio-only slip remains a separate alignment edit.

## Below-waveform curve tools (0.55.16)

**New curve endpoints (0.55.17):** a newly revealed segment parameter line has neutral nodes at the exact start and end of the visible segment. These are available immediately, including for Filter and VST parameter curves; selecting an empty line alone still leaves the project unchanged. The first actual node edit saves the handles through normal Undo. Creating a line with an Add Marker or precise-entry command saves both segment-edge nodes alongside the requested point in the same Undo step. Edges use the segment's source-elapsed clock, so trimming, rate changes, tempo mapping and audio slip do not confuse them with project zero or the original file's end. Existing saved curves retain their original nodes and shape. Generated fade/swap adjustment handles inside the segment remain available alongside its edge handles.

Select a segment to reveal **VOL / BASS / MID / TREBLE / FILTER / FX...** beneath its waveform. The strip occupies reserved space inside that lane, clear of the waveform and every track below it. A filled coloured dot means generated or saved automation exists; a hollow dot means there are no nodes yet. The pressed button identifies the selected curve. Selection reveals that exact segment's curve without adding points, changing the sound, seeking, adding Undo or changing automation recording mode. Existing line clicks, node edits, the unity stop and the Transitions inspector remain available. A selected band is revealed if the inspector had hidden it.

Narrow segments use a native parameter dropdown. Scrolling keeps the tools within the visible part of their segment; segments too narrow for a readable selector and lanes shorter than 96 logical pixels hide the strip rather than covering other content. All normal-height lanes reserve the same 28 logical pixels, so changing selection never moves waveforms. **FX...** opens the existing VST3 parameter editor; showing a parameter is view-only, including during an active take. Its first admitted edit creates saved automation with one Undo. It does not add a built-in effect.

The former on-waveform **INCOMING / OUTGOING** song panels are now compact **I / O** drag handles in the same lower area. **I** moves that segment as the incoming song within its entering transition; **O** moves it as the outgoing song within its leaving transition. Middle songs can have both. Hover for the full action. They retain the fixed-window song movement, snapping/Alt bypass, locks and one Undo per gesture. Starting an I/O drag does not jump the playhead or scroll to the transition. The segment-header whole-arrangement handle, Shift audio slip, IN/OUT alignment markers and transition-frame edges remain separate controls. The lower Transitions console is retained.

## Beatmatch corrections and fine snapping (0.55.14)

Right-click a **Library** or **Playlist** track for its Beatmatch menu. **Track BPM / key...** reviews the clicked source, including in sorted tables. The same commands remain in the top Beatmatch menu.

From 0.55.42, clicking a Library **BPM** or **Key** header captures the highlighted library source as the sort reference. Closest BPM differences or Camelot-wheel relationships appear first; exact keys lead, with adjacent/relative keys nearby. The reference stays first and unknown values stay last. Reversing the header orders the other distances in reverse. Another header click captures the then-highlighted source; selecting a different row alone does not reorder the table. Missing/unknown references use normal column order. This does not change the timeline selection, source analysis or saved mix.

Timeline, automation-node, Library/Playlist and tempo RMB menus open at the cursor, constrained by the display edges.

Enter an exact constant source BPM or use **-0.01 BPM / +0.01 BPM** to correct steady drift. Check the beginning, middle and end. BPM changes beat spacing; **Beat 1 here** and **-1 ms / +1 ms** set and shift the downbeat anchor. Shift makes the phase nudge 10 ms. Changes remain drafts until saved; Cancel discards them. The effective tempo is shown even when the override is blank and detection is in use.

- **Save source only** stores BPM, downbeat and key corrections. Existing timeline audio keeps its captured playback clock and placement.
- **Save + rebuild timeline** also runs Beatmatch across the mix using the current transition preset and length. This applies corrections to prepared playback and the timeline, and can move tracks and replace manual transition placement. Unlock all transitions first. Rebuilding is undoable; saved source corrections remain separate from musical Undo.

A failed source-record write leaves the draft open, shows the error and restores the previous source correction. Timeline rebuilding starts only after that record is saved successfully.

**Beatmatch / Snap grid** selects **Beat**, **Half beat**, or **Quarter beat**: 1, 2 or 4 divisions per beat. A quarter beat is a sixteenth note in 4/4. This sets timeline guides and snapping for segment movement, trim/automation/label edits, transition anchors and audio slips. Fine guides appear when at least 8 pixels apart and also cross the waveform. Whole-beat and bar accents remain. The Snap switch enables snapping; Alt bypasses it for supported drags. Musical transition lengths retain their selected beat/bar counts.

The subdivision is undoable and saved with the mix. New mixes use session format **28**; older mixes default to whole-beat snapping. Maximum timeline zoom is **800 pixels per second**, twice the previous maximum. Analysis revision **9** and audio-cache compatibility remain; no detector reset is required.

## Editing console (0.55.11)

Select a playlist row or timeline segment, then open the lower **Editing** tab. **Start**, **Source in**, **Length**, **Fade in**, **Fade out**, **Gain**, **Reverse** and **Loop...** act on the same selected segment as before. Segment Settings in the timeline menu and **View > Editing console** also open it. Musical Undo, transition locks and source-preparation restrictions still apply. No selection leaves the controls disabled.

Drag the divider above the lower consoles to give Editing more height. Smaller panels scroll; the controls wrap into rows. Use the hexagonal detach icon or **View > Detach Editing** to move the same console to a resizable native window. Its tab focuses the detached window. The attach icon or native close button returns it to the lower area. Controller and Transitions can detach independently; when all three are detached the timeline can use the extra height.

The selected lower tab and Editing's docked/detached state, window size and position are saved as workspace preferences, outside musical Undo and saved mixes. Restoring a window onto a disconnected display constrains it to an available screen. Older workspaces open with the existing Controller default. Invalid preference fields leave the file and active layout unchanged. Broader named/rearrangeable workspaces remain planned in P14.

VST3 instruments/effects stay on the right. **Keyboard** reveals the piano below the chain, so the inserts remain accessible while playing the selected lane. **Audio / MIDI...** beside it opens the existing device/controller setup; the top toolbar shortcut remains available. The old right-side Segment/MIDI tabs are removed. Plugin editor contents remain plugin-owned.


## Independent tracks and explicit transitions (0.55.59)

Ordinary additions, timeline/file drops and splits create independent segments. Drops use the chosen time and lane, with beat snapping. Moving a segment or its IN/OUT markers never creates a relationship. All four lanes may overlap freely.

Use **Add Transition** in the Transitions console or timeline menu. Ctrl-select two overlapping tracks, or select one and choose its overlapping partner. The earlier track is outgoing; lane order breaks equal-start ties. The existing preset/length defaults apply without moving either track, in one Undo. Auto Arrange and **Auto Arrange timeline** also create transitions. Ordinary insertion before/after a track creates no joins.

Frames now store source-relative edges independently of fades. Drag either edge or the entire frame anywhere inside both tracks' audio. Neighbouring frames may overlap completely, with no bar-count ceiling or half-track limit. The Length field accepts a typed positive bar count. Existing preset, band, filter, lock, navigation, swap and rehearsal controls remain.

**Mute outside bounds** is saved per transition and defaults off. On silences the incoming head before the frame and the outgoing tail at/after its end. Off retains exterior audio subject to the track's own fades, automation and source boundaries. The toggle is undoable and respects locks. Session format 39 stores these choices; analysis revision 11 is unchanged.

## Audio outside transition windows

When **Mute outside bounds** is enabled, the incoming track is silent **before the window starts**, and the outgoing track is silent **at and after the window ends**. The waveform dims in excluded regions. With the toggle off, the frame itself excludes no audio. Material between transitions stays audible; the first track keeps its lead-in and the final track keeps its ending.

These cuts apply independently of volume/EQ mode, fade strength or manual automation. Moving or resizing the frame updates them immediately. Audio slip moves the source inside the same bounds. Segment insert and EQ output also stop at the cutoff; shared lane/master effects retain their usual behavior. Playback, WAV export and A/B/C rehearsal use the same boundaries. Segment lengths, source files and automation positions stay intact. Removing a transition link restores the segment's own fade/source boundaries.

## Move a track and following tracks (0.55.12)

Drag the hexagonal left/right handle at the right of a segment header to move the **whole selected box and the following arrangement**. Earlier playlist tracks stay unchanged. The incoming transition keeps its exact timeline window, including its incoming fade and paired bass endpoints; outgoing and later linked transitions travel with their tracks. Unlinked later tracks move by the same musical displacement, preserving their gaps/grid relationship through the project tempo map. The operation uses the document's arrangement order; sorting a playlist column does not rewrite that order.

Snap moves by the selected beat subdivision while retaining the current alignment; hold **Alt** for a free move. One drag creates one Undo step. Repeated pointer updates retain live control changes. Segment-owned treatments travel with their source; session labels, tempo markers and lane/master automation retain their absolute timeline ownership. The fixed incoming window must remain inside the selected audio: moving its box past that window is rejected, with a status explanation. Pending/unavailable sources, protected transitions or links crossing back into the earlier playlist reject the whole move. The first track has no incoming window to pin. Ordinary segment-body and transition-frame dragging retain their existing meanings.

## Independent audio slip

Hold **Shift when starting a drag** on the same hexagonal handle to slide only the audio/waveform, leaving the segment boundaries, automation points, fades, IN/OUT alignment markers, tempo map, paired tracks and transition frames fixed. A complete drag has one Undo. Snap aligns an actual source beat with the mix pulse; hold Alt for exact free movement, including exposing source silence at an edge. Double-click the handle to reset its audio-only offset. Existing saved slip offsets remain intact; the new whole-box move does not automatically unwind earlier slips. Unlock protected tracks before editing. Unavailable audio at exposed edges is silent. Playback, waveform source ticks, splitting, Save/Open and WAV export share the saved audio clock (format 22). Normal segment-body movement and paired transition handles keep their existing behavior.

The user’s screenshots guide the energy overview, four-bar divisions, orange intro/outro markers and right-click menu. The current requirement is explicit: IN/OUT align consecutive tracks and move them on the timeline, while fades and other controls remain separate.

## Behavior and ownership

- Energy view uses 2.5 × cached mean-square amplitude, capped at the display height. At close zoom it shows RMS and transient peaks. The scale is fixed, independent of the visible passage; audio and analysis caches are unchanged. Detail and peak-only views remain available.
- Strong guides and alternating bands count 16 beats on the shared tempo map. Finer subdivisions yield to the waveform when zoomed out. These are four-bar counting guides, not detected musical phrases.
- `Clip` owns optional source-elapsed intro/outro alignment markers and converts them to local timeline times. Omitted markers sit at the segment edges, independent of fades. Each marker can move across the whole track.
- `Project::moveTransitionAnchor` uses chronological segment order, independent of playlist insertion order. OUT aligns the next IN to its new position and reflows each later track. IN selects a different source position, slides its track under the previous OUT and reflows later tracks. The first IN and last OUT remain unpaired. `Clip::alignIntroTo` owns source/tempo conversion and uses the existing non-destructive `moveTo`/`retime` path. Tempo points, automation and recording mode do not change; source spans and source fade spans survive reflow. Moves requiring a negative track start fail atomically. Snap/Alt and one Undo entry per drag remain supported.
- Playback and the displayed gain envelope use separate fade positions. `Clip::slice` preserves off-segment fade positions so splits keep the original audible fade. Session version 10 stores alignment and fade positions separately and reads versions 1–9. Older marker values are interpreted as alignment guides; malformed positions are rejected.
- The right-click menu and shortcuts share command handlers. Cut/copy/paste operate on whole segments; marker actions use existing automation lanes. Effects require an automatable VST. Loop start/end creates two repetitions of the selected source range, undoable in one step. “Tweaks” opens the app's BPM/key/downbeat editor; the full MixMeister tweak preset library is not implemented.

## Verification

Current production-component checks cover linked IN/OUT gestures, three-track movement, chronological adjacency, repair of previously unaligned markers, zero-boundary rejection, retained source/fade spans and tempo controls, sample-identical mixer PCM with markers shown or cleared on the moved arrangement, Snap/Alt, Undo/Redo, recording-mode retention, session validation and split fades. Broader placement, tempo and core tests protect existing editing behavior.

The following verification records describe earlier releases and their former linked-transition behavior; they do not establish the v0.26 alignment-only semantics.

Real inputs are **Better Off Alone** and **Ode To Omar**. Nine scenarios retain fixed-tempo, ramp, drop, tempo-edit and movement checks, and add intro/outro edits. `.local/real-tracks-v022/` contains 93 passing checks, real UI snapshots, listening mixes, isolated stems, source/sidecar digest audits and independent envelope timing measurements. Both real source pulses and bar phases remain aligned after anchor edits. Original audio and sidecars are unchanged. Downbeats remain 0.540735 s / 0.053062 s; the prior reference limitations still apply.

`Dance To The Beat` from the supplied waveform screenshot is also rendered through the production UI in energy, detail, peak and whole-track views. This is a visual comparison using local audio, not an assertion of pixel-identical MixMeister rendering.

Final focused anchor verification: **40 checks**, `.local/real-anchors-v022-release/`, unchanged source/sidecar digests. Independent bass-envelope timing comparisons accepted all 520/684 examined windows: OUT p95 **4/3 ms**, maxima **8/6 ms**; IN p95 **4/4 ms**, maxima **6/8 ms**. This measures transfer timing, not musical downbeat ground truth. The nine-scenario run still shows the pre-existing fixed-tempo maximum of **46 ms**.

Regression checks: **454** existing checks in `.local/verify-rhythm/v022-release/` and **29** new editing checks in `.local/v022-packaged-editing/`, all passing. Same-song waveform verification has **28** checks in `.local/v022-dance-release/`. The final packaging-only change lets the timeline receive keyboard focus; editing checks were rerun after it. The deployed executable is `out/releases/0.22.0-2be3aa095a5c/Mixworks.exe`, SHA-256 `2BE3AA095A5C882638E95F611AE605BA9409F841C0E56BFDB9411F0EDCCC68C5`; real audio runs retain their exact build identities in `run.json`. Earlier releases and launcher backups remain available. Existing unsaved sessions were left open.

No human listening or physical audio-device pass is claimed. Existing fixed-tempo stretch outliers and broader MixMeister detection/stretch parity remain separate work. Automated mix selection remains deferred.


## v0.24 interaction and live insertion corrections

Short overlapping drops now put the outgoing OUT at the incoming IN, rather than calculating it backwards from a longer outgoing file. Append uses the chosen audible end. Anchor hit testing includes both edge triangles and the exact right edge, with priority over automation. Hover cursors identify draggable segments, anchors, automation points, labels and tempo pins.

RMB marker actions retain the waveform, curve selection, recording mode and playhead. The automation dropdown was removed. Existing curves overlay the waveforms; click/drag a point to select its controls and edit its time/value. Ordinary segment/anchor gestures remain available. The tempo strip attaches below the four channel lanes, capped to the available window height. Fullscreen starts with a hidden title bar; F11 restores a window and File → Exit/Alt+F4 retains the close confirmation.

Live additions use the existing tempo map and immediate elastic mappings for incoming tracks. Existing prepared PCM/elastic identities, playhead and plugin DSP are retained. Pending/failed segments never render raw, unmatched audio. Background hydration, failed input, subsequent additions and Undo no longer stop the running transport; Pause remains enabled. Live drops into the past join at the next aligned beat/bar. Idle arrangement still prepares native-tempo ramps. Explicit tempo edits retain their existing preparation workflow.

Verification: **399 checks, zero failures**, `.local/v024-release/` (core 279 with test VST3/Vital; editing 44; placement 42; Alloy 34). `.local/v024-red/` records the three original failures. Includes sample-identical uninterrupted callback PCM, lazy loading/failure continuity, upcoming live placement, short overlaps, insertion-order-independent cue links, uncertain neighbours and actual marker/segment/cursor gestures. Release build has no C++/linker warnings or errors. Fullscreen/footer native appearance verified; human input interrupted the native second-track playback check, so no fresh listening or completed real-device live-insertion pass is claimed.

## Pair bass swap from the right-click menu

Right-click the receiving segment and choose **Pair bass swap** to complement the nearest full manual cut on its existing transition partner. The original cut stays in place; the new pair uses the existing linked handles and one Undo. Ambiguous or conflicting cuts are explained without replacing existing work. See [pairing rules and handles](SWAP-HANDLES.md#pair-an-existing-bass-cut-052).


## Suggested energy sections

Alloy can show numbered source-section strips such as **S1** beneath the waveform. These are optional energy-change suggestions derived from saved waveform RMS: four bars before/after, at least 3 dB contrast and eight bars between accepted boundaries. They use the existing source downbeat phase and follow the waveform through source trimming, tempo mapping and audio slip. The display toggle itself does not adjust alignment, alter analysis or write new metadata. The **Sections 8–64 bars** insertion default uses these same suggestions when placing new tracks, preferring a shared boundary on both songs. The hints do not identify verse/chorus or guarantee phrase accuracy. Sections with similar loudness may be missed.

Use **View / Suggested energy sections** to hide/show them for this app run. Missing/incomplete RMS, unknown downbeats, variable-tempo sources without a manual constant grid, reverse and loops remain unmarked. No rescan or cache reset is required; original analysis, corrections, mixes and controller mappings are retained.


## Selected automation node level

Click an automation line to reveal its saved nodes, then click or drag a node to show its value. Release keeps it selected. Selecting another line transfers the revealed nodes; clicking a segment or the background ends node selection. Other visible curves remain drawn. Gain/EQ percentages use linear amplitude: unity is 100%, silence is 0%, and a boost may exceed 100%. Cutoff values show LP/HP frequency or Off; pan and VST parameters use their normalised values. Precise gain/EQ entry uses 0–130%; cutoff retains LP/HP frequency or Off, and other parameters retain their existing units. Selection is a view choice and adds no musical Undo entry.


## Automation in Editing (0.55.24)

Open **Editing**, then choose an **owner** (one placed segment, a mixer lane covering all its segments, or master output) and **parameter**. Volume, Bass, Mid, Treble, Pan, manual Filter and named effect parameters share this area. The same parameter list lets you choose between coincident lines without guessing which line receives a click. Segment numbers follow playlist order; duplicate songs remain separate owners. Master exposes its volume curve. Existing footer buttons and direct line clicks remain shortcuts.

Use **Add at playhead** to insert on that exact curve, with the current Snap setting. On a segment, right-click for **Add Volume/Bass/Mid/Treble/Filter/FX Node Here**; V/B/M/T/F/E inserts at the playhead. The Automation submenu's precise node editor seeds at the context position (or playhead when opened from Mix). FX insertion reuses the selected insert and parameter; choosing or showing FX alone does not allocate a curve, change sound or add Undo. An empty segment parameter displays neutral adjustment handles; the first actual edit saves them. Insertion outside the selected segment is refused with an explanation.

**< Node / Node >** selects adjacent nodes without seeking. **Time / value...** edits the selected node or seeds a new one at the playhead; exact time bypasses Snap. Feedback identifies project bar/beat and signed seconds relative to bar 1. Filter uses LP/HP frequency or Off; FX also exposes the plugin's value text in node details/help. **Delete node**, Delete with Editing focused, and a node's right-click **Delete manual node** command remove only saved manual points, with Undo. Right-click no longer deletes immediately. Generated handles need an actual edit before they become saved nodes; clearing manual nodes leaves the generated treatment in place.

The state row distinguishes visibility, Read/manual override, generated treatment and saved node count. **Read curve** returns to saved manual automation after an override or disables its playback while retaining the points. **Read mode / Touch / Latch** controls recording separately. The Arm button identifies the selected owner's lane or master; its help lists all armed lanes and master state. Controller Record still arms all four lanes and master. Touch returns after 0.5 seconds without control input. Browsing and cancelling exact entry leave recording intact. Finish an active take explicitly with **Read mode** or Stop before editing nodes, so edits cannot discard captured writes. Busy/preparing, export, rehearsal and transition locks also block node edits.

**Paired filter...** opens the associated transition's real stage controls. It prefers the selected transition when a segment has an incoming and outgoing relationship. Those filters are distinct from segment/lane manual Filter nodes; this route does not convert or replace them. The live desk cutoff knob remains lane-owned. Editing moves with its existing detachable console; compact layouts retain scrolling. These controls do not change the detector or saved session format.

## Direct curve selection (0.55.5)

The transition header keeps its four-step counter with the bar count immediately beside it. The selector strip and TRANSITION/LOCKED wording are removed; the lock badge remains. Click the drawn automation line to select its exact track/segment/parameter and reveal that line's nodes. Other lines stay visible according to the existing Transitions inspector's Show/Only settings. Line clicks do not create nodes or alter saved automation. Generated treatments retain their existing paired handles and inspector controls. Narrow/scrolled views retain direct line selection and a compact header.

Nodes have a 12-pixel grab radius, with priority for the selected curve. After selecting a node, Left/Right chooses adjacent nodes, Enter opens precise entry, and Delete removes that node with one Undo. Locked transitions permit curve selection but reject node changes; export also blocks editing. The selected value uses a whole-number percentage (cutoff uses LP/HP frequency or Off) in an opaque caption placed clear of dots, handles and the frame controls. Extremely occupied views retain the value in the status bar when no clear caption position fits. Precise entry still uses physical units.


Cutoff precise entry in 0.55.6 accepts `LP 800 Hz`, `HP 1.2k` or `Off`. Centre is dry; left removes highs and right removes bass. See [DJ cutoff](LIVE-DESK.md#dj-cutoff-filter-0556). Older low-pass curves retain frequency-domain linear interpolation on migration.


## Transition automation repair (0.55.15)

Default volume fades, generated volume/EQ treatments and saved adjustment points now share the selectable automation path. Clicking a generated line reveals neutral adjustment handles without changing the project, dirty state or Undo. The first real handle edit saves ordinary source-owned adjustment points. These shape the existing treatment; frame controls and paired swap handles retain the generated treatment's timing. Saved points remain editable and deletable; an untouched generated handle is not a saved point to delete.

For gain/EQ nodes, the waveform bottom represents the existing −60 dB floor, displayed as 0%. Unity (100%) sits below the transition title bar, with space above it up to 130%. Dragging up from below unity stops at exactly 100% for that drag; release and start another drag to boost. Returning below unity rearms the stop. Precise entry accepts 0–130%. Existing saved values are retained until edited. Nodes paint after the frame and song handles so they stay visible.

Mapped MIDI volume, master and Bass/Mid/Treble controls use the same unity stop and 130% ceiling, including routed desk pots. A non-touch MIDI control has no release message: a pause of at least 350 ms ends a movement, allowing a second movement from unity to boost. Continuous input cannot break through. Arrival time owns this boundary even while transport is stopped or input is queued. Pan, cutoff and plugin parameter mappings retain their own ranges. Physical controller feel still needs human acceptance.

Adding a marker for the selected matching parameter retains that exact segment or mixer-lane owner. A volume marker on a selected default fade stays on its combined line. Adding another parameter preserves the previous curve-control selection. Precise edits validate a candidate before creating Undo or changing the project.

The native preset matrix covers all three built-ins, all six Auto Arrange styles (Current with each built-in), named/portable and A/B/C routes, and every legal independent band mode, on both sides where a curve exists. It checks direct selection/reveal, handle dragging, marker ownership, locks, Undo/Redo, serialized points and identical-treatment reapplication. An absent curve is not applicable. This is not musical acceptance or exhaustive non-identical regeneration/reopen testing. Paired generated filters still use their inspector controls; direct paired-filter nodes remain open. **Add Filter Node Here** and the **F** shortcut use the existing manual LP / Off / HP cutoff parameter in 0.55.24. Paired filters remain separate, with a direct route from Editing to their Transitions controls.


### Curve bending (0.55.18)

Drag between two automation nodes vertically to change the bend while keeping their times and values fixed. For a rising bass curve, dragging up brings the bass in earlier; dragging down delays its rise toward the second node. At the lower limit it holds the first value until an exact step at node two. Drag the flat held section back up to restore a gradual curve. Flat segments with equal endpoint values have no bend to adjust. Nodes retain their existing time/value dragging and precise-entry controls.

The control applies to saved parameter segments and generated Volume/Bass/Mid/High band fades. Generated incoming and outgoing sides are independent; it does not move the transition frame or alter the partner curve. Existing instantaneous Swap markers still use their timing controls. Legacy segment volume fades without an editable segment keep their existing controls. Manual adjustments still compose with generated treatment, so an edited manual segment does not remove an independently active generated envelope.

A click only selects the line. A bend drag creates one Undo step and enables Read for that edited curve. Export and transition locks protect edits. Drawing and playback share the same bend evaluation; adding a node on the curve or slicing a segment preserves the shaped signal. Session format **29** stores bends; sessions through 28 open with their original interpolation. Older application versions cannot open newly saved version-29 sessions. Analysis revision 9 and source caches are unchanged.


### Transition grabs preserve transport (0.55.19)

Grabbing or dragging either transition width handle or its frame header selects that pair without seeking the playhead or scrolling the viewport. This applies while stopped and during ordinary playlist playback, including switching from another selected transition. Existing transition-rehearsal lifecycle rules remain. I/O handles retain their existing transport-preserving behaviour. Width/frame edits still use the same geometry, protection and Undo logic. Explicit Previous/Next transition navigation continues seeking to the selected frame's start.


## Pre-roll before musical zero (0.55.20)

Stop playback, then choose **View > Pre-roll before bar 1 > Add 2 bars**. One- and four-bar additions are also available; repeat an addition if a longer pickup needs more space. The session moves intact to make room on the left, with a marked **PRE-ROLL** area, signed times and a musical **1 | 00:00** boundary. The grey four-bar groups align with that boundary. Adding pre-roll changes the session in one Undo step and does not trim source audio or clear analysis/prepared audio. Busy/pending preparation, playback, rehearsal and locked transitions protect the operation; stop/unlock first.

Drag the complete song left into that area until the chosen downbeat meets the required bar or grey-group edge. Source-aware Snap remains available; hold **Alt** for free positioning if you need to line up a transient independently of an inferred source grid. Source segment automation and effects stay with the song. A newly inserted first song uses available pre-roll to put its trusted downbeat at bar 1 while retaining the file opening. An inferred downbeat still needs listening verification; a louder entry can be a later bar than the first quiet downbeat.

Ordinary editing retains the current playhead. Press **Home** or **Stop** to return to the retained intro. Reopening also starts there. Playback and WAV export include the complete intro before musical zero, and export omits unused blank pre-roll before the earliest retained segment (while preserving any deliberate gap after zero). An optional effect ending is still appended. Exported track-list times use the WAV's start, so labels and track entries match the audio file.

**Remove pre-roll** returns the musical origin to the beginning only when all retained audio, markers and captured mappings fit. If an intro occupies the pre-roll it rejects the change rather than cutting anything away; move that data to or after zero before removing it. Undo/Redo restore both the origin and the coordinated session clocks. Session format **30** stores the musical origin; older sessions retain their existing zero until pre-roll is explicitly added. New saves require 0.55.20 or later. Analysis revision 9 and existing source/analysis/audio caches remain compatible.


## Segment editing and view controls (0.55.25)

A **song/source** is a library audio file. A **segment** is a placed editable portion of that source. A **lane** is a mixer row. Bass, Mid and Treble name the EQ bands; high-pass and low-pass describe filters. VST headings identify whether inserts belong to a segment or a mixer lane. This wording does not change controller routing.

The Editing console has explicit Select segment, Cut, Copy and Paste at playhead controls beside the existing Reverse and Loop workflow. Select lists source, lane and start time. Cut leaves other segments in position; Paste uses the playhead and selected lane with Snap, shares the source and gives effects independent ownership. Undo restores an admitted edit. The console scrolls at compact sizes and remains the same editor when detached. Range editing, explicit ripple/gap-close and protected ranges remain separate work.

Hard-cut amount labels have vertical arrows. Drag up/down while the cut time stays fixed; horizontal movement and Snap do not move the cut. Generated gain/EQ treatments share their amount across both participating segments, including historical paired Bass cuts. Manual steps expose the value before and after the step. **CUT 100% means full attenuation; CUT 0% means unity.** A negative manual cut depth is boost. Filter displays LP/Off/HP frequency; FX displays its own normalised parameter, rather than an attenuation claim. Paired generated filters are continuous sweeps and retain their Transitions inspector.

Short lanes show one cut parameter at a time to keep amount targets distinct. Select the parameter in Editing or use **D** to reveal/cycle every available amount handle. In normal-height lanes generated parameters remain visible together. Press **D** to select/cycle amount handles, **Shift+D** for the previous handle, **Up/Down** for one-percent value steps, or **Shift+Up/Down** for ten-percent steps. **Enter**, double-click or right-click opens exact amount entry. Cancel keeps the treatment. Delete on an amount handle cannot delete its segment. Locks, preparation, active automation writes and export protect mutations. Existing horizontal swap handles still move paired timing.

The overview viewbox has separate left/right grips: drag an edge to zoom while its opposite time stays anchored, and drag the body to pan. **Shift+Left/Right** resizes the left edge; **Ctrl+Left/Right** resizes the right edge. Ordinary arrows pan and +/− zoom about the view centre. Manual navigation disables Follow and leaves transport and musical Undo unchanged.

Drag the right divider toward the edge to collapse the whole inspector. The centre reclaims its width, while the peak/master controls stay available. The edge grip restores the previous expanded width with Enter, Home or double-click; End collapses it from the keyboard. The collapse state is a local workspace preference, outside the saved mix.


### Saving the current document

**Save / Ctrl+S** updates the current file. An untitled mix asks for its first file. **File > Save mix as / Ctrl+Shift+S** chooses a new file; Cancel keeps the existing path, file bytes and dirty state. A successful Save As makes the chosen file current, so the next Save updates that file. The status bar keeps the document name and an asterisk for unsaved changes; the Save tooltip exposes the full path. Existing atomic writes, backups and recovery cleanup remain the save owner.

Native keyboard/accessibility selection: timeline segments and transition frames expose named musical objects with current owner, time and lock information. Tab/native focus or Enter selects the object through Editing/Transitions without seeking; an offscreen object is revealed and a focus outline identifies the keyboard target. Existing edit and Undo shortcuts apply. Delete on a transition selection targets its incoming segment; transition locks retain protection. Refresh preserves the focused object while it exists; removing it returns focus to the canvas. Marker/automation-node objects and the full Windows screen-reader journey remain separate access work.

Label and visible automation-node native access: focus selects/reveals without seeking; Enter opens the existing guarded name/time or node time/value editor. Node names identify owner and project/curve time; native value interfaces are read-only. Earlier point insertion cannot change which node an action targets, including equal-time cut endpoints. Value edits keep native identity; time changes retire it. On dense curves, Editing Previous/Next reaches every point while native traversal stays bounded around the selected/visible points. Canvas/ruler containers now expose their actual children, including offscreen objects. Tempo-marker/dedicated amount-widget access and the full hands-on screen-reader journey remain open.

Right-click a segment at a grid point and choose **Add Section Boundary Here...** to add a reusable source boundary and label. Enter/right-click on an S flag edits its label and position. These source annotations are distinct from project Label Markers (L); see [Section flags](SECTION-FLAGS.md).


### Reachable IN/OUT anchors while zoomed in (0.55.57)

Visible segments keep off-screen IN/OUT anchors reachable through arrow tabs at the visible timeline edge. Both anchors remain separate even when they lie beyond the same edge. Tabs indicate the direction of the real anchor; they do not relocate it on selection or zoom. Dragging uses displacement from that real position, with the existing snapping/Alt, linked-placement, lock and Undo rules. Collapsed lanes do not show extra tabs. Ordinary on-screen anchor bars retain their existing behaviour.


### Removing automatic tempo targets (0.55.57)

Delete/Cut of whole segments removes tempo targets owned by those segments instead of converting them into unowned markers. Explicit manual tempo points stay. If songs remain, their beat-matched playback is prepared against the updated map before committing one Undo; their timing may change, as requested. Locked transitions must be unlocked before a tempo-changing deletion. Deleting the final segment removes its automatic markers immediately. New Mix clears all tempo points and pending tempo edits; a late preparation result from the previous mix cannot repopulate the new document. Old files whose ownership was already discarded cannot reliably distinguish those old automatic points from intentionally manual points; New Mix clears both.


### Drag multiple library tracks (0.55.57)

Ctrl/Shift-select library rows and drag them onto the timeline. The batch uses their displayed library order at drag start, including the current sort and filter, even if the library is subsequently resorted. It uses the existing automatic transition placement on the chosen lane, with the current new-song settings. One Undo removes the entire batch. Missing/stale source identities reject the complete drop; playlist rows cannot accidentally duplicate a selection into the timeline. File hydration can finish asynchronously without changing the requested order.

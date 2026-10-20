# Timeline editing — v0.27.0

## Audio outside transition windows

Each linked pair sets two hard playback boundaries: the incoming track is silent **before the window starts**, and the outgoing track is silent **at and after the window ends**. The waveform remains visible at reduced opacity in those excluded regions, so you can still edit or slip the source. Material between transitions stays audible; the first track keeps its lead-in and the final track keeps its ending.

These cuts apply independently of volume/EQ mode, fade strength or manual automation. Moving or resizing the frame updates them immediately. Audio slip moves the source inside the same bounds. Clip insert and EQ output also stop at the cutoff; shared lane/master effects retain their usual behavior. Playback, WAV export and A/B/C rehearsal use the same boundaries. Clip lengths, source files and automation positions stay intact. Removing a transition link restores the clip's own fade/source boundaries.

## Independent audio slip

Each timeline clip has its own hexagonal left/right slip handle at the right of its visible header. Dragging it slides only the audio/waveform, leaving the clip boundaries, automation points, fades, IN/OUT alignment markers, tempo map, paired tracks and transition frames fixed. A complete drag has one Undo. Snap aligns an actual source beat with the mix pulse; hold Alt for exact free movement, including to the left of mix zero. Double-click the handle to reset its offset. Unlock protected tracks before editing. Unavailable audio at exposed edges is silent. Playback, waveform source ticks, splitting, Save/Open and WAV export share the saved audio clock (format 22). Normal clip-body movement and paired transition handles keep their existing behavior.

The user’s screenshots guide the energy overview, four-bar divisions, orange intro/outro markers and right-click menu. The current requirement is explicit: IN/OUT align consecutive tracks and move them on the timeline, while fades and other controls remain separate.

## Behavior and ownership

- Energy view uses 2.5 × cached mean-square amplitude, capped at the display height. At close zoom it shows RMS and transient peaks. The scale is fixed, independent of the visible passage; audio and analysis caches are unchanged. Detail and peak-only views remain available.
- Strong guides and alternating bands count 16 beats on the shared tempo map. Finer subdivisions yield to the waveform when zoomed out. These are four-bar counting guides, not detected musical phrases.
- `Clip` owns optional source-elapsed intro/outro alignment markers and converts them to local timeline times. Omitted markers sit at the clip edges, independent of fades. Each marker can move across the whole track.
- `Project::moveTransitionAnchor` uses chronological clip order, independent of playlist insertion order. OUT aligns the next IN to its new position and reflows each later track. IN selects a different source position, slides its track under the previous OUT and reflows later tracks. The first IN and last OUT remain unpaired. `Clip::alignIntroTo` owns source/tempo conversion and uses the existing non-destructive `moveTo`/`retime` path. Tempo points, automation and recording mode do not change; source spans and source fade spans survive reflow. Moves requiring a negative track start fail atomically. Snap/Alt and one Undo entry per drag remain supported.
- Playback and the displayed gain envelope use separate fade positions. `Clip::slice` preserves off-segment fade positions so splits keep the original audible fade. Session version 10 stores alignment and fade positions separately and reads versions 1–9. Older marker values are interpreted as alignment guides; malformed positions are rejected.
- The right-click menu and shortcuts share command handlers. Cut/copy/paste operate on whole clips; marker actions use existing automation lanes. Effects require an automatable VST. Loop start/end creates two repetitions of the selected source range, undoable in one step. “Tweaks” opens the app's BPM/key/downbeat editor; the full MixMeister tweak preset library is not implemented.

## Verification

Current production-component checks cover linked IN/OUT gestures, three-track movement, chronological adjacency, repair of previously unaligned markers, zero-boundary rejection, retained source/fade spans and tempo controls, sample-identical mixer PCM with markers shown or cleared on the moved arrangement, Snap/Alt, Undo/Redo, recording-mode retention, session validation and split fades. Broader placement, tempo and core tests protect existing editing behavior.

The following verification records describe earlier releases and their former linked-transition behavior; they do not establish the v0.26 alignment-only semantics.

Real inputs are **Better Off Alone** and **Ode To Omar**. Nine scenarios retain fixed-tempo, ramp, drop, tempo-edit and movement checks, and add intro/outro edits. `.local/real-tracks-v022/` contains 93 passing checks, real UI snapshots, listening mixes, isolated stems, source/sidecar digest audits and independent envelope timing measurements. Both real source pulses and bar phases remain aligned after anchor edits. Original audio and sidecars are unchanged. Downbeats remain 0.540735 s / 0.053062 s; the prior reference limitations still apply.

`Dance To The Beat` from the supplied waveform screenshot is also rendered through the production UI in energy, detail, peak and whole-track views. This is a visual comparison using local audio, not an assertion of pixel-identical MixMeister rendering.

Final focused anchor verification: **40 checks**, `.local/real-anchors-v022-release/`, unchanged source/sidecar digests. Independent bass-envelope timing comparisons accepted all 520/684 examined windows: OUT p95 **4/3 ms**, maxima **8/6 ms**; IN p95 **4/4 ms**, maxima **6/8 ms**. This measures transfer timing, not musical downbeat ground truth. The nine-scenario run still shows the pre-existing fixed-tempo maximum of **46 ms**.

Regression checks: **454** existing checks in `.local/verify-rhythm/v022-release/` and **29** new editing checks in `.local/v022-packaged-editing/`, all passing. Same-song waveform verification has **28** checks in `.local/v022-dance-release/`. The final packaging-only change lets the timeline receive keyboard focus; editing checks were rerun after it. The deployed executable is `out/releases/0.22.0-2be3aa095a5c/Mixworks.exe`, SHA-256 `2BE3AA095A5C882638E95F611AE605BA9409F841C0E56BFDB9411F0EDCCC68C5`; real audio runs retain their exact build identities in `run.json`. Earlier releases and launcher backups remain available. Existing unsaved sessions were left open.

No human listening or physical audio-device pass is claimed. Existing fixed-tempo stretch outliers and broader MixMeister detection/stretch parity remain separate work. Automated mix selection remains deferred.


## v0.24 interaction and live insertion corrections

Short overlapping drops now put the outgoing OUT at the incoming IN, rather than calculating it backwards from a longer outgoing file. Append uses the chosen audible end. Anchor hit testing includes both edge triangles and the exact right edge, with priority over automation. Hover cursors identify draggable clips, anchors, automation points, labels and tempo pins.

RMB marker actions retain the waveform, curve selection, recording mode and playhead. The automation dropdown was removed. Existing curves overlay the waveforms; click/drag a point to select its controls and edit its time/value. Ordinary clip/anchor gestures remain available. The tempo strip attaches below the four channel lanes, capped to the available window height. Fullscreen starts with a hidden title bar; F11 restores a window and File → Exit/Alt+F4 retains the close confirmation.

Live additions use the existing tempo map and immediate elastic mappings for incoming tracks. Existing prepared PCM/elastic identities, playhead and plugin DSP are retained. Pending/failed clips never render raw, unmatched audio. Background hydration, failed input, subsequent additions and Undo no longer stop the running transport; Pause remains enabled. Live drops into the past join at the next aligned beat/bar. Idle arrangement still prepares native-tempo ramps. Explicit tempo edits retain their existing preparation workflow.

Verification: **399 checks, zero failures**, `.local/v024-release/` (core 279 with test VST3/Vital; editing 44; placement 42; Alloy 34). `.local/v024-red/` records the three original failures. Includes sample-identical uninterrupted callback PCM, lazy loading/failure continuity, upcoming live placement, short overlaps, insertion-order-independent cue links, uncertain neighbours and actual marker/clip/cursor gestures. Release build has no C++/linker warnings or errors. Fullscreen/footer native appearance verified; human input interrupted the native second-track playback check, so no fresh listening or completed real-device live-insertion pass is claimed.

## Pair bass swap from the right-click menu

Right-click the receiving clip and choose **Pair bass swap** to complement the nearest full manual cut on its existing transition partner. The original cut stays in place; the new pair uses the existing linked handles and one Undo. Ambiguous or conflicting cuts are explained without replacing existing work. See [pairing rules and handles](SWAP-HANDLES.md#pair-an-existing-bass-cut-052).


## Suggested energy sections

Alloy can show numbered source-section strips such as **S1 ?** beneath the waveform. These are optional energy-change suggestions derived from saved waveform RMS: four bars before/after, at least 3 dB contrast and eight bars between accepted boundaries. They use the existing source downbeat phase and follow the waveform through source trimming, tempo mapping and audio slip. The display toggle itself does not adjust alignment, alter analysis or write new metadata. The **Sections 8–64 bars** insertion default uses these same suggestions when placing new tracks, preferring a shared boundary on both songs. The hints do not identify verse/chorus or guarantee phrase accuracy. Sections with similar loudness may be missed.

Use **View / Suggested energy sections** to hide/show them for this app run. Missing/incomplete RMS, unknown downbeats, variable-tempo sources without a manual constant grid, reverse and loops remain unmarked. No rescan or cache reset is required; original analysis, corrections, mixes and controller mappings are retained.


## Selected automation node level

Click or drag a node to show its percentage beside that node; release keeps it selected. Selecting another node transfers the single caption, and clicking the background clears it. Other curves and node dots stay visible. Gain/EQ percentages use linear amplitude: unity is 100%, silence is 0%, and a boost may exceed 100%. Low-pass percentages express the position within its 20–20,000 Hz range; pan and VST parameters use their normalised values. Double-click/precise editing retains the parameter's physical units. Selection is a view choice and adds no musical Undo entry.

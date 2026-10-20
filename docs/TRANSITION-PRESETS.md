# Transition editing, rehearsal, presets and locks

Auto Arrange offers Balanced, Smooth, Bold, Use my settings, Constant power and Hard cuts only, with a separate Energy direction selector. Its Balanced/Bold treatments replace the provisional volume fade; Smooth and the unknown-energy fallback use the ordinary volume crossfade. Constant power and Hard cuts have their own Volume treatment and never inherit that default fade. Hard cuts use a short anti-click ramp at the frame's musical midpoint. Constant power follows a sine/cosine gain pair; it maintains summed source power for equally loud unrelated recordings, rather than matching their loudness. The same compiled curves drive the timeline, playback and export. **Constant power** is also available in the selected transition's Volume menu. Sessions containing the new mode use format 26; old sessions and cached analysis remain compatible with this app. See [Auto Arrange choices](../README.md#auto-arrange) for energy-policy limits.

Transition windows also define hard audio cutoffs. Incoming audio is silent before the frame's start, and outgoing audio is silent from its end onward; excluded waveforms are ghosted. These boundaries remain active for None, manual and partial-strength treatments, and follow frame moves/resizes. See [cutoff behavior](TIMELINE-EDITING.md#audio-outside-transition-windows).

The **New tracks** dropdown in the lower **Transitions** panel offers **Align IN / OUT**, **Bass swap**, and **Crossfade**. It starts on **Crossfade / Sections 8–64 bars**. Each addition captures those defaults before background preparation, so changing the selectors later does not alter pending additions. These controls are defaults for new tracks and whole-arrangement Beatmatch. To edit an existing pair without rebuilding the mix, select its frame and use the lower **Transitions** panel.

## Playlist preset picker

Each Playlist row has a **Transition in** dropdown for the transition **into that track**, with None, Bass swap, Crossfade and saved session presets. The first track has no incoming transition. Sorting the playlist does not change the pair being edited. Changes retain the frame length and track positions, follow the existing lock/manual-automation guards and have one Undo step. Existing or edited treatments may show Custom / edited. Column order and resized widths remain available.

The default overlap varies from **8 to 64 bars** in constant-tempo 4/4. It searches outgoing energy-section changes within the last 64 source bars and incoming changes near the head, preferring a simultaneous change on both tracks, then a match on either track. Among equally matched valid choices it selects the shortest overlap. With no usable hints it falls back to an eight-bar grid overlap; source bar phase may require a small length adjustment. These are RMS energy suggestions, not identified verses/choruses or guaranteed phrase boundaries. The source files and full usable audio spans are retained.

Automatic mode rejects a placement when at least eight bars cannot fit in both tracks, or playback is too late for a valid 8–64-bar overlap. Add the track earlier or select a fixed length. Fixed-length choices remain available; Align IN / OUT retains its own anchor behaviour. Each asynchronous addition retains the mode selected when it was queued. Existing saved mixes are unchanged when opened; whole-arrangement Beatmatch rebuilding remains explicit.

**Rehearsal & presets...** expands the A/B/C, context and preset-management controls. They start collapsed, retain their existing functions and scroll when necessary. Wide panels show all four bands in a row; compact panels use two columns.

## Editing the selected transition

Click an orange frame header, or click **Transition** beneath the Clip / Effects / MIDI tabs. **Previous / Next** selects a pair and brings its start into view. The panel groups length, preset and lock controls above the four bands, followed by filter controls. Track details and long instructions no longer occupy the panel; key context and shortcuts remain in tooltips.

- **Length** immediately changes that frame to **4, 8, 16 or 32 bars**, measured through the current tempo map. Its start and both songs remain fixed. Existing paired bass and fades scale with the frame. An unavailable length is rejected without moving or trimming songs.
- **Preset** offers **None (keep alignment)**, **Bass swap** and **Crossfade**. None removes the pair's owned bass handoff and volume fades while retaining its frame and cues. Bass swap adds paired bass EQ without a volume fade. Crossfade creates both volume fades. Unrelated manual automation is retained; a Bass swap that conflicts with manual or neighbouring bass points reports the conflict and leaves the project unchanged.
- Each successful length, preset or lock change has one **Undo** entry; **Redo** restores it. These local edits do not rebuild the mix, prepare audio, seek or restart playback.

The preset menu is an apply command rather than an inferred label for custom curves. Existing manual automation can still shape a transition after a preset is applied. The insertion defaults at the top of the panel remain separate.

## Independent Volume, Bass, Mid and High

The selected pair now has four independent rows in the **Transitions** panel. Scroll within the panel in a smaller window. Changes apply immediately, with one Undo per committed edit, while song placement, tempo and prepared audio stay fixed. Coloured curves on the selected frame preview the same envelopes used by playback and export: orange Volume, yellow Bass, blue Mid and purple High.

| Control | Behaviour |
| --- | --- |
| Mode | **None**, **Crossfade**, **Fade in**, **Fade out**, **Swap** or **Manual** on every band. Bass also offers **Swap fade**. Fade in changes the incoming song; Fade out changes the outgoing song. Crossfade changes both. |
| Strength | **25 / 50 / 75 / 100%** attenuation depth. At 100%, Volume reaches silence and an EQ band reaches Alloy's full cut. Lower strength retains more of the original level. Strength is independent for each band. |
| Gap | Separation between the two swap-ramp centres of **0 / 0.5 / 1 / 2 / 4 / 8 mix beats**. Longer Swap fade ramps can overlap when the gap is shorter than their width. A full-strength Volume Swap creates silence around the crossing. Gaps follow the integrated tempo map, including a BPM ramp. |
| Swap at | Place that band's crossing at **25 / 50 / 75%** of the transition's beats. A gap that cannot fit at the selected position is rejected without changing the mix; shorten the gap, move the crossing or lengthen the frame. |

Swap uses a short anti-click ramp; Bass **Swap fade** uses a one-beat ramp, shortened for a very small frame. None removes only the generated band treatment. Manual removes the generated treatment, selects that band's ordinary automation editor and retains existing manual curves. Generated bands combine with existing manual automation; their settings are not a second stored copy of those points. The frame's existing source-owned boundaries still control audio outside the transition.

**Existing / custom** means that band has no new settings record. Earlier fades and paired bass points are retained on opening. Editing Volume explicitly replaces the selected pair's earlier volume fades; editing Bass explicitly replaces its owned bass handoff, leaving unrelated manual curves intact. Choosing Manual on an earlier bass handoff keeps its points as ordinary automation. Other bands and neighbouring relationships remain unchanged. Applying a whole-pair preset intentionally resets all four generated treatments to that preset's existing behaviour. Bass swap remains bass-only unless you independently add Volume.

Locks protect these settings, including an adjacent pair that shares a protected track. Session format **16** saves optional per-band settings and reads versions 1–15 without inventing them. Earlier Alloy releases cannot read v16 projects; the normal save workflow retains backups.

This adopts the composable workflow described by [DJ.Studio's Transition Editor](https://help.dj.studio/en/articles/8106059-transition-editor), checked 19 October 2026. Strength here has the explicit attenuation-depth meaning above; exact DJ.Studio curve/DSP equivalence is not claimed. Curve visibility/isolation, saved user presets and rehearsal are delivered below. Paired filters are deployed in0.40; see [their controls and verification](TRANSITION-FILTERS.md). Noise and additional effect generators remain future work.

## A / B / C rehearsal and saved presets

Select a frame and scroll to **A / B / C** in the Transitions panel.

1. Click **Store A** to retain its current treatment. Edit the ordinary transition controls, then **Store B**, and optionally edit again and **Store C**. Each Store deliberately replaces that slot and has Undo. Save the session to retain the alternatives.
2. Click **Hear A**, **Hear B** or **Hear C**. The first click prepares the stored alternatives, then loops that choice. **Prepare rehearsal** explicitly rebuilds them and starts the first stored slot. Preparation can be cancelled. Switching Hear restarts the comparison at the same beginning.
3. The rehearsal includes the existing mix, effects and manual automation, with the selected **Before / After** context (default one4/4 bar each) around the selected frame where audio is available. Its non-silent choices are matched to the quietest choice's complete-window **RMS by attenuation**. Silent choices are labelled explicitly. This is average-level matching, not perceptual LUFS normalization or independent headphone preview.
4. **Apply A/B/C** applies the chosen treatment to the existing frame with **one Undo**. Comparison gain is monitor-only and is not applied to the mix or export. **End rehearsal** returns to the previous cursor, paused. Stop cancels rehearsal and rewinds; ordinary seeking, arrangement/control edits, navigation, loading and export invalidate the preview.

Rehearsal renders a snapshot through the same renderer as playback/export; audition itself does not commit an alternative or record automation. It starts effects from a reset state at the lead-in, so effects requiring a longer history can differ from an uninterrupted whole-mix render. Native plugin-editor changes made after preparation require **Prepare rehearsal** again. Preparation runs off the UI/audio callbacks; published stereo float buffers share a **256 MiB memory budget**, checked before allocation. A larger rehearsal reports the limit and asks for a shorter frame. Cancellation and errors keep the session unchanged.

Each treatment includes all four optional band settings, earlier transition volume fades, and an earlier explicitly owned paired bass handoff with its values. Positions within the frame are retained as fractions of its timeline span. Applying elsewhere uses that destination's existing frame; it does **not** change its length, move songs, copy manual curves/plugins or change source analysis/tempo. Manual or neighbouring bass points that conflict with a restored handoff cause an atomic rejection. A legacy fade or owned bass ramp extending outside the selected frame cannot be captured as a portable treatment; its current audio is kept and the panel explains the boundary.

**Save preset...** names the current treatment under **Saved session presets**. An existing name deliberately replaces that preset with Undo. Choosing a saved preset applies it to the selected frame. **Delete preset** removes it from the session with Undo. **Export preset...** writes a portable `.alloypreset` file; **Import preset...** adds one to another session without applying it. An imported duplicate receives a distinct name. Existing exported files receive a separate backup before replacement. Presets are stored with the session, rather than silently creating a global library.

Locks permit storing and rehearsing alternatives, while **Apply** and preset application remain protected by this pair and its locked neighbours. Session format **17** retains A/B/C and named presets and reads formats 1–16 unchanged. The temporary audition buffers and matching gains are never session data.

A/B/C belong to that relationship. Removing it, or using whole-arrangement **Beatmatch** to rebuild relationships, can remove its slots along with the previous transition edits; Undo restores them. Save useful choices as named presets before a global rebuild. Named session presets remain available independently of the relationships.

## Transition locks

**Lock transition** adds a green status badge and changes its header to **LOCKED**. The frame keeps its orange treatment. **Unlock transition** enables editing again. The lock is saved with the relationship and participates in Undo/Redo.

A lock protects **both participating tracks** from geometry, trimming, split/remove/cut, reverse/loop and source-marker edits, as well as control automation and EQ/volume/pan edits through Alloy. Connected or adjacent gestures cannot move a protected track indirectly. Lock protection is checked by model operations and controller input, not just by disabled buttons. Playback, mute/solo audition and master output remain available. Native VST editor state is outside this arrangement/automation lock.

This first implementation conservatively requires unlocking before **global tempo changes or Beatmatch**, or duplicate/paste operations that add tempo markers. It rejects the operation rather than rebuilding around a locked pair. There is no automatic partial rebuild of unlocked sections. Unlock both neighbouring relationships before editing a song shared by two locked transitions.

Locks were introduced in format **15** and remain in format **16**. Versions 1–14 open unlocked. Save backups are retained by the existing save workflow.

## Placement

An alignment-only insertion joins the incoming IN to the existing outgoing OUT. An existing OUT remains authoritative; if it is omitted, the outgoing clip edge is used. The incoming musical cue uses its known downbeat, falling back to its pulse when the bar is unknown. Source audio, including a pickup before IN, is preserved. This mode creates no bass EQ or volume fades.

Bass swap and Crossfade use the selected overlap length, restricted to the available source overlap. Bass handoff selects a shared bar boundary when both source bars are known, with a shared beat as fallback. Its paired ramp endpoints are converted through each clip's source clock, including a changing mix tempo. This is beat/bar-aware placement, not a newly reconstructed phrase detector. Existing manual bass curves are preserved instead of being silently overwritten by an automatic bass preset.

The preset and length are captured when an insertion is requested. Changing the dropdown while decoding/preparation runs does not change the pending insertion. Live insertion retains the existing transport. If the chosen OUT is already behind the live insertion position, alignment-only insertion reports that it cannot make the pair; stop playback or move OUT forward before adding again. It does not silently move the user's OUT or claim an unaligned pair.

## Bass ownership and editing

Each explicit transition link can own four source-elapsed coordinates: outgoing ramp begin/end and incoming ramp begin/end. Dragging either owned bass endpoint, or editing its time numerically, moves its partner to the same session position. Its value can be edited independently. Movement respects both curves' neighbouring points and the shared audio range. Undo/Redo retains the relationship.

IN/OUT and whole-track movement reconcile the bass pair through the updated source clocks. A move that would cross unrelated EQ points or put the bass swap outside shared audio is rejected atomically. Ordinary EQ edits preserve pairing. Deleting an owned endpoint, clearing its curve, or recording over that endpoint detaches only the affected bass relationship. Other transition links remain intact.

Splitting a clip translates the owned coordinates into the retained segment. If a split separates the ramp from the clip that owns the transition, its bass pairing is detached while the sliced automation/audio remains. This avoids retaining references to points that no longer belong to that segment.

Bass-pair coordinates were introduced in session format **14** and remain supported in format **16**. Old projects are opened without inventing bass ownership; explicitly reapplying Bass swap creates the pairing. Original recordings and detector/model data are unchanged.

## Transition frame and independent song movement

During playback, frames pulse orange on a **1-2-3-4** count from the existing mix clock, with a stronger first beat. Wide headers show the four counts. Tempo changes and ramps use that same clock. Pausing leaves a steady warm outline. Windows' client-area animation setting suppresses the repeating brightness pulse while keeping the count and lock cues readable. The molten timeline scrollbars retain native scrolling, focus and accessibility. These accents are presentation only; they do not change audio, frame geometry, locks or saved sessions.

Each linked pair has an orange frame spanning its outgoing and incoming lanes. Its header shows the transition length in bars. Drag the header to move the transition through the outgoing song, either white top-corner grip to change the start or end, or the labelled **OUTGOING / INCOMING** handles to move a song within the fixed frame. Ordinary waveform dragging retains linked-track movement. Snap applies unless Alt is held. Each complete gesture has one Undo entry; Redo restores it.

Changing the start repositions incoming audio around its selected source IN while keeping the outgoing song stationary. Changing the end retains the start and both song positions. The song handles keep the frame stationary and retain the counterpart's placement. Existing linked neighbours outside the pair follow their established relationships. Edits that exceed either song's available audio, cross unrelated bass automation, or invert the frame are rejected atomically.

Explicit frame edits mute incoming preroll before the left boundary and outgoing audio after the right boundary. Existing crossfades scale with the frame. Paired bass ramps follow its relative timing; other manual EQ stays source-owned. Default alignment-only insertion remains an alignment operation until an effect or frame edit is requested. Bass preset placement leaves room after a preceding owned bass handoff on short tracks, so the next transition cannot overwrite it.

The frame has no second stored rectangle: its start is the outgoing OUT, paired to incoming IN, and its end comes from the outgoing source-owned fade envelope, bounded by incoming audio. Existing source cues, fade anchors and transition relationships own persistence and clock reflow.

Reference: [DJ.Studio Transition Editor](https://help.dj.studio/en/articles/8106059-transition-editor), checked 18 October 2026. Its documented header, boundary handles and independent song bars informed Alloy's controls. Read-only REA inspection of the installed 4.2.15 frontend chunk is retained under `.local/transition-edit-13/`; semantic coverage is partial and does not prove runtime drag behavior. No DJ.Studio project was changed.

## Verification evidence

Current visual release **0.33.0-111783fd6a57** is deployed. Exact frozen `.local/molten-20/bundle-03/` passes **1,274 checks / 18 suites**, zero failures or infrastructure errors, including 157 Alloy and 282 timeline checks plus real test-gain VST3/Vital. Count/ramp/paused/reduced-motion checks, native scrolling/accessibility and unchanged session/lock/Undo coverage pass. Native captures cover the four counts, paused, narrow/large windows and 200% vectors. Receipt `receipt-03.json` passed VerifyOnly and deployment; reports/captures are `.local/m20-r3/`. Sixteen audio/model/CMake files remain byte-identical to the rehearsal release. Physical mouse/controller and UI frame-time measurements were not performed. No new musical-accuracy/listening result is claimed.

Earlier rehearsal/preset release **0.33.0-940844d9dc1b** was deployed. Exact frozen `.local/rehearsal-19/bundle-04/` passes **1,260 checks / 18 suites**, zero failures or infrastructure errors, including **282 timeline checks** and real test-gain VST3/Vital. The focused workflow includes native Prepare/Hear/Cancel, Apply/Undo/Redo, portable backups, strict session persistence, manual/lock protection, memory/silence and automated VST state restoration. Real Arc Mode/Chilli pass **41 checks**, with original source hashes unchanged and nine finite non-silent stereo WAVs, including three rehearsal callback captures. Actual Save/Open retains A/B/C and named presets. Receipt `receipt-04.json` passed VerifyOnly and deployment; reports are `.local/t19-r4/` and `.local/t19-real4/`. No physical mouse/controller or listening test is claimed. Native component captures were inspected. Recovery and precise boundaries are recorded in HANDOVER.md.

Earlier independent-band evidence is retained in `.local/transition-bands-17/`. Frozen `bundle-03/`, SHA-256 `59802e12219bc563a66d798fed99064564488ab305e09235cbefa5f249955f66`, passes **1,100 checks across 18 suites**, zero failures, including **222 timeline checks** and real test VST3/Vital workflows. Exact reports are in `.local/t17-r3/`; `receipt-03.json` was accepted by the deployment script. Arc Mode/Chilli pass **30 real-track checks**, including public Save/Open with all caches reused and full-strength Volume-gap silence. Four stereo renders contain finite, non-silent audio. Native Computer Use verified all saved settings, unlock, independent Volume strength, Bass gap and Mid mode edits, unchanged High and fixed geometry/playhead. Undo/Redo is component-tested; listening and exact DJ.Studio DSP parity are not claimed.

Normal deployment updated the backed-up `out/Launch.cmd` to immutable **`out/releases/0.32.0-59802e12219b/Mixworks.exe`** without restarting the user's application. Launch Alloy to use the controls. Source checkpoints and release identity are recorded in `session-result.json`. Earlier local controls/locks and frames remain under `.local/transition-controls-16/` and `.local/transition-edit-13/`. Intermediate failures and superseded candidates are preserved. The loading findings and measured cache-hit costs are recorded separately; this release does not change the loading policy or disk-cache budget.


## Curve display controls

Each transition band has **Show curve** and **Only curve** buttons. Show hides or displays that band's generated preview. Only isolates one band's preview; click it again to restore the other visibility choices. Isolating a hidden band makes it visible. Hiding the isolated band returns to the remaining visible curves.

These are window-local display choices. They apply as you browse transition pairs, remain available on locked pairs and do not change audio, automation, master gain, transport, session contents or Undo history. Manual automation retains its ordinary editor. At narrow window sizes, scroll the transition inspector to reach every band's controls. Exact verification/deployment status is in HANDOVER.md; the controls are deployed in 0.34.0-050b2b856b26 after 1,328 passing checks across 18 suites.

## Adjustable rehearsal context (0.42)

The inspector adds separate **Before / After** choices of **0 / 1 / 2 / 4 / 8 bars**, defaulting to one bar each. They define the common A/B/C window through the integrated mix beat map, clipped to available session audio and its selected tail. Complete-window RMS matching uses that same context for every alternative. Short/zero context starts fresh effect history or ends at the frame, so it can intentionally omit a comparison tail. This adds no private headphone route.

Context changes have one Undo, retain ordinary playback position/playing and automation mode, and invalidate cached rehearsal. Locked pairs still permit these monitor choices. Session21 requires its context object and reads1–20 with the old1/1 default; portable musical presets remain version2 and exclude monitor context. Invalid/fractional/negative/overflow counts reject before conversion; the existing256MiB rendered-audio budget governs actual expanded windows.

Candidate03 context tests pass actual zero/asymmetric callback loop lengths, tempo-map integration, cancel/silence/budget cases, production Save/Open, malformed/legacy parsing, locked/busy/native/Undo paths and narrow captures. Two ordinary exports with different context choices are byte-identical. The exact0.42 bundle passes1,676 checks/22 suites with no failures/errors and is deployed; see SWAP-HANDLES.md for release identity.

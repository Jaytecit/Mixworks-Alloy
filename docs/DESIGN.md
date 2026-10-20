---

name: Mixworks Alloy

description: Precision, with warmth. A native studio workspace in graphite, metal and copper.

colors:

  background: "#15191D"

  panel: "#20262B"

  raised: "#2A3239"

  border: "#3B454E"

  text: "#E7EBEE"

  muted: "#ABB5BE"

  copper: "#D49A72"

  molten-edge: "#753519"

  molten-orange: "#FF933D"

  molten-core: "#FFE1A0"

  focus: "#D5E9F5"

  error: "#EB8280"

  track-blue: "#80B2CF"

  track-sage: "#9EB8A1"

  track-mauve: "#B5A3C5"

typography:

  body:

    fontFamily: "Segoe UI"

  control:

    fontFamily: "Segoe UI"

    fontSize: "12px"

rounded:

  control: "4px"

components:

  button:

    backgroundColor: "{colors.raised}"

    textColor: "{colors.text}"

    rounded: "{rounded.control}"

  input:

    backgroundColor: "{colors.background}"

    textColor: "{colors.text}"

---



## Overview



**Creative North Star: "Precision, with warmth."**



The user chose subtle studio hardware: graphite, brushed metal and copper details. Rounded hexagonal dials are the signature. The daily workspace stays restrained so waveforms, values and selection remain clear. This captures the native v0.14.0 implementation; `design/alloy/concept-02/` retains the approved visual reference.



## Colors



Copper identifies active controls, tabs, tempo and the first track. Blue, sage and mauve distinguish the remaining tracks; names and selection outlines also carry identity. Neutral surfaces establish depth. Pale focus outlines remain distinct from copper selection. Error red is reserved for faults and clipping. Runtime colors are owned by `Source/AlloyLookAndFeel.h`; update that source and this specification together.



## Typography



Segoe UI supplies familiar Windows legibility. Use compact 11–13 px button labels, 12 px selectors and larger numeric readouts where needed. Preserve readable track names with ellipsis and tooltips. The header wordmark is small, with a stronger MIXWORKS weight and copper ALLOY.



## Layout



Library and Playlist share tabs in a full-height left column. Track controls stay attached to timeline lanes. Master output and Clip/Effects/MIDI occupy the right inspector. File/edit commands and the always-visible Beatmatch/Cut controls stay above the workspace; transport and automation stay above the timeline. Beatmatch also owns a top-level menu. A restrained copper lane wash, insertion line and exact seconds label preview manual library/file drops; the target uses the visible timeline's coordinate system. Dropped tracks match the shared tempo and align near the requested position. The first track sets the starting BPM; two editable footer points control each transition toward the incoming tempo. Full Beatmatch rebuilding remains an explicit command. Publish clips and saved waveforms immediately; label preparing/unavailable audio and pending external-file durations. Keep arrangement editing and Undo/Redo available during source loading. Playback and file output must wait for readiness, without a modal import panel for timeline drops.



Default preferred widths are 320 px left and 270 px right. Each panel has a 240 px minimum; the centre has a 620 px minimum. Dividers are 10 px wide. A narrower window clamps actual widths without discarding preferences. Below 760 px centre width, transport controls narrow slightly to retain one row. Automation selection and recording never reserve a toolbar row or change the timeline layout. The upper timeline header contains only the 24 px ruler and 20 px labels. A separate 22 px tempo footer stays at the bottom, follows horizontal scrolling and remains visible through vertical scrolling. Timeline lanes are 132 px high. These are JUCE logical pixels, not physical display pixels. `WorkspaceLayout` owns workspace geometry; `Timeline` owns its internal lanes. The small Alloy emblem occupies the corner above track controls.



## Elevation & Depth



Quiet tonal separation defines panels. Thin borders and slight button gradients indicate controls. Metallic highlights, a copper rim and fine circular grain are confined to dial caps and the small emblem. Draw these as vectors so scaling remains sharp. Avoid texture behind waveforms and long text; keep the timeline readable during extended sessions.



**Molten accents, requested 19 October 2026:** extend the material to the timeline's two 14 px scrollbars, active tab seams, Play/Hear controls, master/zoom thumbs, focused/hovered slider caps and selected tempo markers. A burnt-copper edge surrounds an orange core with an irregular pale seam and a small bounded halo. Keep the graphite surface, native hit areas, focus outline, text and waveform detail intact. Other scrollbars and inactive controls remain restrained. These native vectors require no bitmap assets or blur passes.



Transition frames now use orange instead of blue/green box fills. Their translucent wash and outline surge once per mix beat in a 1–2–3–4 count, with a stronger first beat. Wide headers show four count cells. Use the existing integrated tempo map/playhead for every frame, including ramps; never maintain a separate musical timer. Paused transport and reduced motion use a steady orange treatment. Keep the LOCKED wording and a green status badge, with unchanged lock enforcement and edit handles. Respect Windows client-area animation preferences and reuse the existing 25 Hz UI repaint; skip offscreen frame work.



Native Windows caption bars use the background and text palette. Preserve OS window controls, dragging, snapping and resizing. Windows 10 uses its dark-caption support; explicit caption colors are supported on Windows 11.



## Shapes



Controls use modest rounded rectangles. The signature rotary cap is a rounded hexagon inside a fixed circular scale with a 270-degree sweep. Its face and pointer rotate together; the value remains upright below. A circular focus outline surrounds the dial.



## Components



- **Transition cutoffs:** ghost the waveform at 14% of its normal opacity before a linked incoming frame starts and after a linked outgoing frame ends. Derive both edges from the compiled transition playback facts used by audio. Keep the audible waveform at normal strength and preserve labels, automation, source ticks and editing handles. A middle track combines its incoming start and outgoing end; unpaired clip edges retain their usual fades. Moving/slipping material never changes these visual cutoffs independently of the frame.



- **Shared lower panels:** Controller and Transitions tabs share the lower centre. Selecting a frame reveals Transitions independently of the right inspector. Each panel has the same 32 px hexagonal icon-only attach/detach control, with a window/arrow glyph, tooltip, accessible action name and focus outline. Detached windows keep their header control; closing attaches them. Both may detach independently, leaving their tabs reachable and freeing timeline height. At short window heights, protect one complete timeline lane and let tool content scroll on either axis. Transition defaults and all selected-pair, band, filter, preset and rehearsal controls belong in the Transitions panel; wider windows use columns, smaller ones scroll.

- **Live desk:** a graphite instrument chassis with an engraved copper seam and small corner fasteners. Eight hexagonal pots form Bass A/B, Mid A/B, High A/B and LPF A/B pairs. The top eight pads cycle each pot's lane, excluding the partner's target; names and lane colours carry identity. The lower eight pads are four mutes and four solos. Four level faders, four pan faders and master sit beside them; small pads reset four EQs, open four filters and centre all pans. A 2×4 transport pad cluster holds Undo, Redo, Play, Stop / Snap, Record, Spare, Spare. Controls respond directly to mouse and touch; MIDI setup is optional. Keep real lane/master meters, exact numeric entry, keyboard focus and the same detachable component. Protect legibility with a 1240×276 minimum scrollable surface. See [live desk operation and mapping](LIVE-DESK.md).

- **Lane controllers:** retain the reference's title strip, paired mute/solo buttons, LPF and pan dials, and horizontal volume slider in Alloy materials. EQ belongs to the desk. Preserve collapsible lane geometry; at 100 px show compact dials, at 124 px add captions, and at 164 px show numeric dial editors. Use the same locked control ownership and undo/automation paths as the desk.

- **Multiple displays:** offer the detachable live desk for mixed-size monitors, and full-workspace spanning across their shared visible band. Offset side-by-side displays share height; stacked displays share width. Keep F11 as a visible escape to windowed mode. Never spread controls into a desktop gap without a display.



- **Hexagonal dial:** pan, three-band EQ and clip gain. Relative vertical drag, Shift at drag start for fine adjustment, double-click/Home reset, editable value, focused wheel input and native slider accessibility. The scale follows the existing parameter range, including the zero reference for bipolar values. `AlloyDial` preserves the existing model callbacks and undo gestures.

- **Panel divider:** subtle vertical grip, visible hover/focus, horizontal drag. Arrow keys change width; Shift refines movement; double-click/Home restores default. Width persistence belongs to local workspace preferences, independent of a mix session.

- **Attract lighting:** explicit opt-in light show under View / Ctrl+Shift+A; Escape exits. Starts off and never persists with a mix or workspace. Reuse the existing UI refresh and one shared clock across docked/floating panels. Crossfade six changing scenes through dial halos, fader-edge LEDs, pad/button seams and panel edges. Bounded vector rings retain text and real peak meters; no blurred textures or synthetic meter values. Overlays accept no input or accessibility focus. Follow Windows motion preferences by default, with explicit Animated and Steady choices for this mode. This optional mode permits broader glows than the normal quiet interface.

- **Tabs:** quiet surface and copper underline for the selected page, with a separate focus outline.

- **Linear sliders:** narrow copper track and small metal thumb. Keep time and volume controls linear where direct positional comparison matters.

- **Timeline:** subdued clip backgrounds, eight track colors, near-white playhead with copper marker. Detail mode uses a bright RMS envelope beneath faint maximum peaks; peak-only mode remains available. Leave 12% headroom and use a common scale within each clip view. Display never changes audio.

- **Audio slip handle:** a small outlined hexagon with a drawn horizontal double arrow at the right of each visible clip header. Pin it inside the visible horizontal clip region so long tracks remain reachable. Copper indicates a nonzero offset; protected tracks use muted ink. Keep it distinct from frame, fade and paired-swap handles. The waveform and source ticks move; automation and clip/frame geometry stay fixed.

- **Tempo strip:** a fixed 22 px footer uses small copper hourglass pins and plain 11 px BPM values, following the supplied MixMeister reference. Keep one baseline and a quiet surface, without a graph or connecting line. Horizontal drag changes time; clicking a number opens precise editing. The existing tempo map still supports ramps. `TempoStrip` owns marker selection, geometry and hit regions; viewport changes update its alignment immediately. Keep labels tied to their painted bounds, suppress crowded unselected values and protect the first marker at zero.

- **Automation:** keep curves and nodes on the timeline without revealing an editing strip. Mix / Automation and the timeline context menu share Read, point insertion, Clear and VST selection; advanced Off/Touch/Latch and arm choices live in a Recording options submenu. The live desk retains the simple Record / Play / Stop workflow. Preserve locks, Undo and current curve ownership.



## Do's and Don'ts



- Do preserve the hexagonal dial silhouette and fixed readable scale.

- Do reserve bright accents for values, selection and useful status.

- Do keep library and inspector widths independent and preserve timeline space.

- Do retain numeric entry, keyboard focus and existing audio parameter ownership.

- Keep steady heat tied to control values and meter levels. Optional forge particles and the waveform seam require the saved View toggle and the selected motion policy; keep them bounded and preserve waveform detail, hit regions and fault colours.

- Don't treat the concept board as a promise of every illustrated control or a finished logo asset pack.





## Timeline navigation and lane sizing (0.49)



Transport, BPM, snap and zoom share the upper toolbar between Waveform and Audio / MIDI. At narrow widths, less frequent file/edit buttons remain available in their menus. The freed centre row is a full-mix navigator: four coloured source strips, bar/time labels and a copper rectangle representing the current main timeline view. Drag the rectangle or click to centre it; double-click fits the mix. Arrow keys pan, Home/End reach the ends and +/- zoom. Preserve modified keyboard shortcuts.



The detailed bar/time ruler and label row are independent fixed components above the scrolling lane canvas, using the same horizontal position and tempo map. Tempo pins retain their fixed footer. Four default lanes share the available vertical space with no timeline scrollbars. Lane boundaries and track-header bottom edges resize individual lanes between a six-pixel line and twice their responsive default height. Double-click or keyboard Home resets a lane; Up/Down resize and End doubles it. Scroll vertically only when the content exceeds the available height. Lane sizes persist in workspace preferences, independently of musical sessions and Undo. The controller/transition tool content retains its own scrolling.



Waveform column intervals and rendered vertices use the same pixel centres as rounded grid ticks; this is a drawing correction only. Never move source clocks, detected anchors, prepared PCM or playback to make a visual peak coincide with a grid line.


### 0.53 timeline and transport extension

The lane dropdown exposes up to eight real mixer lanes while retaining four as the default. Keep occupied upper lanes scrollable when the visible count is reduced. Eight distinct muted lane accents identify the added controls. When both tool panels are detached, the timeline fills the available centre height and a horizontal grip directly beneath its fixed tempo footer adjusts its height; retain the docking tabs beneath it. Store the preferred height with the eight per-lane scales in workspace preferences. The global BPM follows the playback tempo map even if another marker is selected; retain marker editing while stopped. Windows Audio / MIDI exposes ASIO alongside existing backends without installing or automatically selecting a driver.


### 0.54 stereo output and forge effects

A dedicated 116 px Alloy peak panel, with an 8 px gap, sits between the timeline and inspector. WorkspaceLayout owns its bounds from the overview top to the fixed tempo footer bottom; it follows timeline/window height and reserves horizontal space from the navigator, ruler and lane viewport together. Preserve 620 px for the original centre where the window permits it. L/R rails show sample peaks in dBFS (-48 to 0), with independent 1.2-second peak holds, falling levels, and explicit red clipping with a native reset button. The existing master gain control moves below the rails as a rotary control; when the timeline collapses below 240 px, it returns to the inspector row to stay usable.

The audio callback publishes maximum absolute L/R samples after master processing and before monitor mute. Accumulate maxima between UI frames; offline rendering/export/rehearsal preparation never feeds the live display. The display owns decay, hold and animation; the feed owns transient capture. No audio changes are introduced.

All meter fills and enabled slider/dial caps use the shared value-dependent metal palette: burnt copper at minimum, orange when warm, pale molten metal near maximum. Keep numeric values, hex cap shape, focus and separate red clipping readable. Bipolar controls heat toward their numeric maximum.

View / Molten sparks / forge playhead is an optional, saved workspace preference, off by default and outside musical sessions and Undo. Forge motion defaults to Follow Windows, with explicit Animated and Steady choices; an explicit Animated choice never changes the Windows preference. It permits occasional bounded droplets near meter peaks and a luminous seam at the actual playing cursor while two audible, ready clip spans overlap. Reuse the already-built waveform paths and transition cut boundaries; never reshape source data or move a clock. The trail is local to the cursor, with a warm waveform edge, small molten pool and vector droplets. Skip stopped transport, source/rehearsal preview, offscreen work and reduced motion under the default policy. Use the existing 25 Hz UI tick and at most 120 cells per meter rail; no new animation timers, bitmap generation or audio-thread drawing.


### 0.54.4 transition controls

Transition editing uses two compact top rows for new-track defaults/navigation and selected length/preset/lock, followed by four band columns (two at compact widths), then paired filter controls. Remove repeated track names, key paragraphs and long instruction blocks; retain context/shortcut tooltips. A single Rehearsal & presets disclosure keeps A/B/C and preset management available without occupying the default view. Playlist rows place the incoming-pair preset picker beside the track name and preserve user column sizing. Source-energy hints are numbered, explicitly tentative strips. Their display toggle leaves the grid/audio unchanged; the 0.54.7 Sections insertion mode explicitly consumes them for new-track placement.


### 0.54.7 automation captions and section defaults

Render a percentage caption only beside the selected automation node, retaining other dots and curves. Selection survives mouse release, transfers on a new node and clears on background clicks or a replaced project. Keep the exact AutomationAddress with the selected point, and resolve fresh curves after transactional edits. Gain captions use linear amplitude, LPF uses its canonical range, and precise editors retain physical units. The default new-track length is Sections 8–64 bars, with both-track boundary matches preferred over one-track matches. These tentative retained RMS hints need no new detection pass. Live additions preserve established voices and append future BPM changes only after their protected clock horizon.


### 0.55 menus and automatic blends

App-owned alerts, menus, tooltips and file pickers share Alloy's Segoe UI typography, panel colours, borders and orange selection. The application owns the fallback LookAndFeel for detached and unassociated pop-ups; Workstation retains its local controls' theme. Use app-drawn file pickers for consistent styling. File names use 12 px text, size/date 11 px muted text; compact rows hide metadata before crowding names. Preserve keyboard and file operations through JUCE's browser.

Auto Arrange offers Balanced (default), Smooth, Bold and Use my settings. Automatic overlaps use existing source-energy section hints with an explicit 8–32 bar cap; manual insertion's 8–64 bar default stays unchanged. Balanced combines bass handovers and gentle paired filter sweeps when measured overlap energy changes; Bold adds tighter handovers, stronger sweeps and mid/high fades. Smooth uses volume fades. Missing, malformed, silent or incomplete measured windows fall back to volume fades. Treat RMS as energy only; do not imply vocal, instrument or verse/chorus recognition. Saved transitions use existing session treatments and are never regenerated on open. Successful generation enables animated attract lighting and forge effects through existing UI owners; Undo remains musical, and Escape still exits attract lighting.

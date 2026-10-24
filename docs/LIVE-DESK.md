# Alloy live desk

**Performance mode is planned, not delivered:** see the [dedicated plan](PERFORMANCE-MODE-PLAN.md). The existing live controls and automation Record below are working foundations. Current source/grid and A/B/C rehearsal use the main monitor path; they are not private headphone preparation while the audience mix continues. The new plan includes track/segment and whole-transition private editing, explicit lower-panel targets, separate outputs, deliberate live commit and consolidation of the paused 0.43 cue prototype. The confirmed plan adds independently switchable programme audio and editable performance-event recording, including continuous controls and live actions while excluding private preparation. Product decisions are settled in the plan (user-confirmed D01–D07; delegated defaults D08–D18); implementation has not started. These extensions are planned; the existing automation Record described below retains its current limits.



The lower workspace is a live performance surface styled from the approved Alloy concept: graphite chassis, copper seam, hexagonal EQ pots, metal fader caps, lane colours and illuminated pad edges. The five meters show actual lane/master peaks.



## Attract lighting



Choose **View → Attract lighting**, or press **Ctrl+Shift+A**, to start the hardware-style light show. Press **Escape**, use the shortcut again or untick the menu item to stop. It starts off on each launch.



Scanner sweeps, control chases, expanding rings, pad-bank patterns, vertical cascades and breathing lights crossfade in a changing sequence. Dial halos, fader-edge LEDs, button seams and panel borders light up across the workspace and both detached tool panels. The effects leave labels, waveforms and real meter readings visible; they do not change audio, parameters, automation, transport or MIDI output. Controls remain usable.



**View → Attract motion** offers **Follow Windows** (default), **Animated**, or **Steady**. Follow Windows uses steady copper illumination when Windows interface animation is disabled. Choose Animated to run the show explicitly without changing Windows settings. These choices last for the current launch.



## Live controls (0.50)



Mouse and touch operate immediately; no lane arming or MIDI setup is required. A direct move outside recording disables Read for that parameter while keeping its curve points. Undo restores the previous state; selecting a node and enabling **Mix / Automation / Read selected curve** resumes it. Existing transition locks still apply.



Each adjacent pair of pots controls one function: **Bass A/B, Mid A/B, Treble A/B, Cutoff A/B**. Each pot has its own lane selection. Its matching top pad cycles through the three lanes its partner has not selected. The label and lane colour show the current target. Switching lanes never copies a value or changes audio. Two lanes per function are available simultaneously, and all four are reachable. Selections start at lanes 1 and 2 each launch; they are controller routing, outside musical Undo and session data.



| Hardware | Live desk action |

| --- | --- |

| Pots 1–2 | Bass EQ A/B, independently selected lanes |

| Pots 3–4 | Mid EQ A/B, independently selected lanes |

| Pots 5–6 | Treble EQ A/B, independently selected lanes |

| Pots 7–8 | Cutoff A/B, independently selected lanes |

| Top pads 1–8 | Cycle the matching pot's lane, excluding its partner's lane |

| Bottom pads 1–4 | Mute lanes 1–4 |

| Bottom pads 5–8 | Solo lanes 1–4 |

| Faders 1–4 | Level, lanes 1–4 |

| Faders 5–8 | Pan, lanes 1–4 |

| Master fader | Master level |

| Small fader pads 1–4 | Reset each lane's three EQ bands |

| Small fader pads 5–8 | Reset each lane's cutoff to centre Off |

| Master small pad | Centre all lane pans |

| Small transport pads, first row | Spare, Spare, Undo, Redo |

| Small transport pads, second row | Play/pause, Stop, Record, Snap |



The lane header now contains mute, solo, cutoff, pan and horizontal volume in Alloy's graphite, copper and hexagonal-dial style. Collapsed lanes progressively hide controls; expanding a lane reveals numeric dial entry. EQ is on the live desk.



## DJ cutoff filter (0.55.6)

The cutoff dial is **Off at centre**. Turn left for a low-pass (remove highs), right for a high-pass (remove bass), each spanning 20 Hz–20 kHz with a logarithmic physical sweep. Double-click or Home returns to centre; mouse dragging has a centre detent and MIDI CC63/64 both mean Off. Text entry accepts an explicit side/frequency such as `LP 800 Hz`, `HP 1.2k`, or `Off`.

**Live ownership corrected in 0.55.12:** the lane-header cutoff and the Controller cutoff pots always control the selected **mixer lane**, including later segments reusing that lane and live instruments. Selecting an earlier, finished segment no longer redirects these knobs to that silent segment. The displayed LP/HP value and recorded Touch/Latch curve use the same lane owner for mouse and MIDI. Pot lane-selector labels still determine A/B routing; they do not automatically follow playback. Lane transition locks still protect changes.

Explicit segment cutoff automation/manual values remain separate, continue to process and are preserved in saved projects. For an older segment-specific setting, select that segment and use **Edit > Reset selected segment cutoff** to turn its manual cutoff Off and disable its Read curve while retaining points. This is undoable and leaves the lane cutoff separate. Segment Cutoff automation can still be edited through its timeline curve.

Both sides are two-pole Butterworth filters, with roughly 5 ms smoothing and a blend into dry near neutral. Independent states remain warm across centre. Exact neutral preserves dry samples once smoothing settles. Segment and lane controls share playback/WAV-export processing; generated transition filter treatments keep their existing controls.

Current session format **27** saves both directions and their automation. Formats 23–26 convert old low-pass Hz values and curves while preserving their linear-frequency trajectory; formats 1–22 default dry. New sessions require the new app. Analysis revision 9 and valid retained analysis/audio caches are reused. Existing low-pass MIDI action IDs load as aliases; no automatic profile replacement or controller programming occurs.



Lane controls follow the existing mixer ownership: a selected segment owns controls on its lane, otherwise the lane mixer owns them. The status says **CLIP** when applicable. Mute/solo remain lane switches. Locked transition segments retain their control locks; master, mute and solo remain available.



## Map the controller

The single toolbar controller/settings button confirms MIDI input state: **detected** means an available input, **connected** means its input is open and Alloy is listening, and **receiving** confirms a recent control or note message. The tooltip lists each port, the active mapping count and the exact profile path. Clock and active-sensing traffic do not trigger the receiving confirmation. Click the button to open **Audio / MIDI**; this is its label when no input is detected. Long input names are abbreviated, with full names and all input ports in the tooltip. The additional-port count includes separate inputs from the same device.

The live desk header shows the title of the active in-memory mapping. Legacy profiles show **Unnamed mapping**; an empty untitled profile shows **Default / no mappings**. A loaded title stays with its assignments through automatic saves and changes only after a successful profile load. Editing a saved file alone does not change the active title or assignments. Full naming/import/export/profile management remains planned under P08.

If the active layout differs from the saved profile, choose **Reload saved mappings** in Audio / MIDI. Alloy validates the complete file before replacing active assignments; unreadable, invalid or conflicting entries leave the active layout intact and show an error. Reload cancels Learn and discards controls queued under the previous layout. It does not rewrite the profile or controller, change the mix, or clear analysis/audio caches. Finish Record or export first. This loads the saved assignments; it does not infer a new hardware layout.

In 0.55.4, Reload reports success/failure and the byte count/SHA-256 of the file actually read. **Clear mappings** backs up the existing saved file, then empties active and saved learned assignments and cancels Learn. If the backup/save fails, active assignments stay intact. Finish Record or export first. Standard MIDI note input and realtime transport remain separate from learned assignments; the controller itself is not rewritten or reset.

The **receiving** label is on the top-right controller/settings button and lasts briefly after a control/note message; its tooltip retains **received earlier**. On 20 October, the user confirmed receipt and old fader-5-to-Mid-1 routing only after closing FL Studio and toggling the FLkey input off/on. Closing FL Studio alone did not restore receipt. This establishes that reopening the input helped, not the original reason it stopped. The running 0.55.3 still held 39 real assignments despite an externally emptied saved file and a normal restart; the origin of that old profile remains unresolved. The reset's original 47-entry profile and historical backups are preserved independently under `.local/flkey-diagnosis-20261020/`. A controlled fresh 0.55.3 startup subsequently confirmed zero, and the delivered 0.55.4 was verified with zero active/saved assignments. The original cause of the old-profile return remains unresolved; do not restore/remap the cleared profile without user direction.



1. Enable the FLkey input in **Audio / MIDI**. Choose **Custom Mode** for its pots, pads, faders and buttons. Novation Components configures the actual messages; Alloy does not overwrite the controller's firmware or Custom Mode.

2. **FLkey guided setup** in Audio/MIDI now teaches the current paired live desk and resumes only its unmapped actions. **Map live desk** on the controller runs the full current walkthrough, deliberately replacing each control/action assignment when you move that hardware control. Neither entry point teaches the obsolete Mid-fader layout. Move only the named control. **Skip** advances past an unavailable button without changing its mapping and is enabled only during a walkthrough. Repeated events from one moving fader cannot fill subsequent steps; already-used controls show an explanation. Standard MIDI Play/Stop already work, so the complete walkthrough has 46 learnable controls.

3. To replace one assignment, enable **MIDI Learn** on the desk, click a button/pad or move an on-screen pot/fader, then operate the corresponding physical control. The first message assigns it without operating the mixer. New desk pots map to `desk-pot-1` through `desk-pot-8`, so their hardware follows the same lane selectors. Existing fixed-lane bass/mid/high/pan/level mappings remain valid and are never automatically replaced. Use the walkthrough only when you want to adopt the new layout. **Cancel learn** ends learning. Existing mappings save with backups; assigning an action replaces its previous mapping and assigning a physical control replaces that control's previous action.

4. Dedicated transport/navigation buttons can be learned when they send MIDI CC or note messages. FLkey's standard MIDI Start/Continue/Stop already operate Play/Stop without an assignment. Choosing Play or Stop in individual Learn recognises those messages and explains that no binding is needed, without triggering transport during capture. Other standard transport messages are held while waiting for a learning target. Enable the appropriate input if necessary. Alloy does not claim FL Studio's proprietary integration, display feedback or pad-light feedback.

Since 0.55.5 the desk and settings share one Learn state. Cancelling in either exits pending learning everywhere, including a walkthrough; Reload/Clear also exit it. Settings Learn captures one assignment and returns to normal operation. The desk's MIDI Learn mode lets you choose successive on-screen targets until cancelled. **Last MIDI** in settings shows the most recent received control/note/transport message independently of assignments. The profile hash is explicitly the **last profile read**, not a claim that later automatic saves have identical bytes.

The 20 October input-loss check captured zero messages through a separate Windows monitor even after input off/on. USB reconnection restored 161 messages, and reopening Alloy's input restored the same five mappings without relearning. The user then intentionally cleared those mappings; their independent backup remains retained. The underlying USB/driver trigger is still unestablished. Novation driver 2.29.0.77 is installed; no driver, power policy or Custom Mode was changed. The FLKEY onboarding drive is present; its causal role has not been tested. [Novation documents how to disable Easy Start storage](https://support.novationmusic.com/hc/en-gb/articles/360014582460-Stop-your-Novation-device-appearing-as-a-Mass-Storage-Device), but that procedure has not been applied here.



Plugin macros remain available as `plugin-parameter-1` through `plugin-parameter-8` in Audio / MIDI. Assign their named VST parameters in **Effects / MIDI controls** and map them to a different Custom Mode or chosen hardware controls. Keys, pitch bend, modulation and sustain continue to the selected VST instrument.

### FLkey right-hand buttons

The eight small on-screen buttons now match the FLkey's physical two-row arrangement. Quantise and Metronome remain spare; Undo, Redo, Play, Stop and Record keep their named functions; the physical **Score Log** button is intended to operate **Snap** in Alloy. Moving an on-screen button moves its label and click handler together. It does not change the MIDI messages emitted by physical hardware.

These dedicated buttons are separate from the 16 main Custom pads and the nine fader buttons. On 19 October the user physically checked all six active transport buttons after completing the 354-track scan. The main **FLkey MIDI** port supplied channel 16 / CC76 (Undo), CC77 (Redo), CC117 (Record) and CC118 (Score Log / Snap), each with 127 on press and 0 on release. Play and Stop supplied MIDI realtime Start and Stop. The historical 47-entry paired Alloy profile includes all four CC assignments; existing realtime handling supplies Play/Stop. Those learned assignments were backed up and cleared at the user's request on 20 October. This captured configuration is retained as diagnostic evidence; do not reinstall it without user direction.

For a different keyboard configuration, use individual MIDI Learn: click the on-screen Undo, Redo, Record or Snap action, then press/release the matching physical Undo, Redo, Record or Score Log button. Learning captures the assignment without firing the action; turn Learn off afterwards. Existing pot/fader/main-pad assignments remain. If no message appears, check the separate DAW input before assuming an assignment exists. Do not guess CC numbers. Hardware message capture and successful operation against an activated app profile are distinct checks.

Physical layout reference: [Novation FLkey 49 hardware overview](https://userguides.novationmusic.com/hc/en-gb/articles/23741692683922-FLkey-49-Hardware-Overview).



Hardware reference: [Novation FLkey 49 Custom Modes and Components](https://userguides.novationmusic.com/hc/en-gb/articles/23741692855826-Using-FLkey-49-s-Custom-Modes-and-Components). The supplied layout image establishes physical control positions, not factory MIDI message assignments.



## Record a mix performance



Click **Record**, then **Play**. Record arms all four lanes and master in Touch mode. Move level, pan, EQ, cutoff, master or mapped plugin controls. Touch returns to the underlying curve after release; Click **Record** again to finish recording while playback continues. **Stop** finishes the take, rewinds and returns to Read. Recording a take creates one undo snapshot when the first control is moved.



Save the session to preserve the curves, then **Export WAV** to render the performed automation. This records automation, not a separate live audio file. Mute/solo are session switches and are not recorded as timed events; use level faders for recorded cuts. Keyboard notes are live instrument input and are not captured as a MIDI performance. Existing automation in Read mode still controls playback; choose Record to punch in new moves.



## Two monitors and smaller windows



The lower **Controller / Transitions / Editing** tabs share one workspace area. Selecting a transition frame opens **Transitions**. Segment properties live in **Editing**; the right side keeps the VST3 chain, a **Keyboard** toggle and **Audio / MIDI...** setup access. Transition defaults, selected-pair controls, bands, filters, presets and A/B/C rehearsal are grouped in the new panel; scroll to reach additional controls in smaller windows.



Use the hexagonal icon at the upper right of either panel to detach it into its own resizable native window, preferably on the other display. The icon changes to attach while detached; clicking it or closing the window returns the same controls to the workspace. Detaching one panel reveals the other. Both can be detached independently; their workspace tabs bring their windows forward, and the timeline gains vertical space when both are detached. Engine, selection, playback, recording and Undo history remain shared.



**Display / Span monitors** spreads the complete workspace over the displays' shared visible area. A vertical offset between side-by-side screens uses their common height, so the menus remain visible on both; stacked screens use their common width. **F11** returns to a normal window. Use the detached desk for arrangements with desktop gaps or no sufficiently large shared band. Spanning never places controls in a gap that has no monitor. Monitor topology and window placement are read from JUCE logical display coordinates, respecting Windows scaling.



The desk fills the available lower centre. When the centre is narrower than 1240 logical pixels, its horizontal scrollbar keeps all controls accessible; detaching provides a wider live surface. Pots retain fine Shift-drag and exact numeric entry. Faders support exact entry and double-click reset. Native focus and slider accessibility remain available.


## Timeline automation menus (0.51)

Selecting or dragging a node never opens an automation strip. Curve Read, point insertion, clearing and VST target selection are in **Mix / Automation**, also available in the timeline right-click menu. Advanced recording modes and individual arming are under **Recording options**. The live desk remains the ordinary Record / Play / Stop workflow.

Alloy 0.54.2 returns to four mixer lanes. Each top pad cycles the three lanes other than its paired pot's lane. The existing FLkey profile remains valid; no keyboard remapping is needed. All four timeline lanes and their controls stay visible. The bottom timeline grip works with either panel docked or detached; docking limits expansion to preserve panel space. Individual lane resizing redistributes the available height instead of creating vertical scrolling.


## Tempo when adding tracks during playback

Later incoming tracks now receive their own source-BPM targets instead of copying the previous track's BPM indefinitely. A live addition preserves all existing voices, prepared audio and their clock through the end of established audio and any later existing caller tempo marker. Its new ramp starts after that protected horizon, within the incoming track's remaining audio. If no tail remains beyond the horizon, the existing clock stays in force. Stopped placement uses the ordinary incoming grid target.

Opening an older mix does not rewrite its saved tempo edits or repair flat targets already saved there. Use explicit Beatmatch while stopped to rebuild an existing arrangement, or edit its tempo markers. This update reuses existing analysis and caches; no rescan is required.


## Named MIDI profiles (0.55.8)

Open **Audio / MIDI** from the toolbar controller indicator. It reports detected, connected and receiving separately; seeing a device name alone does not prove that messages are arriving. While the audio device is running, polling releases disappeared input handles and reconnects previously enabled ports. A unique exact name can reconnect a changed Windows identifier. New primary FLkey inputs open automatically; DAW/secondary ports remain opt-in. A manually disabled input stays disabled through reconnection in the current run. This does not install drivers or send controller configuration.

Completed Learn steps autosave to the startup snapshot shown as **Profile** in the status text. The snapshot restores the active assignments and name when the app reopens. Named profiles are separate copies in the sibling **MIDI Profiles** folder. Changes to a named file on disk do not silently replace active routing.

- Type a name, then **Name active** to apply it. Typing or choosing a list item alone changes nothing.
- **Save as** creates a distinct named copy and makes it active. Duplicate names are rejected. **Save profile** updates the active named copy with a backup; Learn does not overwrite it automatically.
- Choose a named profile and click **Open profile** to validate and activate its disk contents. **Reload saved mappings** instead reloads the startup snapshot. Successful loads cancel Learn and discard queued controls from the earlier layout.
- **Import JSON** validates a portable profile and activates a copy in the startup snapshot. Its original stays unchanged; use Save as for a named copy. **Export JSON** writes active assignments/name to a separate `.json` destination without changing active identity. Cancelling either picker changes nothing.
- **Delete active** archives the active named file as a `.deleted.bak` sibling. Active assignments, name and startup snapshot remain, including on reopening. **Clear mappings** separately clears active/startup assignments and cancels Learn; named copies remain available.

Status distinguishes matching, changed and missing named copies. Failed parsing, duplicate physical/action assignments and failed activation writes retain active routing. Individual Learn replaces only the chosen action and physical-control conflicts; guided setup preserves mapped actions unless the full replacement walkthrough was chosen. Cancel retains completed steps. Profile operations are blocked during offline audio export or an active automation take and do not enter musical Undo. Write errors are shown in profile status; do not assume an automatic save succeeded when an error is visible.

Physical USB reconnect and real controller response require normal-use acceptance; native port-policy fixtures do not prove driver behaviour. Existing FLkey desk routing and standard realtime Play/Stop remain unchanged. No old mappings are automatically restored.

Control hover help and native accessible names identify the current segment (number, song and lane) or mixer-lane owner. Volume/Pan/EQ follow the existing selected-segment rule; Cutoff always names the mixer lane and Master names master. Clearing segment selection restores lane descriptions. This clarifies current routing; it does not change it.

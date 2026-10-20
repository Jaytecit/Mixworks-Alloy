# Alloy live desk



The lower workspace is a live performance surface styled from the approved Alloy concept: graphite chassis, copper seam, hexagonal EQ pots, metal fader caps, lane colours and illuminated pad edges. The five meters show actual lane/master peaks.



## Attract lighting



Choose **View → Attract lighting**, or press **Ctrl+Shift+A**, to start the hardware-style light show. Press **Escape**, use the shortcut again or untick the menu item to stop. It starts off on each launch.



Scanner sweeps, control chases, expanding rings, pad-bank patterns, vertical cascades and breathing lights crossfade in a changing sequence. Dial halos, fader-edge LEDs, button seams and panel borders light up across the workspace and both detached tool panels. The effects leave labels, waveforms and real meter readings visible; they do not change audio, parameters, automation, transport or MIDI output. Controls remain usable.



**View → Attract motion** offers **Follow Windows** (default), **Animated**, or **Steady**. Follow Windows uses steady copper illumination when Windows interface animation is disabled. Choose Animated to run the show explicitly without changing Windows settings. These choices last for the current launch.



## Live controls (0.50)



Mouse and touch operate immediately; no lane arming or MIDI setup is required. A direct move outside recording disables Read for that parameter while keeping its curve points. Undo restores the previous state; selecting a node and enabling **Mix / Automation / Read selected curve** resumes it. Existing transition locks still apply.



Each adjacent pair of pots controls one function: **Bass A/B, Mid A/B, High A/B, LPF A/B**. Each pot has its own lane selection. Its matching top pad cycles through the three lanes its partner has not selected. The label and lane colour show the current target. Switching lanes never copies a value or changes audio. Two lanes per function are available simultaneously, and all four are reachable. Selections start at lanes 1 and 2 each launch; they are controller routing, outside musical Undo and session data.



| Hardware | Live desk action |

| --- | --- |

| Pots 1–2 | Bass EQ A/B, independently selected lanes |

| Pots 3–4 | Mid EQ A/B, independently selected lanes |

| Pots 5–6 | High EQ A/B, independently selected lanes |

| Pots 7–8 | Low-pass A/B, independently selected lanes |

| Top pads 1–8 | Cycle the matching pot's lane, excluding its partner's lane |

| Bottom pads 1–4 | Mute lanes 1–4 |

| Bottom pads 5–8 | Solo lanes 1–4 |

| Faders 1–4 | Level, lanes 1–4 |

| Faders 5–8 | Pan, lanes 1–4 |

| Master fader | Master level |

| Small fader pads 1–4 | Reset each lane's three EQ bands |

| Small fader pads 5–8 | Open each lane's low-pass filter |

| Master small pad | Centre all lane pans |

| Small transport pads, first row | Spare, Spare, Undo, Redo |

| Small transport pads, second row | Play/pause, Stop, Record, Snap |



The lane header now contains mute, solo, low-pass, pan and horizontal volume in Alloy's graphite, copper and hexagonal-dial style. Collapsed lanes progressively hide controls; expanding a lane reveals numeric dial entry. EQ is on the live desk.



The low-pass range is 20 Hz–20 kHz with a logarithmic control sweep. A two-pole Butterworth filter smooths cutoff changes over approximately 5 ms. Fully open is exact dry bypass; the final 1 kHz of travel blends toward bypass. Clip and lane controls share the same implementation in playback and WAV export. Filter cutoff and its automation were introduced in session format 23 and remain in current format 25. Existing sessions (1–22) default to an open filter; analysis revision 9 and all retained analysis/audio caches are unchanged.



Lane controls follow the existing mixer ownership: a selected clip owns controls on its lane, otherwise the lane mixer owns them. The status says **CLIP** when applicable. Mute/solo remain lane switches. Locked transition clips retain their control locks; master, mute and solo remain available.



## Map the controller

The bottom status bar confirms MIDI input state: **detected** means an available input, **connected** means its input is open and Alloy is listening, and **receiving** confirms a recent control or note message. The tooltip lists each port, the active mapping count and the exact profile path. Clock and active-sensing traffic do not trigger the receiving confirmation. Click the indicator to open **Audio / MIDI**.

If the active layout differs from the saved profile, choose **Reload saved mappings** in Audio / MIDI. Alloy validates the complete file before replacing active assignments; unreadable, invalid or conflicting entries leave the active layout intact and show an error. Reload cancels Learn and discards controls queued under the previous layout. It does not rewrite the profile or controller, change the mix, or clear analysis/audio caches. Finish Record or export first. This loads the saved assignments; it does not infer a new hardware layout.



1. Enable the FLkey input in **Audio / MIDI**. Choose **Custom Mode** for its pots, pads, faders and buttons. Novation Components configures the actual messages; Alloy does not overwrite the controller's firmware or Custom Mode.

2. Enable **MIDI Learn**, then choose **Map live desk** on the desk for the complete walkthrough, then move only the named control. Each move deliberately replaces that control/action assignment; other mappings remain intact. **Skip** advances past an unavailable button without changing its mapping. Repeated events from one moving fader cannot fill subsequent steps. The older **FLkey guided setup** in Audio / MIDI only fills unmapped actions and preserves existing assignments; it may leave your previous pan/plugin layout in place.

3. To replace one assignment, enable **MIDI Learn** on the desk, click a button/pad or move an on-screen pot/fader, then operate the corresponding physical control. The first message assigns it without operating the mixer. New desk pots map to `desk-pot-1` through `desk-pot-8`, so their hardware follows the same lane selectors. Existing fixed-lane bass/mid/high/pan/level mappings remain valid and are never automatically replaced. Use the walkthrough only when you want to adopt the new layout. **Cancel learn** ends learning. Existing mappings save with backups; assigning an action replaces its previous mapping and assigning a physical control replaces that control's previous action.

4. Dedicated transport/navigation buttons can be learned when they send MIDI CC or note messages. Some FLkey buttons use FL Studio's DAW integration; only learnable messages appear in Alloy's last-message display. Enable the appropriate input if necessary. Alloy also accepts MIDI Start/Continue/Stop. It does not claim FL Studio's proprietary integration, display feedback or pad-light feedback.



Plugin macros remain available as `plugin-parameter-1` through `plugin-parameter-8` in Audio / MIDI. Assign their named VST parameters in **Effects / MIDI controls** and map them to a different Custom Mode or chosen hardware controls. Keys, pitch bend, modulation and sustain continue to the selected VST instrument.

### FLkey right-hand buttons

The eight small on-screen buttons now match the FLkey's physical two-row arrangement. Quantise and Metronome remain spare; Undo, Redo, Play, Stop and Record keep their named functions; the physical **Score Log** button is intended to operate **Snap** in Alloy. Moving an on-screen button moves its label and click handler together. It does not change the MIDI messages emitted by physical hardware.

These dedicated buttons are separate from the 16 main Custom pads and the nine fader buttons. On 19 October the user physically checked all six active transport buttons after completing the 354-track scan. The main **FLkey MIDI** port supplied channel 16 / CC76 (Undo), CC77 (Redo), CC117 (Record) and CC118 (Score Log / Snap), each with 127 on press and 0 on release. Play and Stop supplied MIDI realtime Start and Stop. The updated 47-entry paired Alloy profile includes all four CC assignments; existing realtime handling supplies Play/Stop. No manual learning or DAW input is needed for this captured configuration after installing that updated profile with Alloy closed normally.

For a different keyboard configuration, use individual MIDI Learn: click the on-screen Undo, Redo, Record or Snap action, then press/release the matching physical Undo, Redo, Record or Score Log button. Learning captures the assignment without firing the action; turn Learn off afterwards. Existing pot/fader/main-pad assignments remain. If no message appears, check the separate DAW input before assuming an assignment exists. Do not guess CC numbers. Hardware message capture and successful operation against an activated app profile are distinct checks.

Physical layout reference: [Novation FLkey 49 hardware overview](https://userguides.novationmusic.com/hc/en-gb/articles/23741692683922-FLkey-49-Hardware-Overview).



Hardware reference: [Novation FLkey 49 Custom Modes and Components](https://userguides.novationmusic.com/hc/en-gb/articles/23741692855826-Using-FLkey-49-s-Custom-Modes-and-Components). The supplied layout image establishes physical control positions, not factory MIDI message assignments.



## Record a mix performance



Click **Record**, then **Play**. Record arms all four lanes and master in Touch mode. Move level, pan, EQ, low-pass, master or mapped plugin controls. Touch returns to the underlying curve after release; Click **Record** again to finish recording while playback continues. **Stop** finishes the take, rewinds and returns to Read. Recording a take creates one undo snapshot when the first control is moved.



Save the session to preserve the curves, then **Export WAV** to render the performed automation. This records automation, not a separate live audio file. Mute/solo are session switches and are not recorded as timed events; use level faders for recorded cuts. Keyboard notes are live instrument input and are not captured as a MIDI performance. Existing automation in Read mode still controls playback; choose Record to punch in new moves.



## Two monitors and smaller windows



The lower **Controller / Transitions** tabs share one workspace area. Selecting a transition frame opens **Transitions**, while Clip / Effects / MIDI remain independent in the right inspector. Transition defaults, selected-pair controls, bands, filters, presets and A/B/C rehearsal are grouped in the new panel; scroll to reach additional controls in smaller windows.



Use the hexagonal icon at the upper right of either panel to detach it into its own resizable native window, preferably on the other display. The icon changes to attach while detached; clicking it or closing the window returns the same controls to the workspace. Detaching one panel reveals the other. Both can be detached independently; their workspace tabs bring their windows forward, and the timeline gains vertical space when both are detached. Engine, selection, playback, recording and Undo history remain shared.



**Display / Span monitors** spreads the complete workspace over the displays' shared visible area. A vertical offset between side-by-side screens uses their common height, so the menus remain visible on both; stacked screens use their common width. **F11** returns to a normal window. Use the detached desk for arrangements with desktop gaps or no sufficiently large shared band. Spanning never places controls in a gap that has no monitor. Monitor topology and window placement are read from JUCE logical display coordinates, respecting Windows scaling.



The desk fills the available lower centre. When the centre is narrower than 1240 logical pixels, its horizontal scrollbar keeps all controls accessible; detaching provides a wider live surface. Pots retain fine Shift-drag and exact numeric entry. Faders support exact entry and double-click reset. Native focus and slider accessibility remain available.


## Timeline automation menus (0.51)

Selecting or dragging a node never opens an automation strip. Curve Read, point insertion, clearing and VST target selection are in **Mix / Automation**, also available in the timeline right-click menu. Advanced recording modes and individual arming are under **Recording options**. The live desk remains the ordinary Record / Play / Stop workflow.

Alloy 0.54.2 returns to four mixer lanes. Each top pad cycles the three lanes other than its paired pot's lane. The existing FLkey profile remains valid; no keyboard remapping is needed. All four timeline lanes and their controls stay visible. The bottom timeline grip works with either panel docked or detached; docking limits expansion to preserve panel space. Individual lane resizing redistributes the available height instead of creating vertical scrolling.


## Tempo when adding tracks during playback

Later incoming tracks now receive their own source-BPM targets instead of copying the previous track's BPM indefinitely. A live addition preserves all existing voices, prepared audio and their clock through the end of established audio and any later existing caller tempo marker. Its new ramp starts after that protected horizon, within the incoming track's remaining audio. If no tail remains beyond the horizon, the existing clock stays in force. Stopped placement uses the ordinary incoming grid target.

Opening an older mix does not rewrite its saved tempo edits or repair flat targets already saved there. Use explicit Beatmatch while stopped to rebuild an existing arrangement, or edit its tempo markers. This update reuses existing analysis and caches; no rescan is required.

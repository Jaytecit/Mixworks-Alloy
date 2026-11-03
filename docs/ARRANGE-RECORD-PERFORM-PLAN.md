# Arrange and Record/Perform implementation plan

> Implemented in 0.55.64, 24 October 2026, inline without subagents. Final focused checkpoint: 85 checks passed on the exact candidate, under the minimal-testing and data-preservation instructions. Hardware/listening, long-recording capacity and complete regressions remain deferred.

**Goal:** freely edit/save/export an arrangement, or preserve a live pass as an editable performance, with optional private headphones and master WAV capture.

**Architecture:** separate arrangement, performance take and private-preparation owners. Live replay and offline export use the same committed programme event/control path. Private preparation owns an independent transport and mutable DSP state; only explicit Apply/Launch changes the programme.

**Stack:** C++17/JUCE, current four-lane/session model, existing MIDI/automation, streaming preview, WAV exporter, isolated native/bridge fixtures.

This supersedes the separate top-level Performance mode presentation in [the earlier plan](PERFORMANCE-MODE-PLAN.md). Retain its settled output, preparation and launch decisions where consistent with the two modes below. Do not repeat the product interview. Private preparation includes complete transitions as well as tracks/segments; a routing selector alone is not completion.

## Product contract

| Behaviour | Arrange | Record/Perform |
| --- | --- | --- |
| Edit | Freely place, split, trim, move and automate; save/play the arrangement | Preserve programme actions during a pass in a new editable take |
| Play | Ordinary arrangement playback | Start/resume the take and programme playback |
| Stop | Existing programme transport semantics; stop master preview too | Finalise take/optional WAV before resetting programme transport |
| WAV | Offline export, no recording step required | Optional **Record master WAV**, capturing the performance as heard |
| Library preview | Master output; pause arrangement and keep its cursor | Explicit private headphone route; never silently use the audience output |
| Headphones | Not required | Optional different device or independent stereo pair on one interface |
| Arming | No everyday Arm lane controls | All lanes/master eligible by default; advanced exclusions only |

Entering a mode never starts playback, remaps MIDI or reconfigures devices. Switching away finalises active capture and stops private playback/cancels private launches; private edits remain for explicit Apply/Discard. Routing is a preference within Record/Perform, not a third mode. Recording automation works without headphones; private playback is disabled with an explanation until its route is configured. Configure/test routing while stopped. Device failure never substitutes the master.

Manual changes hold their last value by default (Latch); provide **Return to automation** for the selected control/owner, and Touch as an advanced option. Return records a deterministic punch boundary. Private selection and transport never change programme MIDI targeting.

Capture volume, EQ, filter, pan, mapped VST parameters and master controls, plus audible mute/solo, tempo, launches, hotcues, loops and transport jumps. Direct plugin-editor gestures also need capture; program/state changes must be recorded or explicitly identified as needing the optional master WAV. Capture MIDI notes/live instruments when supported and enabled; never advertise unfinished input recording. Nondeterministic plugins may require the WAV for an exact audible record.

Built-in and VST automation belong to the **originally placed track**, shared across surviving split pieces and deleted gaps. Separate placements of the same song remain independent. Piece trims/source fades remain local. Reset ownership for a new placement, not a split. Preserve natural tails, unrestricted four-lane overlap and explicit transition creation.

## Historical starting foundations and gaps in 0.55.63

The following is the pre-implementation baseline. Its “planned” statements are superseded by the completed0.55.64 checklist and acceptance below.

- `Automation.cpp` / `LiveAutomationTests.cpp`: Read/Touch/Latch capture, timestamped MIDI and Undo exist. They write arrangement curves directly, require arming and end writes on seek; they are not a complete performance take.
- `Model.h/.cpp`, `Engine.cpp`, `EffectTail.h`: session 40 shares VST ownership/curves across split pieces. Built-in volume/EQ/filter are still piece-owned and must join the same original-track owner.
- `LibraryPreview.cpp`: this session makes master audition the default and retains explicit private routing internally. Edited private transitions and same-device stereo pairs remain planned.
- `GridCorrectionUI.cpp`, `TransitionRehearsal.cpp`, `TransitionRehearsalUI.cpp`: existing audition owns master playback; transition preparation can silence the programme. Separate those owners before simultaneous private preparation.
- `PerformanceDeck.cpp`, `PerformanceUI.cpp`, `LiveHotcues.cpp`, `LaunchPause.cpp`: reuse commands and scheduling, recording actions when they become audible.
- `WavExport.cpp`: offline output exists. Live capture needs a bounded background writer, never file writes in the audio callback.

## Implementation tasks

### 1. Mode and take ownership

Files: `Model.h/.cpp`, `Engine.h/.cpp`, `Workstation.h/.cpp`, `WorkspaceUI.cpp`, `Automation.cpp`; new `PerformanceTake.h/.cpp`, focused tests, CMake/SelfTest registration.

- [x] Introduce the contracts below. Keep active mode/routing in workspace preferences and musical takes in the session. Stable owner IDs reference original placements, lanes or master.

```cpp
enum class WorkMode { arrange, recordPerform };
enum class TakeState { idle, recording, paused, finalising, complete };
// TakeEvent: monotonic performance-frame timestamp, tie-break sequence,
// event kind, owner ID, control/parameter ID, typed value/payload,
// programme source position. PerformanceTake owns the initial project/plugin
// snapshot and the ordered events. Its clock is independent of project seeks.
```

- [x] Play starts/resumes a take without replacing the original arrangement. Use monotonic performance time so repeated/looped source passages can have different controls. Pause preserves elapsed performance time, including silence/tails; Stop finalises. Save/Discard are explicit; Undo removes the take as one operation.
- [x] Add two modes and persistent programme/private target and take status to existing transport. Move arming to advanced exclusions. Exiting finalises capture; retain private edits for Apply/Discard.

### 2. Shared automation and recorded gestures

Files: `Model.h/.cpp`, `Automation.cpp`, `AutomationUI.cpp`, `TimelineAutomation.cpp`, `Engine.cpp`, `TimelineEditing.cpp`, `EditSelection.cpp`, current plugin parameter listeners.

- [x] Extend original-track ownership to volume/EQ/filter/pan using existing shared VST owner/source offsets. Resolve node edits and live gestures through that owner. Keep geometry/fades local and retain ownership until its last piece is deleted.
- [x] Route GUI/MIDI/plugin gestures through one timestamped committed-control path. First movement begins a punch boundary; latch final value and record explicit Return. Replay suppresses listener feedback. Record mute/solo and tempo as typed events.
- [x] Record launches, hotcues, loops and seeks at their audible commit, including source positions and deterministic ordering. Private actions and failed/cancelled requests create no programme events.

### 3. Replay, save/reopen and export

Files: `PerformanceTake.cpp`, `Model.cpp`, `Engine.cpp`, `Automation.cpp`, `WavExport.cpp`, `Recovery.cpp`, focused performance tests.

- [x] Replay the initial snapshot and events on performance time; reconstruct state when seeking within a take. Keep the original arrangement independently selectable and unchanged.
- [x] Save take/owner references and bounded recovery checkpoints; increment the current session format once. No legacy compatibility layer. Preserve music, analysis/corrections, caches and downbeat reference evidence.
- [x] Offline WAV export selects arrangement or take and shares live event application/tail rules, including repeated passages and source jumps.

### 4. Private preparation and headphones

Files: `LibraryPreview.h/.cpp`, `Engine.h/.cpp`, `Workstation.cpp`, `PerformanceUI.cpp`, `GridCorrectionUI.cpp`, `TransitionRehearsal.cpp`, `TransitionRehearsalUI.cpp`; new `PrivatePreparation` state/render owner.

- [x] Copy a track/segment or whole transition into independent private transport/automation/plugin state; share only immutable PCM. Preparation cannot set programme exporting or change its playhead.
- [x] Add audience/private stereo routing, private level and Test headphones. Validate non-overlapping pairs on one device or explicit separate devices. Private-device failure stops private audio only.
- [x] Bind library preview to mode: master in Arrange, private in Record/Perform. Add independent private seek/loop/stop/cursor. Keep hardware/onscreen programme transport ownership clear.
- [x] Explicit Apply/Launch shows target and timing, rejects stale/unavailable targets, and commits through the programme event path. Retain private A/B/C transition comparison.

### 5. Optional live master WAV

Files: new `MasterRecorder.h/.cpp`, `Engine.h/.cpp`, transport UI, existing WAV utilities and performance tests.

- [x] Tap programme PCM after master processing and before local monitor-only mute. Exclude private audio and Arrange audition. Use a bounded FIFO and one background writer, the actual output rate/channels, and RF64 support for long captures.
- [x] Capture alongside the editable take; finalise off the callback after its final programme/tail samples. Report the saved path. On disk/full/FIFO errors keep the usable partial file, visibly mark incomplete recording and continue the programme/editable take. Never silently drop samples while claiming completion.

### 6. One focused acceptance checkpoint

- [x] Use tiny deterministic sources and the existing finite-delay VST. Check shared built-in curves across splits/gaps, independent duplicates and tails. Compare captured/replayed PCM for a fader change, cue jump and repeated loop with changed controls, including save/reopen and take export.
- [x] Fake output devices prove Arrange master audition, private/master separation, no headphone fallback and uninterrupted programme during private transition preparation. Verify writer failure leaves programme running and reports an incomplete WAV.
- [x] Batch one below-normal build with `_CL_=/MP1` and `cmake --build build --config Release --target Mixworks --parallel 1`. Run only relevant focused selectors through `verification.run_checks`, sequentially. Capture/inspect both modes. No full/stress/capacity runs unless separately requested.
- [x] Retain independent before/completion source archives, exact candidate hashes/reports/captures, then update HANDOVER and stage for user-controlled launch. Hardware/MIDI and listening acceptance remain separate from fake-device evidence.

## Delivered controls and limits

The mode selector and **Takes / preview** menu expose saved takes, audition,
cancellable reconstruction at performance time, event time/value editing or
removal, take WAV export and discard. Save retains the arrangement and takes;
the pre-take Undo checkpoint removes a pass as one operation. Mode and routing
are workspace preferences; takes and stable original-placement owners use
session format 41. Analysis revision 11 and PCM keys are unchanged.

`PerformanceCapture.cpp` records committed controls, direct VST parameter
gestures, Return, transport/source jumps and structural snapshots on an
independent sample clock. The original arrangement and independent plugin
state are restored when the take ends. Shared original-track built-in/VST
curves and DSP persist across surviving split pieces, gaps and natural tails.
Private preparation has its own renderer/transport, optional routing and
explicit Apply/Launch; its A/B/C comparison is not the old matched-RMS renderer.

Live MIDI instrument notes, opaque plugin program/state/topology changes and
nondeterministic plugins need the optional master WAV for an exact record.
They are not advertised as editable instrument capture; detectable unsupported
changes mark editable capture incomplete. A take holds at most 65,536 events
and 64 structural snapshots; a session holds at most 32 takes. Capture overflow
or writer failure stays visible while programme playback continues. The WAV
writer uses JUCE's RF64 support, but long RF64 recording was not capacity-tested.

Final `.local/r646/evidence` retains 51 performance checks, 22 preview checks
and 12 retry checks, frozen source/bundle/fixture hashes and inspected native
captures. Tiny sources and a real finite-delay VST establish capture/replay/
export equality, pause tails, ending-boundary hotcues, quantized private commit,
split ownership and fake four-channel separation. They do not establish
physical gestures, hardware routing, plugin determinism in general or musical
listening. No full/stress/capacity run was made. Independent completion evidence
and recovery checkpoint are in `.local/arp-20261024/`; see HANDOVER.

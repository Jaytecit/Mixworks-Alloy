# Controller, capture and private performance

Inherit RULES. F-MIDI/F-TAKE/F-PLUGIN; scopes midi/performance, Engine/Automation, data/output consumers. All mandatory, NOT RUN. Physical evidence is required wherever claimed.

### MW-P01 — Every MIDI action has correct range, edge and target
- Contract/source: Model midiActions/parseMidiAction and Engine MIDI dispatch, live desk lane selectors. Current FLkey preference: faders1–4 levels,5–8 pans,9 master; paired desk pots address chosen distinct lanes.
- Cases/procedure: enumerate all generated actions and invalid IDs, MIDI channels1–16, min/centre/max, repeated/held/release buttons, message types/clock/active sensing; test source owner after selection ends, lane selectors excluding partner, snaps/Undo/Redo/panic/reset and plugin assignment.
- Pass/oracle: independent mapping table predicts exact normalized/physical value and correct target; one button edge produces one action/Undo, transport matches mouse route. Pickup/latch semantics must be documented and tested if implemented; absence is not falsely advertised.
- Evidence/mode/severity: exhaustive generated-action list, raw input/expected/output state and PCM, desk captures; automated/native; P1.

### MW-P02 — Learn, profiles and active routing are transactional
- Contract/source: MidiConnection/Engine mapping parser/UI and docs/LIVE-DESK.md.
- Cases/procedure: guided/current/resumed/individual Learn, Cancel across both windows, skip/conflict, Save/SaveAs/Open/Import/Export/rename/delete/reload, malformed/oversized/duplicate bindings, unwritable profile and changed file while pending; record/export rejection paths.
- Pass/oracle: invalid profile keeps active mappings, success replaces complete validated set and clears pending old-layout/held/Learn state; saved path/title/count report actual active profile. Named profiles independent from startup autosave, deletes archived/backup as documented. Compare active dispatch, not only file hash.
- Evidence/mode/severity: before/after files+backups/active action results, cancellation/reopen and real settings interaction; automated + manual; P1.

### MW-P03 — Physical FLkey/USB lifecycle matches status and saved identity
- Contract/source: MidiConnection status/reconnect policy and historical stale-layout outage (cause unresolved).
- Cases/procedure: on an explicitly available test controller capture each intended physical fader/pot/pad/key; unplug/replug, app restart, competing-app ownership, persisted enable/disable, duplicate name/changed identifier where feasible. Never reprogram hardware or user mappings automatically.
- Pass/oracle: raw hardware messages and audible/UI targets match independent desk map; detected/open/receiving are distinct truthful states, clock alone not receiving; reconnect follows saved policy without replacing active map incorrectly. If original outage cannot reproduce, retain uncertainty rather than asserting root cause fixed.
- Evidence/mode/severity: physical device/firmware/port/raw MIDI timestamps, map hash AND active routing, hardware/listening notes; hardware/manual; P1. Mock PASS cannot close this rule.

### MW-P04 — Plugin MIDI and keyboard input respect host limitations
- Contract/source: PluginControl stable parameter IDs, piano/MidiKeyboardState, VST3 instrument route and take warning contract.
- Cases/procedure: note on/off/sustain/panic stopped and playing, physical/on-screen keyboard, source/lane selection changes, removed parameter/module, learn assignment persistence; optional live WAV while instrument plays.
- Pass/oracle: assigned parameter reaches actual plugin, no stuck notes after stop/panic/device loss, no false editable-note-capture claim; master WAV captures audible notes when enabled. Parameter names/index changes reject or disclose invalid assignment, not silently retarget another control.
- Evidence/mode/severity: native instrument PCM plus physical note route and saved binding check; automated + hardware/listening; P1.

### MW-P05 — Read/Touch/Write and arming record exactly the intended control scope
- Contract/source: AutomationRecordMode/canWriteAutomation/activeWrites, Automation.cpp and UI recording options; enumerate actual modes, not a fictional Latch API.
- Cases/procedure: every built-in/master/plugin live address, armed/unarmed × Read/Touch/Write × stopped/playing/export/rehearsal/Perform, no-touch versus drag/release/Return, held MIDI and direct plugin editor gestures; cancel mode switch/reopen.
- Pass/oracle: only permitted states write; no invented points during Read/selection, touches release/return by documented semantics, timestamps use audible sample clock; exactly owned curves with one logical Undo, shared original settings across pieces/gaps and separate placements independent.
- Evidence/mode/severity: authored event schedule versus captured nodes/effective PCM, all finite mode/admission combinations and real gestures; automated/manual; P1.

### MW-P06 — Quantized pause and hotcue execute once on the sample boundary
- Contract/source: LaunchClock/LaunchPause/LiveHotcues/Preroll; timing immediate/nextBeat/nextBar/nextPhrase and docs/LAUNCH-TIMING.md.
- Cases/procedure: all timings/phrase lengths, ramp tempo, request immediately before/on/after boundary, odd blocks, ending-boundary cue, preroll0/1/2/4; cancel via seek/Stop/grid/device/exclusive operation or missed boundary.
- Pass/oracle: independent integrated sample-clock boundary within1 sample; programme clock continues for source jump, scheduled pause holds cursor and completes current take before restart. Exactly one action, cancelled/missed action never rolls forward silently; no pending action restored on reopen.
- Evidence/mode/severity: request/decision/actual sample indices and output attacks, save/Undo of settings and real cue controls; automated + listening/manual; P1.

### MW-P07 — A performance take preserves the arrangement and a distinct elapsed clock
- Contract/source: PerformanceCapture/PerformanceTake WorkMode/TakeState and docs/ARRANGE-RECORD-PERFORM-PLAN.md.
- Cases/procedure: Arrange versus Perform Play/Pause/Resume/Stop/natural end/device loss; source seek/repeat/loop/launch while capture clock advances, structural change/direct VST/Return/mute/solo/tempo; full32-take refusal and event/snapshot boundaries through synthetic bounded fixtures.
- Pass/oracle: original musical/plugin state restored independently, ordered sequence/frame events with no clock drift across block sizes; held pause represented as silence/effect tails on take clock. Limits32 takes/65,536 events/64 snapshots enforced with explicit incomplete/refusal outcome, never silent success/drop. No destructive overflow/stress run needed to construct boundary data.
- Evidence/mode/severity: original versus restored state/plugin hash, hand-authored event/sample schedule, limit-case result and captured take JSON; automated/native; P1.

### MW-P08 — Replay, event editing and reconstruction reproduce the supported take
- Contract/source: Engine renderTake/prepareTakeReplay, PerformanceModeUI event controls and WavExport take path.
- Cases/procedure: deterministic known control/structural sequence, equal-frame events, edit time/value/remove, reordered invalid/nonfinite event, seek near beginning/mid/end with delay history and cancel reconstruction; reopen and replay/export.
- Pass/oracle: live supported deterministic PCM versus replay/export matches float2e-6 plus PCM24 quantization; reconstruction at target matches sequential take history, elapsed duration exact frames, event ordering stable and unrelated takes/arrangement unchanged. Unsupported note/opaque/nondeterministic changes marked incomplete or explained master-WAV requirement, never claimed exact editable replay.
- Evidence/mode/severity: live/reference/replay null metrics, event/duration tables, cancel/state/reopen and actual UI; automated + listening/manual; P1.

### MW-P09 — Private preparation cannot reach audience channels accidentally
- Contract/source: PrivatePreparation/Engine private channel routing, stopped routing configuration and explicit no-fallback contract.
- Cases/procedure: programme1–2 versus independent stereo pair≥3 or separate device; absent/failing/overlapping route, disconnect during preview, monitor mute, private level0/max, record/export/audience playback concurrently as allowed.
- Pass/oracle: independently tagged private signal exactly absent from programme/export/live WAV within digital silence, while correct private route audible; invalid/failing route refuses and never falls back to1–2. Original programme PCM/transport/selection/MIDI target unaffected. Physical separation independently monitored with safe levels.
- Evidence/mode/severity: all channel captures and device matrix, route configuration, actual listening/hardware isolation; automated + hardware; P0.

### MW-P10 — Apply/Launch commits only the prepared target and policy
- Contract/source: PrivatePreparation queueProgrammeProject/PerformanceUI and A/B/C independent renderer.
- Cases/procedure: Library/track/grid/whole-transition preparation, seek/loop/built-in/VST edits and A/B/C; Apply versus quantized Launch; replace/delete/lock target or change programme during preparation, cancelled load and stop/device change before boundary.
- Pass/oracle: private edits remain private until named commit, stale target rejects, Apply/Launch has one defined musical commit and matching event/sample boundary. Private A/B/C renderer doesn't borrow old Arrange matched-RMS history or retarget programme MIDI. Preserve existing effects/tails and timing outside intended target.
- Evidence/mode/severity: source/target generation IDs, pre/private/post state+PCM and boundary samples, real control flow; automated + manual/listening; P1.

### MW-P11 — Live master WAV captures exactly accepted programme frames
- Contract/source: MasterRecorder bounded FIFO, post-master/pre-monitor point, optional capture contract.
- Cases/procedure: deterministic programme plus private tone, mute monitor, paused take tails, instrument notes, zero/short/end and odd callback blocks; new filename, existing destination, FIFO refusal, simulated disk/flush/header failure, stop/close after writer drains.
- Pass/oracle: decoded stereo PCM24 frame count equals accepted callback frames and programme sample content within quantization; private absent; no source overwrite. FIFO/OS/flush failure preserves labelled partial file and incomplete notice while programme/editable take continue. Long RF64 capacity is separate R08 and remains BLOCKED without authorized measurement.
- Evidence/mode/severity: accepted-versus-decoded frames/null, writer fault injection and retained partial-file hash/diagnostic; automated + listening; P1.

### MW-P12 — Mode/take lifecycle never strands edits or leaks a worker
- Contract/source: PerformanceModeUI, Workstation/Engine teardown and TakeSeekPreparation ownership.
- Cases/procedure: mode switch idle/capture/replay/private/export/busy, take discard/Undo, close or new/open during reconstruction, stale callbacks, final cursor beyond arrangement after exiting take; save/reopen all legal states.
- Pass/oracle: exclusive states reject explicitly or finish by contract; cursor clamps to valid target, no hidden resumed audition, callback to destroyed UI or abandoned writer/renderer/child; arrangement edits/Undo remain usable. Saved pending/recording state loads safely stopped with truthful completeness.
- Evidence/mode/severity: state-transition matrix, owned thread/process lifecycle and current-format disk roundtrip, real UI cancel/close; automated/manual; P1.

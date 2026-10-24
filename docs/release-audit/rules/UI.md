# UI, admission and complete workflows

Inherit [rule contract](../RULES.md), fixture profiles and recovery steps. Scope UI includes Workstation/WorkspaceUI/Timeline/UI translation units, AppWindow, AlloyLookAndFeel, accessibility and every expanded row in the control inventory. Use F-UI and J01–J10 in COVERAGE. All rules mandatory, initially NOT RUN.

### MW-U01 — Every advertised command reaches its intended owner
- Contract/source: labels/help must describe the invoked operation; `WorkspaceUI.cpp` typed MenuCommand, `TimelineAction` in Workstation.h, CONTROLS families C01–C23; historical AUD-001/002 are regression seeds, not current failures.
- Cases/procedure: enumerate every top-level/context/submenu/picker command and dynamic ID with empty/one/two selections; use conflicting Library A/Playlist B/timeline C selections. Exercise clean/dirty, stopped/playing, busy/export and locked states pairwise; explicitly try all six transition lengths against Quit. Trace callback → named target ID → model/engine action → feedback → Undo/save/output.
- Pass/oracle: exactly the labelled operation on the active named target, or explicit rejection with unchanged state; no ID collision, hidden selection precedence, inert success or unreachable shipping functionality. Only intentional Quit requests close. Source branch tracing plus actual native/UI state diff is the oracle.
- Evidence/mode/severity: completed per-control trace table with runtime case IDs, before/after state and pixels/command logs; automated native + manual gesture; P1. Missing control discovery remains NOT RUN, never assumed covered by its group.

### MW-U02 — Values, units, defaults and reset agree across routes
- Contract/source: slider ranges in UI constructors, Engine setControlValue, Model automation addresses and docs/LIVE-DESK.md, MASTERING.md, TIMELINE-EDITING.md.
- Cases/procedure: for each numeric control test min/default/max, one step either side, text NaN/Inf/blank/locale decimal, wheel/key/drag/direct entry/MIDI where exposed; double-click/Home/reset, disabled state and reopen. Include gain percentages above100, pan, centre-off cutoff, BPM0.01 and transition positive bars.
- Pass/oracle: accepted physical/normalized values match declared conversion and audible/model value, clamping versus rejection is explicit, reset restores the documented neutral without touching unrelated controls; one logical Undo. Independent unit conversion and source-tone PCM, not paint constants, determine correctness.
- Evidence/mode/severity: range/default/conversion sheet per family, state/PCM for meaningful controls and inspected labels; native + manual; P2 (P1 for audible conversion error).

### MW-U03 — Keyboard, focus and accessibility expose actual operations
- Contract/source: Workstation::keyPressed, Timeline/TempoStrip accessibility, AppWindow F11, native widget handlers and historical AUD-006.
- Cases/procedure: enumerate shortcuts from code and in-app Shortcuts; invoke with focus on timeline, Library, dialogs, text editor, VST editor and detached panels. Tab/Shift+Tab each dialog, Escape/Enter, disabled controls, selection announcements; operate with keyboard and Windows accessibility inspection. Context actions must use the focused item.
- Pass/oracle: text editing retains editing shortcuts; no global destructive command leaks from text/plugin focus; every claimed native accessible control has name/role/state/value and usable action/focus route. No trapped modal/focus or silent inaccessible action. Record unsupported OS/plugin accessibility limits rather than assuming JUCE guarantees them.
- Evidence/mode/severity: native hierarchy, keyboard walk and accessibility notes for each screen/state, shortcut reconciliation; native + desktop/manual; P2. Exact WCAG-style compliance claims require separate requirements (REQ-04).

### MW-U04 — All panel and display configurations remain usable
- Contract/source: WorkspaceLayout, AppWindow, four-lane geometry; docs/WORKSPACE-LAYOUTS.md and testing policy.
- Cases/procedure: F-UI size/scale partitions; all three console panels dock/collapse/detach/reattach, Library/inspector collapse, reorder, drag separators, named layouts, window/max/fullscreen/span and monitor removal. Resize overall height then lane edges and width only; reopen preferences; cancel named-layout pickers and malformed imports.
- Pass/oracle: four lane controls remain reachable, equalized heights differ ≤1 physical pixel after overall resize (integer partition requirement), individual shares survive width-only changes, offscreen windows recover within a present display. No clipped critical control, hidden dialog, overlap that blocks input or musical Undo/state mutation from layout.
- Evidence/mode/severity: inspected compact/wide/scaled/detached screenshots and geometry/state diffs, saved workspace; native + actual display/manual; P2. Span/physical scaling BLOCKED if unavailable.

### MW-U05 — Empty, loading, error and recovery states tell the truth
- Contract/source: Workstation timer/status/busy, ImportProgressView, all asynchronous dialogs, RUNBOOK status contract.
- Cases/procedure: first launch no media/device/runtime; each import/open/analysis/Auto Arrange/stem/export/take-seek pending → success/error/cancel; close/reopen job window; create a stale selection/dialog callback before target removal in an owned fixture.
- Pass/oracle: progress/cancel/disabled controls reflect real ownership, success only after durable completion, errors identify failed source/action and next safe step, no stale callback mutates a new target. Busy clears only after safe handoff and valid retry works. Cancel/failure follows shared recovery procedure.
- Evidence/mode/severity: timestamped UI/state/worker trace and retry, capture each terminal state; native + bridge + manual; P1 for stranded app, otherwise P2.

### MW-U06 — Selection, previews and cancellation do not secretly edit music
- Contract/source: EditSelection, AutomationUI, TransitionUI/RehearsalUI, popup dialogs; docs/PRESET-NODES.md and RANGE-EDITING.md.
- Cases/procedure: select/hover/browse tracks, curves, transition modes, presets and owners without applying; change selection during modal/preparation; Escape/cancel a node drag, range drag, tempo gesture and preset picker. Include hidden nodes and a playing project.
- Pass/oracle: document, Undo depth and PCM stay unchanged for pure navigation/browse/cancel; completed edit has one Undo; selection clears/transfers by documented intent. Appearance restoration uses the exact narrow tolerance from TESTING.md: ≤1 pixel, each RGB delta≤3/255, same alpha/dimensions; state/audio remain exact.
- Evidence/mode/severity: before/after serialized state, Undo and image delta with reviewed captures; native + mouse/keyboard; P2.

### MW-U07 — Help, motion, artwork and visual feedback remain accurate
- Contract/source: AlloyLookAndFeel/AttractLights, PeakMeter, Workstation tooltip/Shortcuts; docs/POPUP-HINTS.md and MASTERING.md.
- Cases/procedure: popup help on/off/reopen, reduced-motion Follow Windows/Animated/Steady, Auto Arrange completion/Escape, missing/corrupt artwork and meters clip/reset. Compare enabled/hover/selected/disabled, light motion and static states at F-UI sizes.
- Pass/oracle: legible units/ownership/status, no missing essential hints, no contradictory guide; explicit motion choice and Escape restore intended workspace state without music edits. Artwork failure cannot strand import. User-approved graphite/copper style is not itself a defect; visual taste findings are labelled recommendations.
- Evidence/mode/severity: inspected captures, motion-state notes and document/state diff; manual/native; P3, P2 where feedback prevents correct operation.

### MW-U08 — Complete journeys cross subsystem boundaries
- Contract/source: current workflows in HANDOVER, COVERAGE J01–J10 and current source; README is a requirement claim, not acceptance evidence.
- Cases/procedure: execute every journey from its real entry, including cancel/error/retry and restart at the documented points; exercise controls by gestures for input paths bridge cannot cover. Compare final saved model, independent rendered/recorded PCM and visible state.
- Pass/oracle: J01–J10 outcomes all hold on one identifiable candidate; no successful unit result substitutes for import→analysis→editing→playback→save/reopen→export or Record/Perform→replay handoffs. Listening and hardware steps remain separately required.
- Evidence/mode/severity: per-journey chronological log, source/output identities, final captures, decoded checks, listening/hardware cross-links; mixed modes; P1.

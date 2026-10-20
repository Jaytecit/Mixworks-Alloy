# Bar jumps and linked-track moves

The verified 0.43 build adds two menu groups:

- **Mix → Jump backward / forward → 4 / 8 / 16 / 32 bars** seeks by mix beats from the current cursor. It clamps at session start and the selected audible end. Ordinary playback retains its running state; arriving at the ending permits the normal device callback to stop there. Jumping changes no musical data, dirty state or Undo history.
- **Edit → Move linked track backward / forward → 4 / 8 / 16 / 32 bars** moves the selected track's start by that many integrated mix beats. Existing linked tracks follow through the model's normal relationship rules. Source cues, effect owners and ordinary playhead position are retained. A successful changed command has one Undo.

These commands use the existing tempo map, including ramps. They do not infer a new source beat grid or claim new automatic beatmatching. Pending placement/tempo work and busy workflows defer the commands. Musical locks and protected neighbours remain authoritative. A move that would push any linked predecessor before the mix is rejected as a whole; it does not silently shorten the requested step.

Duplicate is also prepared before committing: an unavailable owned effect/automation mapping reports its failure while retaining the current project, selection, ownership, history and dirty state. Successful duplication still creates an independent effect owner with one Undo. Preparation and validation happen before recording the edit.

This is a bar navigation/movement foundation. Direct range selection, bar repeat/remove, explicit ripple operations and independent private preview remain planned work. Frozen candidate01 passes **1,705 checks across 22 suites, zero failures/errors** (`.local/b33-g1/summary.json`), with all 162 code/harness hashes unchanged. Focused timeline checks pass477; the full timeline suite passes481 with real plugins. VerifyOnly and deployment accepted the exact executable SHA256 `a7590dc7bf1e562808f31e45851e94b8fb9b3c496df02c28d31ec81af74e8195`. The launcher now selects `out/releases/0.43.0-a7590dc7bf1e/Mixworks.exe`.

The 29 new native-menu and failure-atomicity checks discover actual commands by visible group/count labels and cover every count/direction, tempo ramps, bounds, protected chains, preparation/busy rejection, live flags, Undo/Redo and failed/successful clone ownership. The malformed owned automation fixture establishes rejection safety; it is not a native-provider equivalence claim. Existing paired swap handles and adjustable rehearsal context remain included. The user application was not restarted.

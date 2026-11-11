# Unavailable actions

Current core editing changes are under verification; candidate and evidence are
in [HANDOVER](../HANDOVER.md). W04 remains open in the
[roadmap](../FINISHING-PLAN.md).

Core context rows include the recovery step in their disabled text. Editing
buttons use hover help and an accessible description. Direct commands recheck
the same prerequisites and put refusals in the status area. They do not open a
repeated warning dialog. This covers split, cut/copy/paste, range/group removal,
repeat/crop/replace, unlink and loop-marker prerequisites.

| State | Explanation and retry |
| --- | --- |
| Export in progress | Finish or cancel export before changing the mix. |
| Source/tempo preparation | Wait for the named preparation to finish, then retry. |
| Selected source missing | Relink or reload its audio. |
| Transition lock | Unlock the participating transition before changing audio. Copy remains available. |
| Range/group edit during playback or rehearsal | Stop first; the refusal preserves playback and the selection. |
| Range/group edit during automation writing | Finish writing and select Read. |
| Empty clipboard | Copy audio before Paste or Replace. |
| Split outside the selected segment | Put the cursor inside the segment. |
| Invalid loop end | Mark a start, then an end later in the same segment. |
| Menu target removed or document changed | Select the current audio and reopen the menu. |

Mix-bar jumps and track moves also recheck busy/export admission. Direct commands
name the operation to finish or the export to finish/cancel, without a popup or
partial edit; export disables their menu groups. Missing track selection asks for
a selection. The .90 focused bar suite verifies these status/atomicity paths.

Track bar moves, single splits and single removal also refuse pending playback-
position preparation, a queued programme launch, rehearsal and active automation writing. Ordinary Arrange edits require Read;
idle Record/Perform takes remain editable in their default Latch mode. Finish/wait for the named activity, or select Read, then retry. Live source
preflight failure asks to check the source; the project, history and running clock
are retained. These direct guards run before publication and do not add a popup.

Duplicate, range/group, Replace, group-move commit, unlink and single paste now
use the common pending-seek/launch/preparation and automation guards. A document
changed during preparation refuses publication before history changes. Duplicate
also checks that its source selection is still current. Range/group edits still
require stopped playback; the change does not silently admit live range surgery.

Transition preset replacement separately names the pair, affected parameter and
time range, preserves exterior nodes and mixer-lane curves, and offers Cancel.
Confirmation checks for a changed target/document and a newly unavailable state.
See [transition presets](TRANSITION-PRESETS.md).

Remaining W04 work includes full automation/plugin/device/output coverage,
save/export/chooser failure routes, stale dialogs beyond core editing, and actual
keyboard/MIDI/accessibility interaction acceptance. Earlier audit inventories
remain historical evidence; no whole-app readiness claim follows from this table.

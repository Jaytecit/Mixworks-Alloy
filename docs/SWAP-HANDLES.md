# Swap timing and amount controls

Current guide for 0.55.64. Presets now place ordinary nodes, replacing the procedural model in [the archived swap guide (archived)](MAINTENANCE-20261103.md).

## Move the handover

Select a transition and reveal Volume/Bass/Mid/Treble. A recognizable ordinary swap exposes paired horizontal handles. Drag either side to move the paired handover while keeping its musical gap and song/frame geometry. Snap applies; Alt permits free timing. One changed gesture has one Undo. Ambiguous envelopes do not get guessed handles. Locks, audio/frame bounds, neighbouring ownership and conflicting points still protect edits.

## Change the amount

Vertical amount handles change supported cut levels at fixed time. Selected captions show percentages; gain/EQ remaining level differs from cut depth. Filter uses its actual normalized/physical value. D/Shift+D cycles available handles; Up/Down changes1 percent, Shift ten. Enter/double-click/RMB opens the exact editor. Delete on an amount does not delete audio.

## Place or pair a swap

RMB **Swaps** distinguishes applying Bass/Mid/Treble/Volume shapes from **Pair bass swap**. Placement reveals the proper parameter/lane. Pairing targets the clicked receiver and an existing explicit linked partner; it can use a full cut or supported complementary opening and retain sharp bends. Source-time mapping remains canonical. Unlinked, ambiguous, conflicting or protected targets refuse; ordinary placement does not invent a transition.

Node Add/Delete/time/value/bend operations remain available after preset placement. See [ordinary preset nodes](PRESET-NODES.md) and [transition controls](TRANSITION-PRESETS.md). Native handler/PCM evidence exists across the recorded builds; physical feel/listening acceptance remains open in the [request register](REQUEST-STATUS-20261024.md).

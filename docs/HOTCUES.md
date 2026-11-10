# Timeline hotcues

Candidate workflow under verification, 4 November 2026. The selected production
launcher remains the earlier bundle; see [HANDOVER](../HANDOVER.md) for exact
candidate identities and evidence. [Previous source-only workflow](history/finishing-core-20261104/HOTCUES.md)
is historical.

HC1–HC9 belong to the selected placed segment. A pad jumps the **whole mix** to
that cue's timeline position. While playing, playback continues there on every
lane. While stopped, it stays stopped. The visible playhead and viewport reveal
the destination. Alt+1–9 uses the same action.

Timeline cues jump immediately. The Controller's pause timing still supports
Immediate, Next beat, Next bar and Next phrase; it does not delay a timeline cue.
There is no pending source-only cue hidden behind a timeline pad.

To map a pad, enable MIDI Learn, click HC1–HC9, then use the desired physical
control. The typed assignments are timeline-cue-1 through timeline-cue-9. Virtual
MIDI plumbing is separate from physical FLkey/reconnect acceptance (W12).

A future placement can be a cue destination. A cue excluded by that placement's
trim is unavailable: extend the edge to restore its source interval, or right-click
the cue to edit it. The action never silently restores excluded audio. Empty slots
open the source-position editor while stopped. **Cues / resets** also offers exact
cue edits and the existing EQ/filter/pan reset controls.

In Record/Perform, a cue records a programme seek in the editable take. Replay
follows the same all-lane timeline jump. It keeps earlier heard events rather than
rewriting the original recording. Real hardware and musical listening remain
separate acceptance, not implied by native PCM checks.

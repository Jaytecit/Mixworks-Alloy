# Scheduled programme pause

The Controller has **Pause @** and a neighbouring timing button. This explicitly pauses the audience programme at your selected boundary. Ordinary **Play/Pause** still pauses immediately; **Stop** still returns to the mix start. The same timing choices govern target-only playing hotcues, as described in [hotcues](HOTCUES.md).

Choose **Immediate**, **Next beat**, **Next bar** or **Next phrase**. Next bar is the default. Phrase length is 8, 16 or 32 bars, initially 16; its grid starts at project bar 1 and follows the project tempo map. Section hints and timeline Snap do not change that grid. Timing and phrase length save with the session; an older session starts with Next bar and 16 bars.

While the ready programme plays, press **Pause @**. The Controller shows the queued bar/time and changes the button to **Cancel @**. Press it again, or choose **Cancel queued pause** in the timing menu, to cancel. A quantised trigger chooses the next strictly future boundary, including when pressed exactly on a beat or bar. A boundary beyond the remaining mix is rejected; choose an earlier timing explicitly.

The pause executes on the audio sample clock and leaves the cursor at the stopped position. It does not edit the arrangement or create Undo history. A subsequent Play resumes there. An active automation take finishes at the stopped cursor on the application thread before another start. Change saved timing in Read mode; each actual timing or phrase edit has one Undo and cancels the pending pause.

Seeking, immediate pause/Stop, changing the grid/timing or audio clock, exclusive audition/export, or unavailable programme content cancels a pending pause with a reason. Cancelled or missed actions do not roll forward or execute later. Session reopen restores saved choices silently, with playback stopped and no pending queue. No automatic device configuration, MIDI remapping or transport start occurs.

This shared launch clock covers scheduled programme pause and target-only playing hotcue queues. Private preparation/launch and discrete performance-event replay were added in 0.55.64; general live-loop capture/release remains future work. See [the implemented mode contract](ARRANGE-RECORD-PERFORM-PLAN.md). Physical controller, audio-device and musical listening acceptance remain separate from native verification.

See [live desk](LIVE-DESK.md), [Performance plan](PERFORMANCE-MODE-PLAN.md), [testing](TESTING.md) and [handover](../HANDOVER.md).

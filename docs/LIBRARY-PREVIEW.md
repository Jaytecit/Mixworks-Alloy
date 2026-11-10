# Library preview

Each library title has a small waveform preview strip beneath it. Click or drag along the strip to hear that part of the source; the title stays readable, and seeking does not place the track on the timeline. Clicking another position in the same track seeks without stopping or reopening it. The cursor shows the current audition position. Tracks without analysed waveform peaks still have a seekable line.

With the strip focused, Left/Right seeks five seconds, Shift+Left/Right thirty seconds, and Home/End seeks the source edges. Space or Enter toggles preview; Escape stops it. The artwork play/stop control remains available. Track review warnings do not disable preview.

In **Arrange**, click the library artwork's play overlay to audition through the **master output**; click again to stop. Preview pauses arrangement playback without rewinding its cursor. Programme Play, Stop, seeking, another audition or an audio-device change ends master preview. The master fader and monitor mute apply. Nothing is inserted into the arrangement, written as automation or included in offline WAV export.

The independent reader streams without full analysis. Switching tracks cancels the prior load; end of file stops preview. Audio settings select the master output. From 0.55.64, **Record/Perform** uses an explicitly selected private route without pausing the audience programme. Headphones are optional for recording a take, but private playback requires a valid route. See [the implemented mode plan](ARRANGE-RECORD-PERFORM-PLAN.md).

**Takes / preview → Private preparation / headphones** provides output selection, headphone level/test, private track or whole-transition preparation, seek/loop/control edits, A/B/C alternatives and explicit Apply/quantized Launch. Preparation owns independent transport and DSP; immutable PCM can be shared. Private selection does not retarget programme MIDI. Stale targets are rejected. Private A/B/C uses its independent renderer; the earlier Arrange rehearsal retains its matched-RMS comparison.

Choose a separate device or an independent stereo pair on the same interface while stopped. Audience output occupies channels 1-2; private pairs cannot overlap it. For a separate device, the master device name is excluded, including identical names under other drivers. Missing or failed headphones never fall back to master. Software cannot establish physical wiring behind differently named driver endpoints. Changing main audio configuration/settings stops preview; separate devices require drivers that can operate together. Private audio is excluded from master WAV recording and offline export, and its level is independent of audience monitor mute.

Focused fake-device checks cover rendered master/private audio, cancellation, document/transport preservation and output failures. Physical headphones, driver coexistence and musical listening still need acceptance on the target hardware.

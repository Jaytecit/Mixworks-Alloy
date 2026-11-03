# WAV export and live recording

Current guide for 0.55.64. Arrange exports an offline mix; Record/Perform exports a reconstructed take and can optionally capture a live master WAV. WAV is the implemented output format; MP3/FLAC and label-split publishing remain planned.

## Arrangement endings

Shared original-track processors continue through deleted gaps and finite reported tails, followed by lane tails. **Effect ending** supplies a saved, undoable minimum/fallback where a plugin has no usable reported tail; unknown/infinite tails require an explicit bound. Source geometry/fades remain local. Choose the available last-source/tail ending before offline export. Natural tail planning and the manual fallback supersede the old fixed-window-only description.

Offline WAV is stereo PCM24 at the selected integer sample rate. Export validates frame count, output/backup space and source identity, including hard-link aliases. Cancel/failure preserves the existing destination; successful replacement retains a backup. It uses independent native plugin instances/copied state and shared immutable prepared audio on a single render worker. This exclusive operation pauses programme playback and guards edits, with progress/Cancel available. It does not record live MIDI notes.

## Editable take export

Takes / preview exports through the same ordered event/control path as replay, retaining source jumps, tempo/launch/structure and supported controls. Pause has held source time with silence/tails. Cancellable seek reconstruction rebuilds the take state. The original arrangement remains available independently. Live notes, opaque plugin topology/state and nondeterministic processing cannot be guaranteed by editable reconstruction alone.

## Optional live master WAV

Enable master WAV in Record/Perform for the audible programme after master processing and before monitor-only mute. Private preparation is excluded. A bounded FIFO/threaded writer avoids file writes in the audio callback. Write/flush/FIFO failure retains a labelled partial file and reports incomplete recording while programme and editable capture continue. Final accepted frames are checked.

JUCE supports RF64, but long-capacity recording and latest-build 60/90-minute hardware/listening acceptance remain open. Native short fixtures/software routing are evidence for their stated scope. [Mode contract](ARRANGE-RECORD-PERFORM-PLAN.md), [mastering](MASTERING.md) and [request register](REQUEST-STATUS-20261024.md) describe limits. [Earlier export notes (archived)](MAINTENANCE-20261103.md) preserve dated cancellation/tail evidence and original formats.

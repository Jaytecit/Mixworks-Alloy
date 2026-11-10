# W03/W07 optional original demo

## 5 November verification on the selected build

The Library startup bundle selected by `out/Launch.cmd` has executable SHA256
`0d1a41825c0b9d2a62d3a50f045333bcd7a337d6780f76c6c1771d9244e6990b`.
It passed 28 optional-demo and 9 tutorial checks through `verification.run_checks`.
These covered owned audio generation, cancellation/reuse, preview, export-renderer
output, save/reopen and durable helper preferences. They supersede the pending
candidate25 check. The earlier candidate29 pass is separate evidence.

A fresh bridge-owned desktop session passed 14 checks on the same executable.
Actual pointer input opened the guide from Help, created the three demos, skipped
and resumed the placement step, placed Blue Hours by double-clicking its Library
title, highlighted a 7.5-second waveform range and started muted playback from
the guide. Bridge save enabled Finish and completion was written to disk. Bridge
reopen prepared the same clip and reproduced all 4,096 left-channel samples at
12 seconds. The isolated app exited normally. The first step, range, save-ready
and reopened captures were inspected.

Receipts, captures and commands are in `.local/finish1105`. The first desktop
attempt's keyboard input left the Help menu open. The driver closed only its own
child after cleanup timed out. The second attempt used the visible menu item and
closed gracefully. Both receipts were retained. All runs used owned fixture
retention with stress off. The executable and matching source checkpoint remain
in the current [handover](../../HANDOVER.md).

Automatic first-launch display, own-music import, normal save/export dialogs,
actual WAV delivery, keyboard access and newcomer/listening acceptance remain
open. Production session and data were untouched.

## 4 November interruption and earlier candidates

Jay requested updating the finishing plan and ending the session on4 November2026,
then authorised pointing the launcher to the latest successful verified build.
No further native checks were started after that request.

The tutorial now offers three locally synthesised32s, stereo44.1kHz,120BPM,C-major
tracks, with original drums/bass/chords/melody, WAV provenance and authored manual
grid/key. Automatic detector confidence is not fabricated. Generation uses the
existing single worker, small waveform buckets, a new owned staging folder and
only removes its own generated files/empty folder on cancellation. Hash-verified
reuse preserves saved corrections; unknown/changed files are refused. No external
service, recordings, purchase, agreement or public package was involved.

Source23 compile failure is retained: writer stream needed the JUCE base-stream
unique_ptr type; fixture used the wrong restore API. Corrected24 built and ran
`--tutorial-demo-only` through run_checks:28 checks,2 failures. Cancellation/retry,
actual import of all3 tracks, exact codec dimensions, authored cached facts,
independently decoded finite/unclipped PCM, provenance, unchanged arrangement,
hash reuse, saved-correction preservation, unknown-folder refusal and actual
later-passage preview passed. First-use window capture was inspected.

Placement allocated a timeline clip asynchronously. The test waited only for
`busy`, attempted export before its audio preparation finished, and received
“Wait for all source audio before preparing export.” Save/reopen also failed;
that earlier assertion lacked detailed error output. Generated audio was removed
by ownership-checked retention, with no errors. Original failures are retained.
The original tutorial suite did not run because the batch stopped at this failure.

Candidate25 expands waiting to actual timeline/preparation completion, gives the
tutorial's next step the same readiness requirement and a plain preparing reason,
and adds save/reopen diagnostics. It **built successfully but has not been tested**.
Executable SHA256: `db5bc293a749a1d9ffcc954fa068892aa668752a7fc94c21c018e53b223ad932`.
Checkpoints: `.local/fw04/candidate-23-source.zip` through
`candidate-25-source.zip`, matching per-file manifests, build23–25 logs,
`native-24-demo`, `focused-24.json` and `work-log.md`.

At that checkpoint candidate22 was the verified local launcher. Candidate25
remained untested. Its planned run was superseded by the later candidate29 and
selected-build results above.

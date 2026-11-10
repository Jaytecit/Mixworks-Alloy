> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Playback stopping at outgoing EOF — 1 November 2026

Jay reported playback stopping at the first track boundary after the transition,
and confirmed the playhead stops; the status message was unavailable. Production
initially PID26796, later PID10900 runs ua17; no control,
restart, activation or production-data changes were made.

The read-only recovery snapshot names Alien (Original Mix) followed by Sea Line.
An owned byte-identical copy of Alien reproduces the failure: an independent
exact-length read of the last samples succeeds, but OriginalAudio requests a full
32,768-frame block extending beyond EOF. JUCE's MP3 reader returns failure there,
marking the source failed and stopping the entire mix through requestPlayback.

OriginalAudio now requests only the remaining valid frames of the final block;
its already-cleared padding stays silent. Genuine read failures and source
identity validation are retained. No detector, cache revision or session changes.

Added bounded `--transition-boundary-only --boundary-fixture <absolute audio path>`
coverage. It copies its input under the owned report root, compares every valid
sample in the final partial block with an independent reader, then drives the
actual device callback across outgoing EOF into a continuing incoming clip.
The incoming fixture is synthetic: this establishes transport and PCM behaviour,
not musical listening acceptance of the complete real arrangement.

Two successful serial single-worker below-normal builds. Red candidate
`011f2f91f31c` fails the final-block and callback assertions (3 pass/2 fail).
Final candidate SHA256
`3c1e3f3eb943db1fe857539539c5f9a604fde519374d02776cd96a82907615c1`
passes boundary5/0 and original-playback42/0, serially through
`verification.run_checks`, stress disabled, no infrastructure/retention errors.
The existing suite covers WAV/FLAC seeks, continuous callback PCM, overlaps,
reverse/loops, bounded ownership and changed-source/stem refusal.

Evidence/source archives/exact executable identities survive independently of
Git in `.local/ua18/`. Generated WAV/FLAC fixtures were removed by shared retention.
Encoded MP3 copies are outside that extension matcher, so both exact owned copies
were separately hash-journalled and removed; each matched the unchanged original
at cleanup. Original music, analysis, corrections, arrangements and references
are untouched. Launcher remains ua17. Full regression, hardware/listening and
whole-product readiness remain open under the low-disruption policy.

Jay subsequently requested "Activate it please". Activated the exact verified
candidate at `out/releases/0.55.65-3c1e3f3eb943/Mixworks.exe`; all34 staged payload
hashes, source/archive membership, pinned model/runtime and47/0 reports were
revalidated. Visual runtime matches the prior ua17 payload. Launcher was updated
atomically with both local-evidence and out/ backups; desktop shortcut through
Mixworks.cmd verified. Version remains0.55.65. Previous bundle remains intact.
Production PID10900 still runs ua17; no app launch/restart or user-data changes.
Receipt and activation summary are in `.local/ua18/` and the staged bundle.
This is the explicitly authorised focused local-preview path, without weakening
the normal release gate or claiming complete regressions. No repeat build/tests.

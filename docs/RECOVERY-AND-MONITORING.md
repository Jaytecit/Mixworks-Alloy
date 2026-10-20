# Recovery and monitor mute

The Mute checkbox beside MASTER OUTPUT silences the device's speakers/headphones. Playback, the playhead and meters keep running. It also silences source-grid audition and A/B/C rehearsal. It does not change master gain, automation, saved sessions or exported WAV audio. The checkbox is remembered in local workspace preferences, with the existing preference backups.

Mixworks checks for unsaved recovery snapshots when a window starts. File → Recover unsaved mix opens the same choice later. Restore loads through the normal session/audio/plugin preparation workflow; the restored mix is marked unsaved. Later keeps both your current mix and the recovery. Discard recovery removes those recovery generations. A damaged latest generation can fall back to its previous valid snapshot. Missing audio, unavailable plugins and Cancel preserve the current project and the source recovery.

A dirty mix is snapshotted every 30 seconds while paused and ready. Snapshot capture waits during playback, recording, rehearsal, import, export, pending audio or tempo preparation and save dialogs. Plugin state serialization may block, so this first implementation deliberately waits for a pause rather than periodically interrupting audio. Continuous live-performance crash coverage remains unfinished. Pause after an important take and save normally; a snapshot is not a replacement for a session save.

Each window owns its own UUID recovery folder and holds an operating-system lease. Other running windows are excluded. Two generations use temporary-file replacement, validated session data and a checksum. The original recovered record stays leased until a session save or durable successor snapshot succeeds. Save, New, a successful different-session open and an accepted clean close clear this window's recovery. A small owner record stays behind; unrelated files are preserved.

Recovery lives in the application data folder beside workspace preferences under `recovery`. Unknown, newer, linked or malformed data is retained. Encoding reserves up to six output bytes per input byte before allocation: session text is limited to approximately 21 MiB and the encoded recovery record to 128 MiB. Failure is reported in the status line, retaining previous durable data.

Verification and exact release identity are recorded in HANDOVER.md. Musical analysis remains revision 7; this work does not resolve the preserved variable-tempo or beat-one conflicts.

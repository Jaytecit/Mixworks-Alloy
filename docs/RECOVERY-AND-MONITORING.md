# Recovery, monitoring and playback diagnostics

The Mute checkbox beside MASTER OUTPUT silences the device's speakers/headphones. Playback, the playhead and meters keep running. It also silences source-grid audition and A/B/C rehearsal. It does not change master gain, automation, saved sessions or exported WAV audio. The checkbox is remembered in local workspace preferences, with the existing preference backups.

Mixworks checks for unsaved recovery snapshots when a window starts. File → Recover unsaved mix opens the same choice later. Restore loads through the normal session/audio/plugin preparation workflow; the restored mix is marked unsaved. Later keeps both your current mix and the recovery. Discard recovery removes those recovery generations. A damaged latest generation can fall back to its previous valid snapshot. Missing audio, unavailable plugins and Cancel preserve the current project and the source recovery.

When locating a moved source, choose the original recording. Saved source hashes
and dimensions remain attached to the new location, including saved-take snapshots;
a different recording is refused. Successful Locate marks the mix unsaved so you
can explicitly keep the new location. Cancelling or failing Locate preserves the
current mix and the saved arrangement. The focused ua15 recovery/relink evidence
is recorded in the [audit checkpoint](release-audit/UA15-RECOVERY-RELINK-20261101.md).

A dirty mix is snapshotted every 30 seconds while paused and ready. Snapshot capture waits during playback, recording, rehearsal, import, export, pending audio or tempo preparation and save dialogs. Plugin state serialization may block, so this first implementation deliberately waits for a pause rather than periodically interrupting audio. Continuous live-performance crash coverage remains unfinished. Pause after an important take and save normally; a snapshot is not a replacement for a session save.

Each window owns its own UUID recovery folder and holds an operating-system lease. Other running windows are excluded. Two generations use temporary-file replacement, validated session data and a checksum. The original recovered record stays leased until a session save or durable successor snapshot succeeds. Save, New, a successful different-session open and an accepted clean close clear this window's recovery. A small owner record stays behind; unrelated files are preserved.

Recovery lives in the application data folder beside workspace preferences under `recovery`. Unknown, newer, linked or malformed data is retained. Encoding reserves up to six output bytes per input byte before allocation: session text is limited to approximately 21 MiB and the encoded recovery record to 128 MiB. Failure is reported in the status line, retaining previous durable data.

Verification and exact release identity are recorded in HANDOVER.md. Musical analysis remains revision 9; this work does not resolve the preserved variable-tempo or beat-one conflicts.

## Playback diagnostics (0.55.10)

Open **Audio / MIDI** to see the callback-health readout below the audio selector. Application counters accumulate for the current app run, including stopped transport; closing/reopening the panel does not reset them. **Over budget** counts callbacks whose measured work exceeded that block's duration (2.67 ms at 48 kHz /128 samples; 10.67 ms at /512). **Worst** is the largest measured callback duration. **Lock skips** and **size skips** count callbacks that returned silence because the engine lock was unavailable or the block exceeded prepared capacity. **Export silence** is intentional while an exclusive export owns the engine.

**Driver xruns** is a separate under/overrun count from the current driver. Unsupported/inactive drivers show **unavailable**; driver counts may restart after a device change. App callback timing cannot observe scheduling delays before callback entry, and an over-budget callback does not by itself prove an audible failure. Counters are diagnostic observations, not session data or an automatic driver/buffer adjustment.

If breakup returns, note the counters before and after the affected passage, driver, sample rate/buffer, active plugins, ongoing scan/preparation and other PC load. Compare the same passage with **View > Forge sparks** and **Attract lighting** off, then Steady and Animated, keeping other conditions matched. Preserve the mix and analysis; do not clear caches to diagnose a warm-playback issue. A larger buffer trades latency for processing headroom; choose it explicitly in Audio / MIDI if needed.

P10's isolated native probe used four overlapping synthetic stereo sources at 48 kHz with fixed built-in EQ/cutoff/saturation, 128/512-sample blocks, software-rendered 1920×1080 UI and off/steady/animated visuals, repeated with zero/one additional busy CPU worker. It found no callback budget overruns or lock skips in the focused run. Complete-frame software painting was comparatively expensive; it is not a measurement of the normal desktop compositor/GPU or incremental repaints. Driver output/listening, real VST load, cold caches, scanning and severe system contention were not reproduced. The user's background-process explanation remains plausible and unproven; this increment adds evidence for a recurrence without claiming to fix the earlier breakup or improve DSP throughput. Exact timing reports and verification limits are in HANDOVER.md.

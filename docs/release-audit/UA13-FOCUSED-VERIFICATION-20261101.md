# Combined grid, key-label and destination checkpoint — ua13

**Activation update (ua14):** Jay requested closing any running Alloy instances
and updating the launcher. No instances/owned visual workers were present before
or after. The desktop launcher now resolves to
`out/releases/0.55.64-3a8286068928/Mixworks.exe`; all34 staged files and source/test/
capture identities verified. No application was launched. Receipt and launcher
backup: `.local/ua14/`. This supersedes the unchanged-launcher history below and
does not alter the focused qualification or full-readiness limits.

Jay requested the build and focused checks after removing the RAM guard.
The combined candidate builds successfully and passes **237 checks, zero failures**.
No application-source repair was needed during this checkpoint. Twelve native
captures were inspected. This is focused local evidence, not full readiness or
a complete regression/release receipt.

Candidate SHA256:
`3a828606892804c6afc0cb35888f9d487b8fe20044fac90e714688da9d38c509`.
Executable, source archive/manifests, bundle identity, build log, native reports,
retention journals and screenshots survive independently of Git in `.local/ua13/`.

| Focused entry | Checks | Result |
| --- | ---: | --- |
| `--grid-alignment-only` | 61 | Pass |
| `--review-sorting-only` | 23 | Pass |
| `--destination-safety-only` | 111 | Pass |
| `--save-export-workflow-only` | 42 | Pass |

One build: one worker, below-normal priority, no RAM admission/kill threshold.
Four native processes ran serially through `verification.run_checks`, stress off.
No infrastructure or retention errors. Retention removed19 generated fixture
files /442,941 bytes; reports, hashes, source snapshots and captures remain.
No owned jobs remain.

## What this establishes

- Grid: the three guided pages expose their relevant controls and keep advanced
  controls separate; changing pages preserves draft/Undo. Counted anchors recover
  the independently specified128BPM. Whole-beat bar-accent adjustment preserves
  pulse phase/BPM; drag, precision, bounds, Undo and missing-tempo setup work.
  Source correction persists/reopens; failed save keeps the draft. Save preserves
  timeline placement. Rebuild confirmation cancel/accept dispatches correctly.
- Keys: accepted detector results display Detected across the score range;
  tentative/invalid results retain Review and manual keys remain Manual. Native
  key/grid columns and sorting are independent; display changes do not alter
  musical state. This does not measure musical key accuracy.
- Destinations: the actual chooser action confirms extension-corrected existing
  destinations for all four output paths; cancel preserves bytes and acceptance
  keeps backups. Arrangement-only originals and hardlink aliases are protected.
  Missing/directory destinations reject; Windows-held destinations fail without
  losing prior output, then succeed after release. Mid-write cancel/retry works.
- Save/export: actual Save As/Save/cancel/source-backed reopen and playlist/text
  outputs work. The written24-bit stereo WAV matches the independent2,000-frame
  sample oracle, including silent gaps. Session identity, history and originals
  stay intact through the export journey.

## Pixel review and limits

Inspected all five grid captures (three guided pages and two Advanced views),
the key/grid table, normalized replacement confirmation, commit-error details
and four save/export dialogs. Controls, paths and actions are visible without
overlap; guided pages reserve substantially more space for the waveform.
Guided captures retain the test's120-second overview zoom; they do not demonstrate
fine onset picking. The rebuild consequence dialog was functionally exercised
but not separately captured. Physical gestures, listening and novice success
still require assessment.

Chooser captures precede asynchronous folder-list refresh. Their lists still
show the earlier folder, so they qualify labels/filename fields, not browsing.
Actual resulting destinations and content were independently checked.

The focused grid test checks the rebuild callback contract, not the complete
whole-arrangement Beatmatch retiming pipeline. Wider formats, plugin tails,
physical hardware, disk-full/network/power-loss/concurrent-path scenarios,
detector accuracy and full regression remain outside this checkpoint.

Normal launcher remains ua08 `9770639eb18c`; no staging, activation, production
restart, rescan or cache reset occurred. These results supersede the build/runtime
pending status of ua09–ua11 for the stated scope only. Full readiness remains open.

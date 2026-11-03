# Source waveform/grid alignment and fine controls - ua16, 1 November 2026

**Activation update:** [ua17](UA17-VERSION-ACTIVATION-20261101.md) changes only the production version to0.55.65 and activates the verified local preview. Fresh37/0 and visible version verified; prior112/0 remains this checkpoint's separate evidence.

Jay supplied a source-grid screenshot showing the grid to the right of waveform attacks across analysed tracks, while automatic track-to-track placement works. He then requested fine waveform dragging, colour, clearer 4/4 markers, more detail and closer zoom. Final candidate `e1d1c5737f7b2da37bfc645386e82a76a946d0b103200b0bf883184b9c27c423` builds and passes **112/0 focused checks**: waveform36/0 and grid76/0 through `verification.run_checks`, sequential, below-normal priority, stress off. No infrastructure or retention errors. Twelve native captures inspected. This is scoped functional evidence, not full readiness.

## Reproduced cause and repair

Original-file playback deliberately retains compact source peaks. `Media::waveformRange` previously excluded original streams from exact detail and used complete overview buckets at every zoom. A transient near a bucket end therefore appeared early, although its grid time stayed correct. The source editor also kept drawing the catalogue copy after playback hydration. The320-frame buckets in the authored8kHz fixture produced33.75/33.75/13.75ms early attacks. Red24/9 reproduces this in queries and actual source/timeline pixels. This is evidence for a display defect; it does not establish accuracy of every real-song detector result.

Zoomed original ranges now query exact samples through the existing bounded streaming pool. Requests never wait for decoding or its mutex; pending detail is labelled and repainted, without substituting a misleading coarse peak. Overview remains compact. Block pinning protects each read; existing playback request policy remains unchanged. Clip loop readiness propagates. The source editor draws its prepared audition source and uses the same centred pixel intervals as the timeline; vertical waveform columns and ticks share pixel columns. Saved analysis/corrections/audio placement/detector and session format46 remain unchanged. No global phase offset or rescan/reset.

## Requested interaction and visual changes

- Click the waveform to position source audition. Drag it beneath the stationary grid to adjust phase at fixed BPM. A deliberate drag is distinguished from a click, including directly from detected beat one. Programme/source audition are not sought by dragging.
- Shift scales movement to one tenth; changing Shift mid-drag does not jump. One Undo per gesture; Escape restores phase and view. Existing A/B ruler dragging remains.
- Added250ms,50ms and10ms views; existing zoom IDs remain. Continuous cursor positioning and full opening field precision avoid rounding away fine saved timing.
- Blue-to-violet waveform colour represents RMS/peak body energy, not instrument/frequency classification. Orange marks beat one/bar boundaries; blue marks regular beats; labels use relative bar.beat notation. Footer distinguishes detected, saved and draft4/4 from pulse-only timing.

## Verification and retained failures

Waveform36 checks use a160s/8kHz mono WAV (3.84MB), three authored eight-sample attacks, independent silence/sample/pixel oracles at beginning/middle/end and two timeline zooms, original-file detail readiness, a four-sample streaming-boundary peak/RMS oracle, overview, trim/reverse and unchanged source/grid/geometry. This creates no detector/model/device or long playback workload.

Grid76 checks exercise actual editor handlers: counted128BPM anchors, existing draft/history/bounds/source-save/reopen/rebuild consent, new fine dragging, click distinction, modifier changes, auto-grid Undo and zoom. A completed fine drag moves about0.525ms in the250ms view. This is native event injection, not physical mouse acceptance.

Four successful serial single-worker below-normal builds; no RAM guard. Initial red24/9 preserved. After repair/UI additions, waveform34/1 and grid76/0 were retained. The sole remaining failure counted IN text and the lower downbeat triangle as waveform. Independent retained red/green pixel inspection in the clear region gives early/onset difference counts9/9 before versus0/9 after at both scales. The final fixture excludes those intentional overlays; waveform36/0 and grid76/0 pass on the same final candidate. No app assertion was weakened to conceal waveform error. Two generated final files/4,110,208 bytes removed without retention errors.

All source archives/manifests/diffs, exact executables/bundle identities, commands, reports, journals, screenshot hashes and the user's reference survive independently of Git in `.local/ua16/`. No owned jobs remain. Production PID27916 was initially observed; later process inventory was empty. No production process was stopped/restarted or data changed. Launcher remains ua14/ua13 `3a8286068928`; this candidate is not activated.

## Remaining readiness

MW-T02/N06/U01 and UA03/UA05 have partial native evidence, not whole-rule PASS. Physical dragging, real-music listening/independent BPM/downbeat truth, full arrangement retiming, other timing partitions and broader current-candidate regressions remain open. OriginalAudio/model shared changes make deferred broader regression material under Jay's minimal-check override. No stress/performance/capacity/frame-rate, full-library scan or release qualification. Earlier ua15 recovery/relink67/0 remains on its own exact candidate; it was not repeated here.

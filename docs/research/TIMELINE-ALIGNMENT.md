> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Timeline alignment and immediate moves — v0.20.0

The background grid, matched clip beats and point snapping now use the same project tempo map. Dragging a clip, editing its start or duplicating it performs a placement edit without starting an analysis or whole-track stretch job. **Snap** aligns the measured first pulse, including at the timeline's left edge. Nearby pulses on a deliberately free-positioned clip also act as magnets. **Snap off** and **Alt** preserve exact free positions.

Moving preserves the source span and fades in musical time. Each drag derives from its mouse-down geometry, so moving across ramps and back does not accumulate errors. Moving between lanes retains the exact audio. Source loading that completes during a gesture cannot be overwritten by the original placeholder. Labels and automation snap to the displayed background grid rather than switching grids over unsynced clips.

## Playback ownership

`Clip::moveTo` owns placement and duration reflow; `Clip::sourceElapsed` and its inverse own the shared source/time conversion. Unchanged tempo shapes reuse the existing prepared PCM. A move through a different tempo shape creates an immutable `ElasticSource` mapping and uses the already decoded audio. Construction does no audio analysis or rendering.

`ElasticRenderer` owns continuous SoundTouch processing and small recent-output caches in the engine. It carries processor history across 100 ms cache blocks, with 100 ms preroll when seeking or changing placement. Storage is reused and scales with concurrent sources; overlapping clips do not evict one another at a fixed voice limit. Splits share their original mapping. Pitch is preserved; normal playback/export still performs DSP. Moving is immediate, not a promise of zero CPU during playback.

Continuous playback and export agree sample for sample. A seek restarts processor history, so waveform phase can differ from uninterrupted playback; repeated seeks are reproducible and retain grid timing. Detailed elastic waveforms show the source mapped through that same beat clock, rather than stale audio from the old placement. Explicit tempo and source edits retain their existing preparation workflow. Tempo points stay at their chosen times during clip moves.

This uses the pinned SoundTouch 2.4.0 dependency already in the project. Its [official documentation](https://www.surina.net/soundtouch/README.html) describes pitch-preserving tempo processing and streaming input/output. No new dependency or REA session was needed for the local placement bug. Prior MixMeister evidence and first-downbeat findings remain in [FIRST-DOWNBEAT-VALIDATION.md](FIRST-DOWNBEAT-VALIDATION.md).

## Verification

Tested executable SHA-256: `6E41B2905085272527EE2BFE2CC42574554DABBEA94E9048ACE57F4381E3D5C9`.

**447 regression checks passed**, all exits zero, in `.local/verify-rhythm/v020-verified/`:

| Suite | Checks |
| --- | ---: |
| Rhythm | 23 |
| Beat grid | 22 |
| Main, including native tempo UI, editing and actual VST3 plugins | 271 |
| Playback cache | 52 |
| Library | 42 |
| Placement | 37 |

Coverage includes actual native drag handlers, Snap off/Alt, near-zero first-pulse alignment, neighbouring pulse magnets, lane-only moves, repeated out-and-back drags, numeric starts, duplication, undo/redo, held-tempo PCM reuse, source/fade preservation, reversed loops, split boundaries, save/reopen, malformed playback settings, twelve simultaneous elastic streams, pitch and cache-seam continuity.

The final synthetic ramp test preserved the 440 Hz tone. Maximum attack displacement was **9.18 ms** in continuous moved playback and **3.83 ms** in the sampled seeks. Minimum sustained-tone RMS at cache boundaries was **0.07057** for the 0.1-amplitude tone. Rendering 24.86 seconds took **164.3 ms**; the slowest measured single-clip 512-frame block took **0.816 ms**, versus **11.610 ms** of audio. These are measurements on this machine, not a guarantee for every device buffer size or plugin load. The build log `.local/build-v020-verified.log` contains no compiler warnings/errors.

## The same two real tracks

**41 real-track checks passed** in `.local/real-track-moves-v020/`. Both songs were moved through the existing 125.0033 → 124.0007 BPM transition, first with snapping and then freely. Source spans, tempo points, first-downbeat references and exact free positions were checked. Both snapped placements together took **0.011 ms**; both free placements took **0.010 ms**, excluding report writing. Source analysis and whole-track rendering are absent from the move operation.

`scripts/measure-rhythm.py` independently compared the decoded and rendered bass-energy envelopes against the intended time mapping:

| Scenario | Track | Accepted / examined comparisons | 95th percentile absolute displacement | Maximum |
| --- | --- | ---: | ---: | ---: |
| Snapped move | Sean Finn — Ain't Nobody | 559 / 559 | 3 ms | 7 ms |
| Snapped move | Airsand — Miracle | 826 / 826 | 3 ms | 6 ms |
| Free move | Sean Finn — Ain't Nobody | 559 / 559 | 3 ms | 7 ms |
| Free move | Airsand — Miracle | 826 / 826 | 3 ms | 6 ms |

Weak envelopes are excluded before the examined count; accepted correlations are at least 0.95. This measures stretch displacement, not independent musical downbeat correctness. The existing absolute first-downbeat references still pass at **0.844653 s** and **0.074300 s**, respectively. The MP3s and all four original sidecars have unchanged SHA-256 hashes in `source-audit.json`.

The folder contains native component screenshots, full isolated rendered tracks, transition listening WAVs, source audits, executable identity and `envelope-transfer.json`. The snapped timeline screenshot was visually inspected. No user session, MixMeister installation or live audio device was changed by verification. No human listening or physical gesture/device pass has been performed; wider-library MixMeister parity and the earlier edited-tempo outliers remain unproven/unresolved.

Two failed probes are retained separately: the first exposed cancellation at independently stretched chunk joins and included report I/O in the move timing. Continuous DSP history and a pure-operation timer fix those causes; they were not accepted as final output.

## Installation and sessions

`Mixworks.cmd` selects the immutable v0.20.0 release; close/save the running app and relaunch to use it. Existing releases and launcher backups remain. Session format **8** records elastic playback; versions **1–7** still load, while older app releases cannot open new v8 saves. Analysis revision **4** and the existing prepared-cache revision remain unchanged.

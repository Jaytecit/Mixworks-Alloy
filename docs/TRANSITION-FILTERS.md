# Paired transition filters

In the selected frame's lower **Transitions** panel, scroll to **Paired filter sweep**. Choose **High to low** or **Low to high**, then **25 / 50 / 75 / 100% depth**. **None** bypasses the filter exactly. Each edit has one Undo, keeps playback position and leaves the frame, manual EQ, inserts and song placement intact. Unlock both participating tracks and any protected neighbour before editing.

| Mode | Outgoing song | Incoming song |
| --- | --- | --- |
| High to low | Low-pass closes across the frame | High-pass opens across the frame |
| Low to high | High-pass closes across the frame | Low-pass opens across the frame |

The sweep follows the existing transition frame in session time. Resizing or moving the frame reprojects it through that geometry owner. Before/after the frame, each side holds its starting/ending amount while its audio remains present; the outgoing filter does not abruptly reopen. A song with both an incoming and an outgoing transition gets two independent, stereo filter stages. There is no added noise generator, resonance control or manual filter automation lane.

Filters combine after the song's inserts and built-in EQ, before its control pan/volume. Playback, private WAV export and rendered A/B/C rehearsal use this same processor. **Store A/B/C**, named presets and portable `.alloypreset` files capture the filter treatment together with the existing bands/fades. Applying a whole-pair None/Bass swap/Crossfade preset intentionally resets its filter; applying a saved treatment restores its captured filter. Manual automation is retained.

Session **20** saves the filter descriptor and reads sessions 1–19 with a dry default. Portable preset **2** saves filters and reads preset 1. Older executables cannot read the new formats; normal save/export backups retain previous files.

The DSP is an independent Mixworks design: two-pole Butterworth state-variable filters using double-precision trapezoidal state, a logarithmic 20 Hz to min(20 kHz, 45% of sample rate) domain, 80% maximum travel, and a continuous blend near neutral. Depth scales travel. At 44.1/48 kHz, full travel reaches about 80 Hz low-pass or 5 kHz high-pass; these are cutoff positions, not brick-wall removal. Neutral leaves each dry float exact. Seek/device/project resets clear filter history; a seek is a fresh state and is not an uninterrupted render until it has settled.

## Reference observations and limits

REA inspected the retained DJ.Studio frontend `FullScreenButton-BhZUkJRm.js`, SHA256 `e19d8f7c27e0cbea61eb78529c6170b0643b3a57e535d0848e86a75e3cb0dd32`. Literal styles select `AE_LowPass_Filter` / `AE_HighPass_Filter`. A bounded, provenance-recorded 1,637-byte fragment exposes separate primary/secondary levels, opposite signed directions, a range parameter and an 0.8 scale. A separate exact source observation confirms the clamp helper. The base automation handler delegates progress to an owner whose time direction remains unresolved. These are inert frontend observations, not native DSP or runtime proof.

The whole 6 MB module's REA semantic stage exhausted its 1,536 MiB worker heap; its graph is partial. The bounded fragment parsed 438 AST nodes, with 77 semantic unknowns. Its summary Evidence is `ev_ec73ef2cfa9348693e894aed09d3eb87f512fff1b8161bd3d6aa6b8e99c8d044`; retained inputs, source byte ranges and summaries are under `.local/transition-filters-29/`. No vendor code was executed or incorporated into Mixworks. Native frequency mapping, slope/resonance and exact paired timing remain unknown; identical DJ.Studio sound is not claimed.

## Verification

Candidate 05 passes **343 timeline checks**, including 52 new filter checks: measured low/high-pass spectra at 8/44.1/48/96 kHz, finite isolated stereo, dry bypass, uneven blocks and reset, overlapping neighbour stages, protected direct/indirect edits, current-value retention during geometry gestures, session/portable/A/B/C persistence, real source WAV Save/Open, actual WAV export and sample comparisons, rehearsal callbacks, native inspector edits, Undo/Redo and narrow-window scrolling. Every filter WAV sample matches playback within four PCM24 steps. None and zero-depth are exact under the same block partition; the existing unfiltered source interpolation differs by at most 2.98e-8 across the tested partitions. Native component capture was inspected. The exact frozen candidate also passes **1,571 checks across 22 suites**, with no failures or infrastructure errors; all 154 source/CMake/harness hashes remain unchanged. The same bundle passes the 27-check accepted 90-minute navigation/live-insertion/Save/Open replay, with all original source/session/reference hashes preserved. Receipt `receipt-05.json` passed VerifyOnly and deployment. The launcher selects **0.40.0-5e5ddffaffb7**, SHA256 `5e5ddffaffb7d024df761f42a17c8c6b20f851388e7c8b04dc283e7740b0e7f2`. The running user app was not restarted. These numerical tests do not establish listening quality or hardware/driver behaviour.

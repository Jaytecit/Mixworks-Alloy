> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Two-track validation — v0.18 historical baseline, 17 October 2026

**Current v0.19 results:** both automatic first downbeats are now detected and pass the absolute comparisons with the existing MixMeister markers. See [first-downbeat validation](FIRST-DOWNBEAT-VALIDATION.md) for the implementation, new REA evidence and fresh two-track results. The v0.18 failures below are retained as the baseline.

**Tempo matching and the requested timeline workflow work on these two tracks. Automatic first-downbeat detection does not yet pass. Automated mix creation should remain deferred.** This is a two-song validation, not proof of general MixMeister parity.

## Real recordings

| Track | Mixworks, freshly analysed | Existing MixMeister profile | Automatic first downbeat |
| --- | ---: | ---: | --- |
| Sean Finn — Ain't Nobody (Funkera Extended Remix), 5:01.61 | 125.0033 BPM | 124.999847 BPM | Unknown |
| Airsand — Miracle (Extended Mix), 7:08.04 | 124.0007 BPM | 123.997383 BPM | Unknown |

Both files were decoded by Mixworks. The final workflow probe explicitly recomputed detection from that decoded PCM instead of accepting the cached analysis. The results reproduce the earlier pulse estimates. Both sources pass the application's usable-pulse confidence/consistency checks, but **both fail the separate first-downbeat availability requirement**. Availability itself would not establish musical accuracy; neither song has a trusted, manually annotated bar-one reference in this test.

MixMeister's BPM fields were read from the existing MXM6 sidecars, using the profile layout established by REA/Ghidra. They differ by 0.00345 and 0.00332 BPM. These are historical references associated by filename; the proprietary source identity has not been authenticated and MixMeister was not executed for a fresh A/B render. Beat-map vectors and their decoder time origins remain insufficiently established to use as downbeat ground truth. No guessed MixMeister markers were imported into the application.

## Requested timeline behavior

The first track establishes **125.0033 BPM**. The second is placed near the requested position, aligns to the shared pulse grid and initially runs at that tempo. A pair of footer points then ramps the global tempo to its native **124.0007 BPM** during the overlap. Both clips follow the same map. The test moves the two points by +2 and +3 seconds and changes the target to **128 BPM**; the edited map survives preparation and both pulse grids remain aligned.

The same behavior now applies to library and Explorer drops. Known downbeats align to bars; unknown ones retain pulse alignment with `beat 1 ?` displayed. Double-click append defaults to an eight-bar overlap. Cut remains available and aligns to the next pulse without adding a whole-bar gap. A dropped clip's pending operation belongs to the clip, so loading, Undo/Redo and deliberate moves retain the intended placement. Playback/export/save wait for matching to finish; a preparation failure remains visible for an explicit retry.

![Two real tracks with their automatic tempo transition](C:/Users/jayte/Desktop/REA/Mixworks/.local/real-track-workflow-20261017/dropped-transition.png)

## Timing measurements

These measure displacement of recognizable source bass envelopes after stretching, **not downbeat accuracy or subjective sound quality**. An independent Python probe compares time-warped source envelopes with the actual isolated mixer renders. It uses a zero-phase 40–180 Hz band, 12 ms Gaussian smoothing, 241 ms windows, a ±50 ms lag search and correlation ≥0.95. Weak or ambiguous windows are excluded and their counts are retained. Time resolution is approximately 1 ms.

| Scenario | Track | Accepted / distinct windows | 95th percentile absolute displacement | Largest accepted displacement |
| --- | --- | ---: | ---: | ---: |
| Fixed 120 BPM | Ain't Nobody | 557 / 559 | 7 ms | 10 ms |
| Fixed 120 BPM | Miracle | 791 / 826 | 8 ms | 30 ms |
| Fixed 128 BPM | Ain't Nobody | 558 / 559 | 6 ms | 9 ms |
| Fixed 128 BPM | Miracle | 790 / 827 | 7 ms | 23 ms |
| Automatic 125→124 transition | Ain't Nobody | 559 / 559 | 3.1 ms | 7 ms |
| Automatic 125→124 transition | Miracle | 826 / 826 | 3 ms | 6 ms |
| Moved points, target 128 BPM | Ain't Nobody | 559 / 559 | 3 ms | 7 ms |
| Moved points, target 128 BPM | Miracle | 793 / 827 | 6 ms | 24 ms |

The initial isolated-peak diagnostic also picked individual bass cycles and ambiguous events, producing unstable outliers. Its `audio-measurements.json` is retained as exploratory evidence and is **not an acceptance measurement**. The envelope-transfer method above avoids that interpretation error; it still does not supply human musical labels or evaluate every audio sample.

Listening exports, rendered by the production mixer with 6 dB master headroom:

- [Automatic 125→124 BPM transition](../../.local/real-track-workflow-20261017/dropped-transition-mix.wav)
- [Moved transition points with a 128 BPM target](../../.local/real-track-workflow-20261017/edited-transition-mix.wav)
- [Fixed 120 BPM comparison](../../.local/real-track-validation-20261017/fixed-120-mix.wav)

No human listening pass, live audio-device test or physical-controller test is claimed. Rendered audio and numerical measurements do not establish that the transition is musically phrased correctly.

## Evidence and reproduction

Final v0.18 regression verification: **413 checks pass**, covering rhythm, beat grid, the main mixer/UI suite, playback cache, library and placement workflows. The separate real-song readiness result below remains failing. The deployed executable is `out/releases/0.18.0-8c8b0cbc62d3/Mixworks.exe`, SHA-256 `8c8b0cbc62d3ffb43ccd091207a2675abed2d0748831b096fd512e7b3c42d9cd`; `Mixworks.cmd` selects it through the backed-up launcher.

- `.local/real-track-validation-20261017/`: fixed 120/128 BPM and native-ramp renders, preliminary measurements, envelope measurements and source audit.
- `.local/real-track-workflow-20261017/`: fresh detection, real dropped/edited arrangements, native UI snapshots, mixer audio, `measurements-input.json`, `envelope-transfer.json`, and `mixmeister-profile-comparison.json`. The readiness run has **28 checks: 26 pass, 2 fail**, both failures being unavailable automatic first downbeats; exit 1 is intentional and retained.
- Each run's `run.json` retains its executable SHA-256. The final Cut-only refinement does not change the nonzero-overlap real-track scenarios.
- Both source MP3s, both existing analysis sidecars and both MixMeister sidecars retain identical before/after SHA-256 hashes in each six-file audit. The user's open session was not changed.
- `.local/evidence/mmengine-real-track-profile-20261017.json`: three additional REA records, 14,716 bytes, for the installed `MmEngine.dll` SHA-256 `92496ee0893ac2acc708e175f2b1536dee743ec540b75ef1e54086bb25d50861`. Vector serializer evidence `ev_2a4cc24552579bc333afd291762fb7270df102efdedf7afc1d882d7a96c7f7c3` corroborates the stored array representation; it does not establish every vector's musical meaning. The original serializer and builder evidence remain in the earlier bundles documented in [RHYTHM-RELIABILITY.md](RHYTHM-RELIABILITY.md). The REA session was closed.

The built executable accepts:

```text
--self-test --real-tracks-only --track-a "C:\absolute\track-a.mp3" --track-b "C:\absolute\track-b.mp3" --report "C:\new-test-folder\checks.txt"
```

Use a new output folder. This recomputes source analysis in memory, writes owned test WAVs/metadata and prepares an isolated playback cache. Add `--workflow-only` to run just dropped and edited transitions. `python scripts/measure-rhythm.py C:\new-test-folder` measures the resulting audio; it requires NumPy, SciPy and SoundFile. No dependencies were installed for this session.

The remaining acceptance work is trusted musical first-downbeat annotation and listening comparison with MixMeister, followed by regression coverage across more recordings, especially pickups, breakdowns, syncopation, half/double-time ambiguity and variable tempo. Two matched BPM values cannot replace that work.

> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# First-downbeat validation — v0.19.0, 17 October 2026

Both real tracks now receive an automatic first-downbeat anchor. The detector distinguishes the quiet opening percussion from the later full drums and keeps the established pulse grid unchanged.

| Recording | Mixworks first downbeat | Existing MixMeister first profile marker | Difference | BPM |
| --- | ---: | ---: | ---: | ---: |
| Sean Finn — Ain't Nobody (Funkera Extended Remix) | 0.844653 s | 0.839728 s | +4.925 ms | 125.0033 |
| Airsand — Miracle (Extended Mix) | 0.074300 s | 0.065737 s | +8.563 ms | 124.0007 |

These are comparisons with the **existing historical MXM sidecars**, not a new MixMeister run or a human listening annotation. Their first stored markers are also consistent with the opening waveform attacks and the phase of repeated phrase changes. Proprietary source identity and decoder-origin equivalence remain unresolved. The reference manifest records both MP3 and profile SHA-256 values; the test matches each MP3 digest and compares absolute source times with a 25 ms tolerance. A marker one or more whole bars late fails. The reference values are test inputs only; the detector does not read MXM files, names or track-specific constants.

## What REA established

Read-only Ghidra analysis of the installed `MmEngine.dll`, SHA-256 `92496ee0893ac2acc708e175f2b1536dee743ec540b75ef1e54086bb25d50861`:

- `0x10004330` counts sharp energy rises at cyclic grid positions, combines those counts with normalized interval energy and chooses the strongest position. The assembly shows the selected index being used to append an existing grid coordinate. This is more than choosing the globally loudest kick.
- `0x10005020` extends selected positions forwards and backwards in cycle-length steps, checks timing consistency through `0x100055d0`, and sorts the resulting markers. `0x1009c6d0` calls it before copying the markers into the saved profile.
- `0x1007da50` / `0x1007dd50` derive entry/exit ranges from those markers and thumbnail energy. `0x1007d880` accumulates energy thresholds for a separate vector; that vector is not a list of downbeats.
- Earlier evidence establishes that the refined grid uses an eight-position cycle and the serializer writes four counted 64-bit vectors. Exact musical meanings of every intermediate field and timing subdivision are not established.

The new bundle, `.local/evidence/mmengine-downbeat-v019.json`, contains **7 records / 119,559 bytes**. Key records: cyclic-selection assembly `ev_349535f979e98edb11eebdf33a711a73ce21805c68ef77fac6884026e45187bd`; extension `ev_05c64eb0adaf05f14cb5e6bf486471e90f12a8a8d1e0caeab7835a513d516287`; consistency check `ev_9cffba350e7091648b04a1276c9e4d7f2653e3f15d81e61c4ecb64bf86bbc762`; entry/exit selection `ev_777a67fb9e64b9d4224cf345b89c1a7207c5c738fac6197d37ac7cc4606443a8`. Earlier pseudocode and builder provenance remain in `.local/evidence/mmengine-rhythm-pipeline-v017.json`.

Ghidra marks a cleanup function as non-returning, which cuts parts of its pseudocode. Assembly corroborates the winner-selection/index write; unanalysed fallthrough and implicit register arguments remain limitations. This is evidence-informed independent implementation, not a claim to have recovered MixMeister's entire algorithm. The REA session was exported and closed; installed executable bytes were not changed.

## Implementation

`Source/Analysis.cpp` retains the existing recurring bass-accent inference and adds whole-track phrase evidence. It streams mono audio through bounded 60-second chunks, extracts broadband/bass/mid/treble energy per beat, and compares rises against both the previous beat and the same position one bar earlier. This avoids treating the ordinary kick/snare pattern as a new phrase on every repetition.

Prominent changes vote for one of four bar phases. At least three agreeing events spanning eight bars, a majority of the vote and a clear margin over the runner-up are required. Individual votes are capped; one loud crash or pickup cannot establish bar one. Conflicting accent/structure evidence remains unknown. The winning phase is extended backwards to its first audible attack. The activity check uses local attack contrast above a low track-relative floor, so quiet percussion can anchor the opening even when the full drums arrive much later.

The first experimental run correctly selected Sean Finn's phase but incorrectly skipped its quiet opening to 8.524 s. The revised activity check and a dedicated quiet-intro regression fix that failure; it is not counted as a successful result. Final output is 0.844653 s. Both source BPMs and pulse phases remain identical to v0.18.

Analysis revision **4** refreshes old automatic results while retaining manual corrections. Full-track downbeat coverage is recorded separately as `downbeatExaminedSeconds`; tempo/key retain their existing maximum 180-second window budget. The shared mono-conversion boundary also preserves the existing handling of antiphase stereo. Confidence values are evidence scores, not calibrated probabilities. This remains a 4/4 inference method; ambiguous/unstructured audio and variable tempo can still require correction.

## Verification

**420 regression checks pass**, exits 0: rhythm 23, beat grid 22, main mixer/UI 244, playback cache 52, library 42, placement 37. Main tests include the real gain VST3 and installed Vital. Results: `.local/verify-rhythm/v019-final/summary.json`. The final build log has no compiler warnings or errors.

New generated-score cases cover all four bar phases, delayed phrase cues, leading silence, one louder misleading pickup, quiet opening percussion, low-level antiphase stereo, too few structural cues and conflicting recurring accents. The existing equal-kick and isolated-pickup unknown cases still pass. Warm analysis, stale-cache refresh, session persistence and manual corrections pass through production boundaries.

**49 real-track checks pass**, exit 0, under `.local/real-track-downbeat-v019/`. Both files are decoded and analysed freshly in memory. Checks include absolute first-marker comparisons, fixed 120/128 BPM, a native-tempo ramp, dropped-track preparation and edited transition points. Both detected downbeats align to four-beat bars on the shared changing-tempo map. `measurements-input.json` retains source identities, coverage, reference provenance and mapping predictions; `run.json` retains the exact executable hash. `source-audit.json` confirms both MP3s and all four existing Mixworks/MixMeister sidecars are unchanged.

![Real tracks with inferred bar grids and two tempo controls](C:/Users/jayte/Desktop/REA/Mixworks/.local/real-track-downbeat-v019/dropped-transition.png)

The native component snapshots show `4/4 inferred` for both tracks. They exercise the actual UI code without opening or changing the user's session. No live desktop gestures, audio device or human listening pass is claimed.

Fresh independent envelope-transfer measurements use the same method as the [v0.18 report](REAL-TRACK-VALIDATION.md): 40–180 Hz energy, 12 ms smoothing, 241 ms time-warped source windows, ±50 ms search, correlation at least 0.95. These measure stretching displacement, not first-downbeat accuracy. Weak/ambiguous windows are excluded explicitly in `envelope-transfer.json`.

| Scenario | Track | Accepted / distinct windows | 95th percentile absolute displacement | Largest accepted displacement |
| --- | --- | ---: | ---: | ---: |
| Fixed 120 BPM | Ain't Nobody | 557 / 559 | 7 ms | 10 ms |
| Fixed 120 BPM | Miracle | 791 / 826 | 8 ms | 30 ms |
| Fixed 128 BPM | Ain't Nobody | 558 / 559 | 6 ms | 9 ms |
| Fixed 128 BPM | Miracle | 790 / 827 | 7 ms | 23 ms |
| Native ramp | Ain't Nobody | 559 / 559 | 3 ms | 7 ms |
| Native ramp | Miracle | 826 / 826 | 3 ms | 6 ms |
| Dropped 125→124 transition | Ain't Nobody | 559 / 559 | 3 ms | 8 ms |
| Dropped 125→124 transition | Miracle | 826 / 826 | 2.75 ms | 6 ms |
| Edited points, target 128 BPM | Ain't Nobody | 559 / 559 | 3 ms | 7 ms |
| Edited points, target 128 BPM | Miracle | 801 / 827 | 6 ms | 46 ms |

The edited Miracle render has isolated accepted outliers of −46 ms at clip-local 271.913 s, −35 ms at 237.695 s and +33 ms at 226.913 s. The new bar alignment changes the ramp placement and stretch trajectory. These are retained as an unresolved playback/measurement concern; the passing first-marker comparisons do not establish uniformly accurate stretching or subjective sound quality. The detector change does not modify the stretch engine.

Listening exports: [automatic transition](../../.local/real-track-downbeat-v019/dropped-transition-mix.wav), [edited 128 BPM transition](../../.local/real-track-downbeat-v019/edited-transition-mix.wav). Full isolated stems remain beside them for investigating the later outliers.

Deployed: `out/releases/0.19.0-080a61cde616/Mixworks.exe`, SHA-256 `080a61cde6160bc0f5c2796b864cc965196fd0d8237e63693b3fe3cde036e154`. This is the exact executable used for the final suites and real renders. `Mixworks.cmd` selects it through the backed-up launcher; earlier releases remain available.

Reproduce against a new output folder:

```text
Mixworks.exe --self-test --real-tracks-only --track-a "C:\absolute\track-a.mp3" --track-b "C:\absolute\track-b.mp3" --downbeat-reference "C:\absolute\Mixworks\tests\real-track-downbeats.json" --report "C:\new-output\checks.txt"
python scripts/measure-rhythm.py C:\new-output
```

`--analysis-only` skips transition preparation; `--workflow-only` keeps just dropped/edited transitions. Omitting the explicit reference file performs availability checks without implying first-marker accuracy. The reference manifest rejects different source digests. No dependencies were installed. Wider musical annotation and listening comparisons are still required before claiming general MixMeister parity; automated mix creation remains deferred.

> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Better Off Alone downbeat fix — v0.21.0, 17 October 2026

Better Off Alone now receives an automatic first downbeat. Ode To Omar retains its previous anchor. Both display `4/4 inferred` and align their bars to the shared, changing-tempo grid in the production timeline test.

| Recording | Before | v0.21.0 first downbeat | Reference | Difference |
| --- | ---: | ---: | --- | ---: |
| Airsand / TuraniQa — Better Off Alone (Radio Edit) | Unknown | **0.540735 s** | Existing MixMeister first profile marker: 0.527574 s | **+13.161 ms** |
| Be Like Butter & Enzo Siffredi — Ode To Omar (Original Mix) | 0.053062 s | **0.053062 s** | Previous Mixworks result, regression check only | Unchanged |

BPMs and pulse phases are unchanged: **124.0008 / 0.056867 s** and **127.0006 / 0.053062 s**. The Better Off Alone reference is 23266 / 44100 seconds, compared with a 25 ms tolerance. A whole-bar or half-bar error fails the absolute comparison. Ode To Omar does **not** have an independent MixMeister or human downbeat annotation; retaining its prior result is not proof of musical accuracy. Both source digests and reference provenance are explicit in `tests/real-track-better-off-alone.json`.

## Cause and change

The previous structural score gave recurring melodic changes in a drumless breakdown similar influence to strong drum entries. Better Off Alone's unweighted candidates split between phases 1 and 3, leaving no acceptable winner. A new generated-score stress test also exposed tonal leakage being treated as a recurring bass accent in a quiet window.

`BeatEnergy` now owns the shared 35–180 Hz attack and preceding-window measurements. Both accent and structural scoring consume those facts; the separate accent filter and duplicate beat/time interpretation were removed. Structural novelty is weighted by positive bass attack contrast, normalized against the track's upper-quartile attack strength and capped before local-peak selection. Weak melodic events therefore cannot inflate the supporting event count. A quiet accent window needs meaningful support relative to the same track-level reference before it can establish or veto a phase.

The existing recurrence, majority, winning-margin and conflict checks remain. Selection still extends backwards to the first audible attack, including quiet introductions. The detector does not read filenames, reference times or MXM files. Analysis revision **5** refreshes old automatic results through the existing cache boundary while retaining manual corrections. Session format **8** and prepared-audio revision **2** are unchanged.

## REA evidence

Read-only Ghidra analysis of installed `MmEngine.dll`, SHA-256 `92496ee0893ac2acc708e175f2b1536dee743ec540b75ef1e54086bb25d50861`, provider Ghidra 12.1.4:

- `0x10004330` combines cyclic sharp-rise counts with normalized interval strength. Evidence `ev_87c7306d225619f4df1a40ac4df6eedc281a5037a02ae8b3248b0ee28f6c6970`.
- `0x100840b0` scans a float interval and returns its **maximum**, clarifying the strength calculation used above. Evidence `ev_91f1ef08ba777e1dc4e23154147aff5d07ebf9a4b0ff0e29d7c4e5032106e65a`.
- `0x1007d6a0` separately finds the first thumbnail value above a relative activity threshold; this is not itself proof of bar phase. Evidence `ev_6f6402fee83ed6cd3e26fc8f602d4a6855369a0284857dd1b87146ff95e1a981`.

Bundle: `.local/evidence/mmengine-better-off-alone-v021.json`, **3 records / 44,165 bytes**. Earlier serializer/builder evidence established the profile's sample-rate field and counted 64-bit vectors. The existing Better Off Alone MXM contains 23266 as the first entry in both its entry-range vector and selected-marker vector; its SHA-256 is `b9424d1d54bce5e95f41358624040eec23fd294c721d39b1fe393c1c20600807`. Decoded prefix values and uninterpreted trailing bytes are preserved in `.local/better-off-alone-baseline/mixmeister-profile-0.json`.

This is an evidence-informed independent detector, not recovered source or exact MixMeister equivalence. Ghidra's non-returning-cleanup assumptions and implicit register parameters remain limitations. The session was exported and closed without modifying the installed binary. The MXM is a **historical reference**, associated by filename; original source association and decoder-origin equivalence are not authenticated. A fresh Fusion UI attempt showed a missing NewBlue plugin notice and did not complete the reference run; no fresh profile or MixMeister A/B audio is claimed. Isolated MP3 copies remain in `.local/mixmeister-better-off-alone-v021/`.

## Verification

- **451 regression checks, zero failures**: 27 rhythm, 22 beat-grid, 271 main (including real gain VST3/Vital), 52 playback cache, 42 library, 37 placement. Results: `.local/verify-rhythm/v021-final/summary.json`.
- New score-grounded cases cover melodic breakdown interference in all four bar phases, including reduced gain. Existing unknown/equal-kick, isolated-pickup, conflicting-evidence, quiet-intro and cache/manual-correction checks pass.
- **71 real-track checks, zero failures** for Better Off Alone/Ode To Omar: fresh analysis, absolute references, fixed 120/128 BPM, native ramp, dropped transition, edited tempo points, snapped moves and free moves. Production mixer WAVs, full isolated stems and native component snapshots are in `.local/better-off-alone-v021-final/`.
- **17 additional checks** protect the earlier Sean Finn/Miracle pair: anchors remain **0.844653 / 0.074300 s**, with **+4.925 / +8.563 ms** differences from their historical MixMeister references. Results: `.local/prior-pair-v021-percussion/`.
- Real-test audits confirm all five existing source/sidecar files for the new pair and six for the earlier pair are unchanged. The real tests use read-only analysis access.
- The initial baseline has one failure (Better Off Alone unknown). The first broadband-weighted candidate passes the real pair but fails four new stress cases; those failed results remain under `v021-probe`. They are not included in the passing totals.

![Both tracks with inferred bar grids](C:/Users/jayte/Desktop/REA/Mixworks/.local/better-off-alone-v021-final/dropped-transition.png)

The screenshot was generated by the production UI component test and visually inspected. It is not a fresh physical desktop gesture or listening pass.

Independent bass-envelope transfer measurements use the established script (`scripts/measure-rhythm.py`), with explicit weak/ambiguous-window exclusions. These measure stretching displacement, not musical downbeat correctness:

| Scenario | Better Off Alone: p95 / maximum | Ode To Omar: p95 / maximum |
| --- | ---: | ---: |
| Fixed 120 BPM | 8 / 39 ms | 8 / 45 ms |
| Fixed 128 BPM | 7 / 46 ms | 7 / 9 ms |
| Native 124→127 ramp | 3 / 7 ms | 2.85 / 8 ms |
| Dropped 124→127 transition | **4 / 6 ms** | **3 / 6 ms** |
| Edited points, 128 BPM target | 4 / 7 ms | 6 / 12 ms |
| Snapped move | 4 / 7 ms | 3 / 8 ms |
| Free move | 4 / 8 ms | 3 / 8 ms |

The dropped transition accepts all **520 / 684** examined distinct envelopes. Full accepted/excluded counts and per-window lags are in `envelope-transfer.json`. Fixed-tempo outliers up to **46 ms** remain an unresolved stretching/measurement concern, separate from this downbeat fix. No listening-quality or general MixMeister-parity claim is made. Placement alone took **0.012 ms** for both snapped moves and **0.010 ms** for both free moves; this excludes rendering/report I/O.

Listening samples: [automatic transition](C:/Users/jayte/Desktop/REA/Mixworks/.local/better-off-alone-v021-final/dropped-transition-mix.wav) and [edited target](C:/Users/jayte/Desktop/REA/Mixworks/.local/better-off-alone-v021-final/edited-transition-mix.wav).

## Release and reproduction

Deployed `out/releases/0.21.0-2b3994b9d382/Mixworks.exe`, SHA-256 **2B3994B9D382FC91592E93A48F7345D878036F4890E46669BFBE424D43BC63F3**, matching the executable used for verification. The final build log `.local/build-v021-percussion.log` has no compiler warnings/errors. `Mixworks.cmd` selects the new release through a backed-up launcher; earlier releases remain available. The existing v0.20.0 UI session was not closed or overwritten. Save it before relaunching; reopening refreshes automatic analysis, and **Beatmatch** rebuilds an existing arrangement against the new grid if desired.

```text
Mixworks.exe --self-test --real-tracks-only --track-a "C:\Users\jayte\Music\01. Airsand - Better Off Alone (Radio Edit).mp3" --track-b "C:\Users\jayte\Music\09. Ode To Omar (Original Mix).mp3" --downbeat-reference "C:\Users\jayte\Desktop\REA\Mixworks\tests\real-track-better-off-alone.json" --report "C:\new-output\checks.txt"
python scripts/measure-rhythm.py C:\new-output
```

No dependencies or subagents were added. Automated mix creation remains deferred.

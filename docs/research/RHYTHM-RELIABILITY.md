> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Beat grid and tempo reliability — v0.17.0

Current downbeat results and additional REA evidence are in [FIRST-DOWNBEAT-VALIDATION.md](FIRST-DOWNBEAT-VALIDATION.md): both real tracks now pass absolute comparisons with the existing MixMeister markers. The [v0.18 two-track report](REAL-TRACK-VALIDATION.md) and the v0.17 observations below retain the earlier failures as historical baselines. Tempo matching and first-downbeat identification remain separate acceptance requirements.

The first beat/downbeat, source grid and audible beatmatching are the priority for this app. This release fixes reproduced timing errors; it does not establish general MixMeister equivalence or reliable automatic bar-one detection for every song.

## Changes and measured evidence

- **Correct BPM, wrong phase:** controlled 123.4567 BPM rhythms with kicks, syncopated hats and offbeat bass previously displaced the grid by 122–182 ms. A separate percussive energy detector, waveform timing refinement and retained attack observations now place these fixtures within **1.11 ms**, without measured long-track drift at the test's precision. Broadband spectral analysis remains a fallback when bass evidence is weak. Strong disagreements still require review.
- **First downbeat:** repeated median bass accents replace averages that could let one loud pickup invent bar one. Equal accents, conflicting patterns and uncertain meter remain unknown. Manual first-downbeat corrections remain authoritative.
- **Tempo matching:** fixed and ramped preparation share SoundTouch settings of 40 ms sequence, 15 ms search and 8 ms overlap. This reduces transient movement compared with its longer automatic sequences at slow tempos. In actual two-track mixer output at 96/125/150 BPM, worst grid displacement was **12.20 ms** and worst inter-track separation **3.99 ms**. The separate 120→144 BPM overlap measured **7.07 ms** grid displacement and **6.89 ms** separation. These are fixture measurements, not guarantees for arbitrary audio or extreme ratios.
- **Source corrections:** each prepared tempo mapping retains its source BPM. A later BPM correction rebuilds duration and pitch-preserving PCM even when the mix tempo map is unchanged. Before the fix, a 120→128 source correction could leave the old duration and playback preparation active. Four-decimal manual BPM values survive opening the correction editor.
- **Saved analysis:** revision 3 invalidates old automatic detection; prepared PCM uses a separate new cache identity. Valid old library snapshots retain waveforms and manual corrections while awaiting refresh. Stale results cannot be promoted to the current revision simply by saving a snapshot. Playback refreshes them first, including when manual corrections exist only in the catalogue. External sidecar changes still participate in refresh selection.

Tests: **407 checks, zero failures**, all process exits 0. Main 244 (real gain VST3 and installed Vital), rhythm reliability 11, beat grid 22, playback cache 52, library 42, placement 36. Build has no compiler diagnostics. Evidence is under `.local/verify-rhythm/final/`; initial reproduced failures are `red.txt` and `guards-red.txt` in the parent folder. An intermediate runner incorrectly quoted suite flags and ran overlapping main suites against shared fixtures. Its `check-*.txt` results are invalid for release verification; the final runner uses correctly passed flags, separate directories and sequential UI suites.

## Real reference tracks

Both files were analysed read-only with the production decoder/detector. SHA-256 checks confirm both audio files and both existing analysis sidecars are unchanged.

| Track | Estimated BPM | Source beat phase | Fitted pulse RMS |
| --- | ---: | ---: | ---: |
| Sean Finn — Ain't Nobody (Funkera Extended Remix) | 125.0033 | 0.364666 s | 9.87 ms |
| Airsand — Miracle (Extended Mix) | 124.0007 | 0.074300 s | 7.08 ms |

The phase is a repeating pulse origin, not proof of the first musical downbeat. Both bar-one anchors remain unknown. These BPM values agree to displayed precision with the previously observed Fusion playlist values of 125 and 124, but no new listening comparison or manually annotated beat evaluation was performed. The new RMS statistic measures attacks matched to the fitted pulse; it is **not directly comparable** to the old all-selected-onset RMS and is not an accuracy percentage. Confidence and the inconsistent-grid flag remain separate. The examination budget remains three windows, at most 180 seconds; unsampled tempo changes remain possible.

## MixMeister investigation

REA/Ghidra inspected the installed `MmEngine.dll` without executing or modifying it. SHA-256: `92496ee0893ac2acc708e175f2b1536dee743ec540b75ef1e54086bb25d50861`. The complete 21-record evidence bundle is `.local/evidence/mmengine-rhythm-pipeline-v017.json` (193,118 bytes). The REA session was closed after export.

Observed static pipeline:

| Function | Observation | Evidence |
| --- | --- | --- |
| `0x1009d6c0` / `0x1009d8c0` | Profile work-item dispatch reaches the profile builder and saves its results. | `ev_6e1c4855ea57a5f4327400a3c60a64e79a5cf073e20ff0287cb96b288377f0be`, `ev_6b44e6ab4b4b2fa590c19a51aaaee4f2481e62816c8dfdc6d0767195604e4ccc` |
| `0x1009c6d0` | Builds PCM thumbnails, runs tempo estimation/refinement, constructs `CPfBeatPattern` values and writes the profile. | `ev_602730108f246907766ae81000e0616f374a3245ef1cc5c47bb4fd38fbe76ec3` |
| `0x10014830` | Evaluates nine positions separated by one tenth of a source span, calls the estimator with subdivision arguments 4 and 8, and takes the median of nonzero estimates. | `ev_83422d9040ece3fe4c68390f66f4f54f306af65d234fc0913504375bbecf7e3e` |
| `0x10014980` | Candidate sweep from 82 to below 164 in 0.1 steps; counts event remainders in 40 phase bins. This is one internal stage, not the application's complete supported BPM range. | `ev_4f8d4a9055078a851397604930076c369f0dae0e3c12cfec5dfa520e71d92b26` |
| `0x10005730` | Refines period from event spacings, calls another fitting routine, converts the refined period back to BPM, and builds grid data. | `ev_16047accc2ff23fb657dd4625c958bbf8c32ca24270778eb9ac53ebff3a1deb1` |
| `0x10004330` / `0x10004c10` | Accumulates event/energy evidence by cyclic grid position; the latter normalizes pattern energies. This supports an inference of accent-pattern analysis, not a complete recovered downbeat contract. | `ev_87c7306d225619f4df1a40ac4df6eedc281a5037a02ae8b3248b0ee28f6c6970`, `ev_46e023e823e8c4c1a66572e392f23e4536d1a6eadae2561b85323828fefda953` |

Ghidra reports no-return/truncation and recovered-type limitations in several routines. Pseudocode is not original source or execution evidence. Exact first-beat selection, all profile-vector meanings, source authentication for `.mxm`, and Beat Slicer playback DSP remain unresolved. Nothing imports `.mxm` guesses or calls Fusion's DLL from Mixworks. Mixworks' detector and SoundTouch preparation are independent implementations. The skills.sh directory was consulted for methods; no additional skill, dependency or toolchain was installed.

The next verification priority is annotated real-track first-beat/downbeat and listening comparisons, including breakdowns, syncopation, half/double-time ambiguity and variable-tempo recordings. Variable source-tempo correction and general meter/phrase inference are not implemented.

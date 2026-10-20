> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Automatic detection and timeline alignment — v0.31.0

**Later verification, 18 October 2026:** the [77-track DJ.Studio audit](DJSTUDIO-REAL-MUSIC-AUDIT.md) found substantial remaining tempo, key and downbeat failures. The four-track and synthetic results below describe their specific coverage, not general musical accuracy. Release remains 0.31.0. Runtime probing also found that DJ.Studio's native AI addon returned no key field, despite its worker expecting one.

Newly inserted tracks align their inferred or corrected 4/4 bars with the timeline bars. A usable pulse grid is aligned even when bar one is unknown. This applies to Library double-click, Library drag and Explorer insertion, including background source hydration. Explicit free moves and saved arrangements remain intact. After opening an older arrangement, **Beatmatch** rebuilds placement using the refreshed analysis.

Full source audio is preserved. The first clip can have a short leading gap; an incoming clip selects the next valid bar at the requested overlap, so the actual overlap can be up to one bar shorter. Native BPM targets remain at zero for the first track and IN for subsequent tracks. No automatic volume fades are added.

## Reproduced problem and change

On Fabio Florido's **The Bliss (Original Mix)**, the old detector reported 130.0318 BPM, an unstable grid and no downbeat. Downbeat analysis was skipped entirely. Three 60-second windows gave a syncopated middle passage excessive influence; the global fit also combined one window's period with another window's phase. Ordinary insertion separately aligned file edges instead of source beats.

The detector now keeps each measured tempo/phase hypothesis together. Long tracks distribute the same 180-second budget across nine windows; shorter sources retain one or three. Whole-track downbeat analysis distinguishes sustained bass/midrange section entries from recurring treble fills, accounts for overlapping filter bands, and weights strong events while retaining bounded votes, recurrence and disagreement checks. Four synthetic bar phases with misleading treble fills now pass alongside the existing silence, pickup, conflicting-accent and breakdown tests. Equal unaccented pulses still do not invent bar one.

Analysis revision **6** refreshes automatic results through the existing backed-up cache boundary, preserving manual corrections. The cache accepts all nine producer-owned windows. Session format remains **12**. Timeline labels distinguish **downbeat unknown** from **beat grid needs review**; neither is disguised as a successful detection.

## Measured results

| Exact local recording | BPM | First downbeat | Difference from historical MixMeister marker |
| --- | ---: | ---: | ---: |
| The Bliss | 130.0029 | 0.149802 s | +6.129 ms |
| Miracle | 124.0011 | 0.075557 s | +9.820 ms |
| Ain't Nobody — Sean Finn | 125.0035 | 0.844418 s | +4.691 ms |
| Better Off Alone | 123.9991 | 0.537328 s | +9.754 ms |

References are digest-bound in `tests/real-track-detection.json`; tolerance is 25 ms in absolute source time, so whole-bar mistakes fail. They are historical MXM markers, not fresh Fusion analysis or human annotations. Proprietary audio/profile identity and decoder-origin equivalence are not authenticated. For The Bliss, the first selected marker is frame 6336 at 44100 Hz; its later entry-range marker identifies the full-drum entrance, not the first musical downbeat. Neither filenames nor MXM values are detector inputs.

**538 regression checks and 98 real-track checks pass**, with zero failures. `.local/detection-final/summary.json` records eight suites and the exact executable hash. Coverage includes all 24 synthetic major/minor keys, detuning, noise/single-note rejection, long fractional BPM drift, nine-window cold/warm cache reuse, stale analysis/manual corrections, native drag hydration, changing-tempo bar placement, source-span preservation, free moves, save/reopen, live insertion and real VST3 gain/Vital loading. `.local/detection-real-final/` contains fresh analysis, production mixer renders, native component screenshots and before/after audits confirming the four audio files and their existing MXM files were unchanged.

The Bliss remains **11A / F# minor** (displayed enharmonically as Gb minor). Key confidence is a heuristic, not an accuracy percentage. No independently trusted key annotation or Mixed In Key result for these recordings was available; commercial key-detection parity is not established. No human listening or physical desktop gesture test is claimed. Variable-tempo source warping and general meter detection remain outside this implementation.

The attempted older Ode To Omar reference did not match the current file digest, so it was explicitly excluded from the reference pass; its old regression-only annotation was not silently reassigned. Earlier unsuccessful detector candidates and test runs remain under `.local/bliss-*` and `.local/detection-*`.

Independent envelope-transfer measurements compare the actual rendered stems with time-warped source envelopes (40–180 Hz, 12 ms smoothing, 241 ms windows, correlation at least 0.95). For the automatic transition, 895/904 Bliss windows and 798/827 Miracle windows were accepted: 95th-percentile displacement was 6 ms / 0 ms, with maxima 31 ms / 0 ms. Across edited transitions, moves and linked handles, the largest accepted displacement was 35 ms. Weak/ambiguous windows are explicitly excluded in `envelope-transfer.json`. These residual stretch outliers remain a limitation; this measurement establishes neither subjective sound quality nor musical downbeat correctness.

Deployed **0.31.0-9a177ca96709**, SHA-256 `9a177ca96709bdd62caf93244e157222f68d12d4a4b7e3164a19ea1fcd27474a`, matching both final verification manifests. The launcher was backed up and points to the immutable release. Save and reopen through `Mixworks.cmd`; existing arrangements need explicit **Beatmatch** to adopt the new placement, while new insertions align automatically. No installed DJ software, original audio or user session was rewritten.

![Production timeline after automatic insertion](../../.local/detection-real-final/bliss/dropped-transition.png)

## REA and method research

**MixMeister Fusion.** Reused the prior 21-record REA/Ghidra investigation after verifying that installed `MmEngine.dll` still has SHA-256 `92496ee0893ac2acc708e175f2b1536dee743ec540b75ef1e54086bb25d50861`. Imported those records into the current REA session. `0x10014830` samples nine source positions and takes a median of successful tempo estimates (`ev_83422d9040ece3fe4c68390f66f4f54f306af65d234fc0913504375bbecf7e3e`); `0x10014980` scores event remainders in phase bins (`ev_4f8d4a9055078a851397604930076c369f0dae0e3c12cfec5dfa520e71d92b26`); `0x10005730` refines the event-spacing fit (`ev_16047accc2ff23fb657dd4625c958bbf8c32ca24270778eb9ac53ebff3a1deb1`); `0x10004330` combines cyclic sharp-rise and interval-energy evidence (`ev_87c7306d225619f4df1a40ac4df6eedc281a5037a02ae8b3248b0ee28f6c6970`). These are static/decompiler observations, with recovered-type and control-flow limitations. Mixworks uses an independent implementation; exact Fusion DSP and key algorithms are not claimed recovered.

**Installed DJ.Studio 4.2.15.** REA inspected `C:/Program Files/DJ.Studio/resources/app.asar`, SHA-256 `3058f424ada256bcaca0bce3f867e71b10e72fed1c8ec6a15877fe674d55cf48`, Evidence `ev_ba8f152fb47f604fdc516ed44c8eb6b01f68cd399a114f2c0f0f88b94cdedf83`. Complete evidence is retained at `.local/evidence/djstudio-analysis-v031.json`; selected, hashed source bytes are under `.local/djstudio-selected/`. Static worker code loads a native `ai-beatgrid.node`, passes audio through shared memory to `processAsync`, receives beat arrays plus a key, and handles an empty result explicitly. It distinguishes regular and flex model families, accepts a BPM range/fold count, and postprocesses beat arrays. Its phrase path is separate. The package includes LibTorch/ONNX runtime dependencies and `.enc` model files; model internals and actual inference accuracy were not recovered or executed. Several unpacked-file hashes contradict ASAR-declared integrity; REA retained observed bytes with `record-and-continue`, so publisher integrity is not asserted. The installed application and user projects were untouched.

The useful design lesson is to retain measured beats and uncertainty separately from playback tempo, bar phase and trusted key provenance. DJ.Studio's main code also parses Mixed In Key beat/cue metadata; this is evidence of import support, not evidence that the MIK detector itself was recovered.

**Public methods, checked 18 October 2026.** Current [DJ.Studio documentation](https://help.dj.studio/en/articles/14113156-understanding-tempo-bpm-and-beat-grids-in-dj-studio) describes a newer Smart Beatgrid with per-beat timing and optional stabilization; this must not be confused with the installed 4.2.15 implementation. Its [key settings](https://help.dj.studio/en/articles/8105773-settings-menu) keep DJ.Studio, licensed Mixed In Key and imported metadata as separate selectable sources. [Mixed In Key's Traktor workflow](https://mixedinkey.com/workflows/use-mixed-in-key-with-traktor/) leaves BPM/grid analysis with Traktor and transfers key/cue information; it does not disclose the proprietary key algorithm.

[Mixxx's analyzer design](https://github.com/mixxxdj/mixxx/wiki/Developer-Guide-Analysers) distinguishes Queen Mary onset/probabilistic beat tracking from SoundTouch's scalar BPM estimate, and offers Queen Mary/KeyFinder key analysis. This reinforces why a BPM number alone cannot establish beat or bar alignment. [Beat This!](https://github.com/CPJKU/beat_this) is a published neural beat/downbeat tracker whose code and weights are MIT licensed; it is a candidate for a broader future detector evaluation, not a dependency silently added here. Any replacement needs annotated real-track evaluation and validation of its Windows runtime/resource footprint. The [skills directory](https://skills.sh/) was consulted; the existing REA and native JUCE workflow fitted this task without additional skills or dependencies.

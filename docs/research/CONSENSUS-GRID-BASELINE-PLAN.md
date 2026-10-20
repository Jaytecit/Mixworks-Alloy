# MixMeister and DJ.Studio grid accuracy investigation plan

**Status:** Planned investigation, 19 October 2026. No tracks have qualified under this protocol yet. No detector changes, reference-app runs or production-data resets are part of writing this plan.

**Goal:** Establish at least seven distinct recordings for which fresh MixMeister and DJ.Studio results agree on tempo, beat timing, four-beat bar phase and musical key, and independently verify their musical grid alignment. Freeze that reference before measuring Alloy, then identify the producing boundary responsible for each difference.

**Method:** Separate source identity and decoder timing, application analysis, independent musical annotations, project placement, rendering and display. Preserve raw results alongside one normalized interpretation per producer. Execute sequentially, recording the evidence and decision at each gate.

**Scope:** Constant-tempo 4/4 music. A reliable downbeat anchor anywhere in the source establishes bar phase backwards and forwards; finding the earliest downbeat is optional. Key agreement is required for the strict complete-agreement set but measured separately from timing. Rhythm-only matches remain useful comparison cases and do not count towards the seven full-agreement tracks.

## Guided process walkthrough before bulk screening

The user's follow-up asks to see MixMeister's process translated into understandable steps, then the equivalent Alloy process, so they can guide the investigation using specific evidence. Make this the first pilot deliverable. Start with one previously used development recording, establish its source identity and clock, and retain both automatic results before inviting any corrections. DJ.Studio becomes the third comparison once the two-program walkthrough is understandable.

Show the same source passage and these questions: **What sounds mark the pulse? How fast does it repeat? Where do the grid lines start? Which beat is beat one? Does that source grid map correctly into the mix?** These are comparison questions, not an assertion that both programs execute identical stages in that order.

| Walkthrough element | Required behaviour |
| --- | --- |
| Shared audio and time | A source waveform on one verified sample clock, both programs' beat markers, stronger downbeat markers and measured signed offsets. Switchable A/B audition uses the same source audio with the chosen grid's clicks and an accented beat one |
| Human-readable evidence | At each available decision show what was measured, candidate tempos/phases where actually retained, the selected value, the relevant rule or score and remaining uncertainty. Explain observed rules rather than inventing an internal narrative |
| MixMeister visibility | Distinguish values observed in this run, behaviours established by recovered code, interpretations and unobserved proprietary steps. Existing evidence identifies nine-position tempo sampling/median selection, event-phase scoring and spacing refinement; it does not expose every live intermediate or establish the complete downbeat/key algorithm |
| Alloy visibility | Capture actual source-owned neural events, physical attacks, window BPM/phase proposals, local conflict checks, final pulse/bar facts and placement inputs/outputs. Existing diagnostics expose some results; incremental emission and a live viewer still require separate instrumentation. Do not present the final summary as a complete live trace |
| Live mode | Show only actual events as the program produces them. Record monotonic capture time separately from the audio interval examined. Batch/full-track stages may remain pending until they return; do not animate invented intermediate decisions |
| Replay mode | Pause, step and replay the retained real capture at a readable speed, synchronized to the source passage. Label replay and preserve the original event ordering/timestamps. A conceptual explanation must be labelled separately from either live mode or replay |
| User guidance | Let the user identify a passage, mark a candidate pulse/downbeat, or report an audible problem. Save these as review annotations with provenance; keep the original automatic runs and reference qualification unchanged |
| Operational safety | Use owned copies and projects; preserve production data and the running mix. Prefer passive native outputs or read-only observation for MixMeister. Do not patch it or pause its process under a debugger merely to make a display look live |

First establish whether supported read-only observation can expose the relevant MixMeister intermediate values. If it cannot, the honest comparison is its captured input/final grid plus a labelled explanation of the recovered rules, alongside Alloy's actual trace. Do not promise full live visibility into a closed-source engine. For Alloy, keep trace collection bounded and outside the audio callback, and verify that enabling it does not change automatic results or create a second interpretation of detector facts.

**Walkthrough acceptance:** The user can hear/locate the same passage, distinguish pulse from bar phase, see an actual measured disagreement or explicit agreement, identify what each displayed claim is based on, and pause at a specific event to guide the next diagnostic step. No simulated readings are labelled as live values. The seven-track qualification and frozen-reference protocol below remain mandatory.

## 1 Preserve the current state and freeze the protocol

- [ ] Record the deployed Alloy executable/model/runtime hashes, analysis revision and session format. The starting release is 0.54.3, analysis revision 9, session format 25; verify actual identities when execution begins.
- [ ] Back up the research documents and relevant source before any instrumentation. Inventory the real roaming/local Mixworks data and hash its analysis, correction, session, library, recovery and mapping documents. Keep all original audio, analysis, mixes, mappings and prepared/decoded caches.
- [ ] Inventory the installed MixMeister and DJ.Studio versions, binaries, analysis engines, selected key source, tempo range, grid mode and relevant import/export settings. Record observations rather than assuming that current online documentation describes the installed versions.
- [ ] Freeze the candidate order, normalization rules, tolerances, annotation procedure and exclusion rules below before looking at Alloy differences. Hash the protocol and screening manifest. Later amendments create a new protocol version and retain the earlier results.

Use fresh investigation-owned projects, copied inputs and isolated Alloy analysis/output folders. Leave the user's open session running. Creating a new project must not replace an unsaved reference-app session. Do not delete global reference-app caches, rewrite source tags/sidecars, install/update applications or enable cloud processing. Fresh analysis must be established by the application's actual reanalysis completion and provenance; a new filename alone does not establish a cache miss.

## 2 Select a candidate pool without contaminating the reference

- [ ] Start with 20–30 previously used development recordings drawn from the existing authenticated comparison inventory. Exclude the eight untouched reserved recordings before opening or analysing candidates. Keep the separate eight already-evaluated holdouts out of initial tuning selection as well.
- [ ] Select distinct recordings and audio masters, rather than counting format conversions or duplicate releases as additional tracks. Include different tempos, pickups, quiet intros, sparse/broken drums, strong offbeat bass, breakdowns and long durations where available. Log the available diversity rather than claiming coverage the seven cannot provide.
- [ ] Register the entire ordered pool before screening. Retain every rejection and its reason. Expand the pool in recorded batches if necessary; never remove inconvenient failures from the screening report.

Existing 77-track DJ.Studio comparisons are candidate-finding evidence. Historical MixMeister MXM sidecars are leads, not fresh authenticated references: earlier reports explicitly left proprietary source identity and decoder-origin equivalence unresolved. Do not select on how well Alloy already performs.

**Gate:** At least seven distinct tracks must pass every strict qualification criterion. If fewer pass, report the count, failure reasons and additional evidence needed. Do not loosen thresholds or manually correct an automatic grid to reach seven.

## 3 Establish a common source clock

- [ ] Record each original's absolute path, SHA-256, file dimensions and exact version/mix. Preserve the original and any existing sidecars unchanged.
- [ ] Decode an investigation copy once with a pinned decoder. Retain canonical PCM at its native sample rate, channel count and complete decoded length, including leading silence. Record decoder identity/options, PCM format, frame count and PCM digest. Use an identical lossless WAV copy in each application when the verified supported format permits; do not resample, normalize, stretch, filter or trim it.
- [ ] Run an additional original-format lane for all qualifying tracks. Keep original-file and canonical-WAV results distinct: canonical agreement alone does not establish agreement on the user's compressed source.

Strict qualification requires the timing criteria in both lanes, including each application's cross-lane consistency after the independently measured decoder-clock conversion. Preserve and diagnose original-versus-WAV differences rather than silently selecting the better input format.
- [ ] Establish each application's decoded-audio origin against canonical PCM using waveform correlation at multiple identifiable passages near the beginning, middle and end. Resolve repeated-pattern correlation ambiguity with distinctive landmarks and channel checks. Record encoder delay, padding, resampling and any constant capture/export latency separately.

Use integer sample indices for observed audio positions and float64 seconds for fractional predicted beat positions. Preserve raw units and producer precision. Do not fit a timing correction from the beat grids being compared: that would remove the phase or BPM error under investigation. Only independently measured decoder/capture mappings may transform native timestamps into canonical source time. A time-varying disagreement belongs in the results, not in a corrective warp.

**Calibration gate:** Validate timestamp conversion and project-grid arithmetic to one sample on constructed files with known positions. Verify the capture/decoder mapping's repeatability and uncertainty at no worse than 0.5 ms per application. Ambiguous correlation or insufficient timestamp precision produces an inconclusive measurement, not a pass. Sample-level representation does not imply sample-level musical truth.

## 4 Capture fresh final native results from both applications

- [ ] Analyse identical copies in fresh owned projects with original automatic analysis, without manual beat/BPM/key corrections or inherited tags overriding the detector. Record exactly which engine generated each field. A licensed/imported Mixed In Key result is a separate provenance lane, not DJ.Studio's native key detector.
- [ ] Capture the grid that the application actually displays and uses for playback after its normal postprocessing. Retain its raw detector output separately when available. Earlier DJ.Studio investigations showed that raw native AI and final processed grids can disagree; do not substitute one for the other.
- [ ] Preserve exact scalar tempo, all beat timestamps, downbeat labels/anchors, fixed/flexible status, grid changes, unknown spans and key outputs, with unrounded values where the producer exposes them. Retain native project/analysis files, screenshots and settings alongside parsed facts. If a producer exposes only a rounded BPM or an uninterpreted marker, keep its precision/meaning unknown until validated.
- [ ] Repeat analysis in a second fresh owned run and reopen the saved projects. Compare complete grids, field provenance and keys. Repeatability within 1 ms at every comparable beat is required; no manual edits may explain the second run.

Prefer native saved/exported data and public workflows. Validate marker meaning against the native UI and audible playback on a pilot track before applying a parser to the pool. A profile's first marker is not automatically beat one. If format interpretation requires REA, use read-only evidence, preserve raw bytes, and keep unproven fields unknown. Do not patch an installed application or introduce another DJ application's reanalysis into the measurement chain.

Keep DJ.Studio fixed/smart/flexible modes and stabilization explicit. A warped export can make a drifting source sound regular; it cannot qualify that original as constant-tempo. WAV exports are audio evidence, not proof of the saved grid. Current DJ.Studio documentation distinguishes these workflows, but installed-version behaviour must be observed.

**Pilot gate:** One track must complete identity, clock calibration, final-grid extraction, repeat analysis and independent review before bulk screening. If either application's grid or decoder clock remains inaccessible, resolve that measurement gap before claiming seven-track agreement.

## 5 Establish independent musical annotations

- [ ] Make two annotation passes with vendor and Alloy grids hidden. Prefer two musically competent listeners; if only one is available, use separate blinded passes and report the reduced independence. Model-generated labels do not substitute for a listener.
- [ ] Review each complete recording. Mark every audibly supported beat at high waveform resolution, with explicit uncertainty intervals. Use a consistent attack/metrical-position convention, rather than choosing the loudest sample or every low-frequency peak. Distinguish an offbeat bass accent or expressive attack from the musical pulse.
- [ ] Assign beat-within-bar labels from phrasing, harmonic movement, percussion patterns and repeated structural changes. Establish downbeat anchors in the opening, middle and ending supported regions and on both sides of breakdowns. A four-on-the-floor kick alone cannot establish beat one.
- [ ] Label silence, ambiguous events and unsupported intervals explicitly. A silent interval has no directly observable beat attack: record its grid as inferred, corroborated before/after where possible. Any unresolved tempo or bar-phase change prevents strict qualification. Do not quietly omit audible difficult passages.
- [ ] Adjudicate disagreements before revealing application grids. Retain both original annotation passes and the adjudication. For timing qualification, each accepted reference event must have uncertainty no larger than ±2 ms. If that precision cannot be established, retain the track for exploratory comparison rather than certifying 5 ms accuracy.
- [ ] Record an independently reviewed stable tonal centre/mode and any modulation or ambiguity. Normalize enharmonic spelling and equivalent key notation while preserving raw labels. Relative major/minor, compatible Camelot neighbours and half/double tempos are not exact agreement. Ambiguous or unresolved keys prevent entry into the full-agreement set, while leaving a separately labelled timing-only result possible.

## 6 Apply predeclared numerical acceptance criteria

These are proposed engineering targets for this investigation, not universal perceptual limits or already achieved results. "Perfect" means passing all of them within measured uncertainty; literal zero-error musical truth is not asserted.

| Measurement | Strict reference-set requirement |
| --- | --- |
| Recording and playback pulse | Same authenticated recording, constant 4/4, same quarter-note pulse; zero half/double-tempo substitutions or missing/extra pulse assignments |
| Beat correspondence | One monotonic, one-to-one beat index mapping across the full source; zero beat slips. Keep unmatched/missing events explicit. A single fixed index-origin difference is allowed and recorded |
| MixMeister versus DJ.Studio timing | Absolute difference plus combined measurement uncertainty ≤5 ms at **every** corresponding beat, and absolute median signed bias plus its uncertainty ≤2 ms |
| Accumulated disagreement | Start-to-end error change and the range of consecutive eight-bar median errors ≤2 ms, with uncertainty included. Evaluate the complete arrays and report every failing window |
| Tempo-rate agreement | Unrounded effective rates differ by at most `min(0.001, B × 0.002 / L)` BPM, where `B` is the reference rate and `L` is the evaluated duration in seconds; the direct timestamp drift test is authoritative |
| Independent musical alignment | Each application's error plus annotation/capture uncertainty ≤5 ms at every annotated, supported beat; zero adjudicated downbeat disagreements |
| Bar phase | Same beat-one label modulo four throughout. Anchors separated by whole bars can express the same phase; preserve different raw origins/bar numbering without treating the earliest anchor as mandatory |
| Repeatability | Two fresh automatic runs and save/reopen retain beat positions within 1 ms, with identical pulse/bar labels and key meaning |
| Musical key | Same normalized tonic and mode from documented native sources, supported by the independent review; unknown or ambiguous results do not pass |

For a ten-minute 120 BPM track, the drift budget implies a rate difference of approximately **0.0004 BPM**, much stricter than matching a display rounded to 120.00. Report the raw scalar BPM, rate derived from the full grid and fixed-grid fit residuals separately. Neither rounding nor refitting may repair a failed candidate.

For every comparison, report signed median error, median absolute error, p95, maximum absolute error, first/last error, drift in ms/minute, local deviations, beat/bar mismatches, event counts and uncertainty. Apply limits to each track individually. Aggregate averages, p95 alone and circular phase wrapping must not conceal a single local failure or whole-beat/bar error.

Broad beat-tracking scores are supplementary only. For example, mir_eval's documented default F-measure tolerance is 70 ms, which cannot certify this 5 ms target. Include the beginning of the source; do not use its customary five-second trim. No beat, downbeat or key availability flag counts as correctness.

## 7 Freeze at least seven qualifying references before opening Alloy results

- [ ] Produce a selection table showing every screened track, native versions/settings, provenance, repeatability, timing errors, bar/key results, coverage and pass/reject/inconclusive reasons.
- [ ] Freeze the qualifying source identities, both native grids and project files, canonical audio identities, independent annotations, calibration, scoring rules and signed-off decisions. Keep both reference engines individually; do not average them into a new grid that hides their differences.
- [ ] Allocate four of the first seven qualifying tracks to diagnosis and three to sealed confirmation before viewing Alloy errors. If more qualify, retain additional confirmation tracks. Opening a confirmation result for tuning reclassifies it as development evidence.

Seven tracks establish a precise local baseline, not population-wide detector parity. Retain rejected and conflicting tracks as a challenge set. The eight untouched reserved recordings remain untouched; any later generalization study is a separate step.

## 8 Measure the frozen Alloy release without manual overrides

- [ ] Use an immutable copy of the deployed 0.54.3 executable with its exact model/runtime, fresh isolated analysis/preparation stores and newly created mixes. Capture fresh automatic values before making any correction. Existing user analysis, manual overrides and saved arrangement clocks must not feed this run.
- [ ] Analyse both canonical copies and originals. Compare Alloy separately against MixMeister, DJ.Studio and the independent annotations using the same source clock and metrics. Record disagreements with either vendor even when Alloy is closer to the listener's reference.
- [ ] Plot signed beat error against source time, including expanded opening/middle/ending and worst-case regions; show downbeat classes, local BPM, source attacks, predicted beats, uncertainty and supported coverage. Keep low-confidence and missing regions visible.
- [ ] Diagnose source BPM, pulse phase, bar phase and key separately. Exporting a correctly placed grid or manually nudging it cannot establish that detection was correct.

Existing `scripts/run-real-benchmark.py` and `scripts/compare-djstudio.py` are reusable starting points, not sufficient full-track certification. The older comparison accepts broader tolerances; its decoder alignment uses PCM heads. Extend or replace those assumptions at their owning boundary rather than bolting new interpretations onto reports. Analysis-only `RealTrackTests.cpp` currently exports at most 30 seconds of decoded audio: retain full-source clock evidence separately. Audit actual cache isolation and provenance before invoking a reused runner.

## 9 Separate placement, rendering and display from detection

- [ ] First test one unprocessed track at its native tempo. Inspect source-grid coordinates, clip source offset, captured playback clock and project-grid mapping separately. Measure the audio export against the expected source waveform; a screen pixel or playhead position is not the timing clock.
- [ ] For each of the seven tracks, test a source downbeat placed at a known nonzero project bar, pickup audio before it, a trim with nonzero source offset, explicit Beatmatch, movement, slip, Undo/Redo and save/reopen. Verify extension of the same phase before and after the chosen anchor.
- [ ] Repeat with fixed project tempos 120 and 128 BPM, adding 123.4567 when needed to exercise a non-integer stretch ratio; then use a defined 120→128 BPM ramp over 32 project bars. For a changing map use integrated beat position `Q(t) = integral(BPM(t)/60 dt)` and its inverse, not one seconds-to-beats ratio for the entire clip.
- [ ] Test seven adjacent pairs in a preregistered circular order so every track appears as incoming and outgoing. Use the existing four lanes with sequential clips. Keep detector tests, alignment-only playback and transitions with fades/EQ as separate measured conditions.

For a source anchor `s0` placed at project time `p0`, the fixed-source-tempo mapping is `Q(p) = Q(p0) + (s - s0) × Bsource / 60`. `Q(p0)` must lie on a four-beat boundary for bar alignment. Retain the original source anchor even when it lies outside the trimmed audible span. Newly created mixes test new analysis; reopening an old saved clock is a different ownership test.

**Separate playback targets:** Mathematical mapping must agree to one output sample; native-tempo, no-effects audio must have residual lag no larger than one sample after independently calibrated capture delay. For stretched audio, require p95 additional displacement ≤3 ms and maximum ≤5 ms, with measurement uncertainty included. Combining the reference and rendering budgets permits at most 10 ms end-to-end displacement and must be reported as such, rather than called sample-perfect audio.

Measure stretched displacement using multiple independent supported attack/envelope windows and inspect every maximum. Retain ambiguous/rejected correlations and their coverage. Require accepted measurements in every supported musical region and at least 90% of preregistered eligible attack windows; otherwise the render result is inconclusive. Silence has no measurable attack displacement. Do not use a single global correlation shift, arbitrary per-window realignment, or another detector's reanalysis of the exported audio as ground truth. Historical stretch residuals are context and do not waive these stricter targets.

| Error pattern | Boundary to investigate first |
| --- | --- |
| Constant shift across a source | Decoder origin, timestamp units, pulse calibration or source offset |
| Approximately linear growth | Effective BPM, fractional precision, duration/sample-rate conversion |
| Wrong downbeat with correct pulse | Meter/bar inference and anchor identity |
| Local jumps or nonlinear drift | Local timing evidence, edits, genuine source variation or fixed/flexible model mismatch |
| Source facts correct, project mapping wrong | Captured source clock, trimming/slip, cue ownership or tempo-map integration |
| Numeric mapping correct, exported attacks displaced | Preparation/stretching, latency or rendering |
| Audio and numeric mapping correct, screen wrong | Waveform/ruler geometry, display rounding or cursor presentation |
| Key differs while timing passes | Key source, tonic/mode ambiguity or key estimation; do not alter timing to compensate |

These are diagnostic hypotheses. Confirm the producing cause before modifying it, and preserve counterexamples.

## 10 Evidence layout and measurement-tool requirements

Each execution owns `.local/consensus-grid-baseline/<run-id>/` with these outputs:

| Artifact | Responsibility |
| --- | --- |
| `protocol.json`, `applications.json`, `sources.json` | Protocol hash, exact versions/settings, original/canonical identity, provenance and pool order |
| `raw/mixmeister/`, `raw/djstudio/`, `raw/alloy/` | Immutable native projects/exports/logs/screenshots and exact analysis outputs |
| `calibration.json` | Clock transformations, evidence, precision and uncertainty; no beat-derived corrective offsets |
| `annotations/` | Both blinded passes, event uncertainty, musical-role labels, unknown spans and adjudication |
| `normalized/` | One normalized grid per producer: source identity, native raw value/unit, canonical time, beat index, beat-within-bar, observed/inferred/unknown status and originating evidence reference |
| `screening.csv`, `reference-set.json` | All selection/rejection decisions, unique qualifying sources and frozen diagnosis/confirmation split |
| `metrics.csv`, `errors.json`, `plots/`, `renders/` | Every event/window error, coverage, uncertainty, numeric/visual evidence and original rendered outputs |
| `REPORT.md`, `manifest-sha256.json` | Plain-language outcome, unresolved cases, reproducible invocations and artifact identities |

If new tooling is necessary, plan one owner for normalized contracts/metrics in `scripts/measure-consensus-grid.py`, with format-specific parsers at the input boundary and focused tests in `tests/test_consensus_grid.py`. These files do not exist as part of this planning task. Prefer existing Python/NumPy/SciPy capabilities; do not add packages or change production code merely to start screening.

Before accepting tool output, prove it rejects a wrong source hash, wrong units/sample rate, malformed/partial grids, duplicate sources, an odd padded final probe counted twice, missing coverage, a one/two-beat shift, half/double tempo and a local tempo/phase jump. Prove that a whole-bar anchor-origin change retains phase equivalence while preserving raw origins. Use constructed known grids to test exact tolerance boundaries, +1/+5/+6 ms offsets, accumulated drift and uncertainty-inclusive pass/fail. Verify round-trip sample/time conversion at 44.1 and 48 kHz. Gate on named failures and nonzero exit status, not a favourable summary plot.

## 11 Decide whether a change is justified

- [ ] Complete and freeze the reference set before any detector tuning.
- [ ] Produce an Alloy difference report naming the affected source, region, measured discrepancy, uncertainty, producing owner, expected result and smallest falsifiable hypothesis. A mismatch with one application alone is insufficient evidence that Alloy needs changing.
- [ ] If a confirmed defect needs a fix, capture its failing production-path test first, change one producing boundary and rerun the same measurement. No track names, reference grids, per-track BPM constants or tuned offsets may enter production detection.
- [ ] Recheck the diagnosis tracks and retained challenge cases, then unseal confirmation tracks once. Report all failures and coverage changes. If confirmation prompts further tuning, those tracks no longer provide untouched confirmation.
- [ ] For a changed executable, complete the existing 22-suite receipt gate plus the frozen musical/placement/render comparisons. These are complementary: passing native tests alone does not establish musical correctness.
- [ ] Update the handover with exact identities, confirmed findings, data preservation and remaining unknowns. Establishing this baseline does not trigger the detector-development AppData reset. Any eventual detector release must follow the then-applicable retention/deployment instructions; this plan does not authorise a reset or deployment.

The final outcome is either seven or more qualified references with a measured Alloy diagnosis, or an explicit shortfall/inconclusive result explaining which requirement could not be established. Manual corrections can be retained as separate user-workflow evidence but cannot turn failed automatic analysis into a passing automatic baseline.

## Supporting references

- [Current timing assessment](../RHYTHM-TIMING-REVIEW.md), [historical 77-track comparison](DETECTION-V032.md) and [first-downbeat provenance limits](FIRST-DOWNBEAT-VALIDATION.md).
- [DJ.Studio tempo and beat grids](https://help.dj.studio/en/articles/14113156-understanding-tempo-bpm-and-beat-grids-in-dj-studio) and [tempo controls](https://help.dj.studio/en/articles/8261956-bpm-controls-tips-and-faqs): fixed/smart grids, manual changes and export behaviour. Treat these as current public documentation, not proof of an installed version's behaviour.
- [mir_eval beat evaluation](https://mir-eval.readthedocs.io/latest/api/beat.html): conventional event metrics, default timing tolerance and customary beginning trim. The stricter targets above are investigation choices.

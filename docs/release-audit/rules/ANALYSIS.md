# Library, analysis and source review

Inherit RULES; F-MEDIA/F-GRID/F-CORPUS with independent identities. Scope library/analysis groups, Model media reader, PCM/library stores and UI consumers. All mandatory, NOT RUN.

### MW-L01 — Offered audio formats decode completely and safely
- Contract/source: Main/Workstation chooser, Model::readMedia, JuceMp3Length patch, docs/IMPORT-SCAN.md.
- Cases/procedure: WAV/AIF/AIFF/FLAC/OGG/MP3, mono/stereo/antiphase, 8/44.1/48/96/192kHz as accepted by reader; CBR/VBR MP3 with/without Xing/VBRI and padding, short/truncated/corrupt/header-only/oversized/non-audio renamed input, Unicode/path-length boundaries. Hash before/after; use independent decoded frame-count/PCM references.
- Pass/oracle: accepted complete decoded duration/channels/rate and end samples match independent decoder accounting for declared codec delay/padding; lossless ≤1 source quantization step, no silent truncated success. Unsupported dimensions reject before unsafe allocation and identify reason. MP3 patch version/identity reproducible, original untouched.
- Evidence/mode/severity: per-file decode facts/reference version/hash/error, import runtime/CLI and final attack probes; automated; P1.

### MW-L02 — Scan/import queues expose accurate durable outcomes
- Contract/source: MusicScanner, ImportProgressView/ImportRecovery, docs/IMPORT-SCAN.md and remaining572 historical checkpoint.
- Cases/procedure: owned recursive tree including duplicate paths/content, unreadable/missing/unsupported files and directory links; manual/auto scan/stop/scan-report, import during scan, close mid-file and retry; one long source plus short interactive import.
- Pass/oracle: each discovered candidate gets a truthful imported/reused/failed/skipped outcome; stop cooperates and retains completed results without claiming remaining success; interactive imports do not wait for an entire idle scan. Reports and catalogue re-read agree. Never resume production missing-import recovery as an audit side effect.
- Evidence/mode/severity: independently enumerated fixture tree, queue events/counters, durable scan JSON/catalogue and cancellation/retry; automated/native; P1.

### MW-L03 — Catalogue use does not hydrate every source or gate on unused missing media
- Contract/source: LibraryCatalogue/LibraryWorkflows and Engine::prepareProject; current admission contract and historical AUD-003.
- Cases/procedure: saved required clips plus large metadata-only library; unused missing/corrupt source versus required one, cold/warm caches, select/add a cold row, cancelled load and valid retry.
- Pass/oracle: required arrangement alone controls playback readiness; unused entries remain identifiable/unavailable, opening does not decode all metadata-only sources; selected add hydrates the named source; failed required load retains current project. Inspect decoder invocation counts/file traces, not just elapsed time.
- Evidence/mode/severity: required-ID set, decode counts/stage timing, cache hits and final state; automated/integration; P1.

### MW-L04 — Search, key/tempo sorting and selection retain source identity
- Contract/source: LibraryTable, Playlist and TempoBrowserTests; compact note/Camelot/confidence descriptions.
- Cases/procedure: unknown/manual/low/high confidence keys and equal BPM/name rows; note/full-name/Camelot searches, selected-reference similarity sorts, toggle key display, filter while analysis publishes, multi-select/import/drag/right-click, missing artwork.
- Pass/oracle: exact stable source IDs follow row reorder/filter, unknowns sort as specified, all labels represent stored facts/evidence bands rather than probabilities. Targeted correction/play/drag affects highlighted source. Use hand-authored sort/search expected lists and source hashes.
- Evidence/mode/severity: expected/actual ID lists, native browser interactions and captures; automated/manual; P2.

### MW-L05 — Preview obeys its route and does not disturb programme
- Contract/source: LibraryPreview, Engine preview callback, PrivatePreparation and docs/LIBRARY-PREVIEW.md.
- Cases/procedure: preview/stop/change selection/seek/loop during load, end/short/missing input; Arrange versus Record/Perform, programme already playing, private unavailable/disconnect, stale loader completion and close.
- Pass/oracle: latest request owns preview, original source/arrangement/take selection/Undo unaffected; Arrange routes to master by contract, Perform preview only to private with no fallback to audience. Stale completion cannot restart another source. Device PCM capture verifies routing independently of UI.
- Evidence/mode/severity: request IDs/state, captured programme/private channels and physical route walk; automated + hardware/listening; P0 for private bleed, otherwise P1.

### MW-L06 — Cached analysis and PCM reuse follow source identity
- Contract/source: AnalysisCache/LibraryAssets/PcmCache/ContentDigest; analysis11, captured preparation clocks and PCM decoder keys.
- Cases/procedure: same bytes rename, same path modified content with size/mtime collision, changed dimensions/decoder/revision, missing/partial cache, future record, manual corrections and readOnly/refresh/focused modes. Repeat cold/warm and source changed during work.
- Pass/oracle: only compatible source-bound facts reuse; incompatible automatic analysis invalidates without silently losing deliberate corrections/reference evidence; PCM cannot reuse stale frames. Cache hits avoid work only when independently verified hashes/config allow it. Failed publication cannot leave a reusable partial entry.
- Evidence/mode/severity: identity/revision decision table, measured decode/analysis calls, stored hashes and recovery; automated; P1.

### MW-N01 — Actual analysis engine and failure path match release claims
- Contract/source: Analysis/NeuralRhythm/RhythmDecoder, manifest, Auftakt research separation; no invented alternate engine claim.
- Cases/procedure: inspect call graph/config/model/hash/preprocess/postprocess; missing/corrupt/wrong model/runtime in separate owned bundle, unavailable inference and cancellation waiting on serialized model; silence/antiphase inputs. Inspect model30s context borders and actual whole-source coverage.
- Pass/oracle: intended configured engine is called, provenance/method/error/confidence exposed accurately; failed neural results never masquerade as valid Beat This output; physical fallback/abstention is labelled and no partial unearned cache. Compare frozen activations/preprocess fixtures with independent reference where available.
- Evidence/mode/severity: call/config/input-output trace, model/runtime SHA, golden transform/decoder comparison and fault results; static + automated; P1.

### MW-N02 — Constant-tempo beat pulse is accurate independently of bar phase
- Contract/source: AGENTS detection priority, SourceBeatGrid, physical/transient/Neural algorithms, docs/RHYTHM-TIMING-REVIEW.md.
- Cases/procedure: F-GRID known 40–300BPM partitions, nonzero onset, silence/pickups, soft/broad attacks, subdivisions/syncopation, antiphase, half/double ambiguity and long track. Analyse fresh without authored corrections; compare matched beats at beginning/middle/end and report octave errors separately.
- Pass/oracle: report BPM error, accumulated drift, signed/absolute onset residual p50/p95/max, beat precision/recall and coverage. Synthetic unambiguous target: ≤0.1% BPM error and p95≤10ms/max≤20ms pulse residual (provisional REQ-02; justified by edit/onset timing, not current pass rate). Corpus threshold/corpus size must be approved before PASS for general accuracy; do not score only accepted easy tracks.
- Evidence/mode/severity: authored samples/reference annotations, raw auto outputs, source-level metrics/failures/abstentions with no manual override; automated + reviewed listening corpus; P1.

### MW-N03 — Downbeat confidence distinguishes bar classification from pulse
- Contract/source: user requirement reliable anchor anywhere, MusicAnalysis downbeatOrigin/method/guess, protected DownbeatReferences.
- Cases/procedure: known bar accents after silence/pickup and deliberately ambiguous four-floor kicks; late anchor projected backward/forward; F-CORPUS reviewed bar-phase choices keyed by exact source SHA. Include beat-correct/bar-wrong and no-evidence cases.
- Pass/oracle: modulo4 class accuracy and anchor residual measured separately from N02; unambiguous synthetic bar class100%, residual within N02 bound; ambiguous material abstains/labels guess rather than claiming first kick=beat1. Reviewed choices are dated observations, not unquestionable truth; resolve disagreement independently. Corpus mandatory acceptance threshold is REQ-02 BLOCKED until set.
- Evidence/mode/severity: independently reviewed phase table, automatic-versus-manual fields, confidence/guess/abstention and wrong-bar examples; automated + human/listening; P1.

### MW-N04 — Unsupported or ambiguous rhythms do not earn false assurance
- Contract/source: variableTempo/gridError/confidence fields and current 4/4 scope; library eligibility/needs-review consumers.
- Cases/procedure: changing-tempo, rubato, non4/4, no rhythm, silence, short intro and ambiguous octave fixtures; try arrange/review/retry and manual override.
- Pass/oracle: no claim of supported accurate bar grid on excluded material; review/unknown/warning remains visible where intended, bounded finite results, manual workflows available within limits. Mandatory safe handling, not successful variable-meter detection. Independent fixture classification and traces of eligibility determine outcome.
- Evidence/mode/severity: outputs/confidence/consumer decisions and UI captures for all partitions; automated/manual; P1.

### MW-N05 — Key/tuning confidence and eligibility agree
- Contract/source: Analysis.h `keyReviewConfidence=.55`, key encoding C0..B23/-1, SourceCorrection and AutoArrangement eligibility.
- Cases/procedure: independently synthesized tonal major/minor/tuned/detuned chords, silence/noise/percussion/relative-key ambiguity and reviewed corpus; values just below/at/above.55, manual key and guess; key display/search/arranger/retry consumers.
- Pass/oracle: common threshold applies everywhere, Manual and Guess remain distinct, unknown has no fabricated confidence probability. Unambiguous generated tones/chords yield expected key/tuning under declared test construction; report corpus confusion matrix/coverage and REQ-03 threshold separately. No useful key evidence discarded because tempo fails.
- Evidence/mode/severity: independent pitch/key truth, consumer boundary cases and raw metrics; automated + corpus review; P1 for eligibility mismatch, otherwise P2.

### MW-N06 — Manual source corrections save exactly the reviewed intent
- Contract/source: GridCorrection/SourceCorrection UI and T11; source-only save versus save/rebuild contract.
- Cases/procedure: BPM fine±0.01/half/double, pulse/bar anchor late, key override, valid min/max and invalid/nonfinite/locked/stale target; save success/failure and cancel/reopen; source-only versus explicit arrangement rebuild.
- Pass/oracle: independent store read shows exact accepted fields and preserved other overrides; source-only leaves prepared placement clock unchanged, rebuild updates only declared scope atomically/Undo. Failed save cannot show saved success. Append-only reference observation records source identity/automatic/prior/chosen facts without overwriting older reviews.
- Evidence/mode/severity: disk state plus arrangement/grid PCM, dialog target and reference hashes; automated/native/manual; P1.

### MW-N07 — Focused retry is bounded, per-field and source-bound
- Contract/source: retry revision3 and mergeAnalysisRetry, LibraryWorkflows, docs/FOCUSED-ANALYSIS.md/ANALYSIS-RETRY-OPTIONS.md.
- Cases/procedure: tempo-only/key-only/both, accepted manual/pulse with missing bar evidence, three separated≤20s passages for >30s source, short source, unavailable rhythm model, method repeated/new, changed source; tempo failure with useful key and cancellation midway.
- Pass/oracle: only requested unsupported/low-evidence fields reconsidered; accepted/manual fields survive; raw SoundTouch direct/half/double/disagreement evidence retained, never blindly adopted; failed/cancelled attempt doesn't poison eligibility/retry marker. Reports include before/candidate/result/passages/reasons and survive reload.
- Evidence/mode/severity: independently prepared field-decision table, passage bounds/raw values, stored retry/report identities and rerun decisions; automated; P1.

### MW-N08 — Energy and sections are measured suggestions with preserved manual edits
- Contract/source: MusicalEnergy/EnergySections/SourceSectionStore, docs/AUTO-ARRANGEMENT.md/SECTION-FLAGS.md.
- Cases/procedure: authored low/high activity versus RMS-inverted clips, four-bar and two-bar breaks/returns, insufficient hints, manual deletion/move and explicit rescan, profile revision change, cancellation/profile publication.
- Pass/oracle: energy direction follows declared rhythm/bass/dynamics evidence rather than loudness alone; RMS source evidence is not perceived loudness/LUFS or an automatic gain instruction (Model waveformRms and AutoArrangement comments explicitly distinguish this). Automatic sections remain tentative, not semantic verse/chorus claims; preserved manual annotations/deletions and source-time positions, no partial profile publication. Independent authored change points/feature ordering and musical review are oracles.
- Evidence/mode/severity: feature vectors, source sample/time annotations, cache/manual diffs and listening notes; automated + listening; P1/P2 by consumer impact.

### MW-N09 — Asynchronous analysis cannot overwrite a newer decision
- Contract/source: workers, generation/request IDs, cache publication and refresh/cancel ownership.
- Cases/procedure: source replace, new correction, removal/reimport, library reorder, new project/close and retry while analysis is pending; cancel at decode/features/inference/cache commit, then valid retry.
- Pass/oracle: stale job never commits into wrong source/project or replaces newer manual/reference state; completed compatible results may remain cached only as documented; pending counters/children settle and app remains usable. Interleave deterministically using barriers/hooks, not random sleep timing.
- Evidence/mode/severity: generation/source IDs, deterministic interleaving logs, before/after disk/state and retry; automated/integration; P1.

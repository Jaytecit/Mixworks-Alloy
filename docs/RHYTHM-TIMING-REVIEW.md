# Timing changes and grid review

## Desired-outcome assessment — 19 October 2026

The user's priority is **constant-tempo 4/4 music**. Beat alignment requires accurate BPM and pulse timing. Bar alignment additionally requires a reliable downbeat anchor anywhere in the source, with that phase extended backwards/forwards and mapped onto project bar lines. Finding the earliest downbeat is optional for arrangement. Four kicks in a bar do not imply four downbeats. Variable-tempo playback and other meters are deferred. Detector agreement and low residuals are evidence, not independent musical truth.

| Outcome | Current assessment | Remaining boundary |
| --- | --- | --- |
| Beat timing and stable BPM | A steady 125 BPM construction exposed a -213.35 ms calibration shift from the correct neural pulse onto loud offbeat bass. The repair retains the scored pulse at .83/3.43 ms start/end error in the C++ control. Standard, backbeat-heavy and pickup controls also pass. | These are independently scored controls, not a guarantee for every recording. |
| Timing changes between sampled windows | Reproduced a coverage gap in deployed 0.43: a 26-second 150 BPM passage and a 125 ms phase edit inside a seven-minute 120 BPM construction both incorrectly retained a steady-grid state. | See the continuous physical timing repair below; short or weak unsupported changes still require review. |
| Downbeat anchor and bar alignment | A clear recurring beat-one passage after 60 seconds establishes 4/4 phase. A source anchor at 60.213 seconds maps earlier/later bars onto project bar lines, including after trimming. Pulse-only alignment remains available when bar one is unknown. | The bass-heavy loop alone has competing half-bar predictions and may remain unknown; it must not publish a wrong confident bar. Earliest-anchor accuracy is not an alignment requirement. |
| Key, alternative and tuning | Tuning-aware key estimation, competing keys, heuristic confidence and manual correction are implemented and tested. | Tonal ambiguity, modulation and commercial-detector disagreements remain unresolved; no new key-accuracy claim is made here. |
| Confidence and coverage | Scores are labelled as evidence rather than probabilities; unavailable support is explicit in timing diagnostics. | Scores are not calibrated musical-accuracy percentages. |
| Phrase/section selection | Structural changes contribute evidence for bar phase. | Semantic section labels and automatic phrase-aware transition selection remain unimplemented. |
| Development state | The user explicitly chose to clear retained analysis, corrections, audio preparation, library snapshots and recovered mixes with each verified detector update, rather than every launch. | Original music, source and verification evidence remain intact. New detector evaluations use fresh analysis and fresh arrangements. |

### Constant-tempo pulse calibration repair

The ordinary same-tempo calibration gate previously allowed the physical fit to move neural phase by up to .45 of a beat when physical confidence was high. In the bass-heavy scored control, raw neural events followed the actual quarter-note beats (median7ms / p9513ms), but that gate shifted the grid by -213.35ms onto a different rhythmic layer. A low residual and correct BPM concealed the musical timing error.

Ordinary refinement uses the existing neural timing-fit tolerance: the smaller of40ms and .12 beat. A larger correction within the previous phase bounds additionally needs stronger support from recurring sharp physical attacks. The percussion boundary measures relative RMS rise across12ms; calibration compares source-owned sharp-onset support at both proposed phases, with bounded attack weights and one vote per beat. Slow bass intensity alone cannot authorize the shift. This distinction also preserves legitimate larger corrections when the model follows syncopated hats instead of the kick pulse. Separately corroborated metrical-level repairs retain their existing evidence requirements.

The constant-4/4 controls check beat timing across the source, BPM, false bar assertions and reliable bar availability where the construction supplies clear evidence. They include the inherited150-second hat/bass controls, which a blanket40ms correction cap would regress. A separate96-second control adds clear bar-one evidence only after60seconds. Automatic source anchors at60.213seconds verify both directions of grid extension and project placement, with and without source trimming. UI correction text now says **downbeat anchor** and accepts an anchor anywhere in the source.

### Continuous physical timing repair

The detector now examines physical timing in contiguous 20-second windows independently of neural event availability. The distributed windows still calibrate the scalar BPM/phase estimate; their sampling budget no longer determines local consistency coverage. One shared percussion/attack boundary supplies both roles. Local consistency uses each local observation once, with its own measured coverage. The existing estimator needs five seconds for periodicity; shorter tails and silence can retain unknown support. Model weights and tonal estimation are unchanged.

Physical local fits supply their own tempo/phase proposals alongside neural proposals. This matters because the model can omit beats during an actual tempo change: in the preserved tempo-gap diagnostic, the neural predictions leave a 14.36-second hole in the changing passage. Waiting for strong neural disagreement before examining that passage reproduces the blind spot. Existing physical agreement, coverage and conflict rules decide whether a proposal contradicts the fixed grid; neural absence alone never proves a change.

A corroborated tempo or phase conflict suppresses the global downbeat and excludes automatic synchronization. Missing alternate beats alone must remain a steady-grid case. Physical processing retains only the current bounded window; optional diagnostics retain its observations in source-time order. Analysis revision 9 invalidates older automatic facts while preserving valid caller corrections and captured arrangements.

Deployed0.44.0-78c2334ec91d is frozen under `.local/detection-outcomes-35/bundle-05/`, executable SHA256 `78c2334ec91d206300d53e833baf1945767f3abfa0f01b27c0e44a49828652ef`. Its21focused constant4/4 checks,76complete rhythm checks and **1,735checks across22release suites** pass with zero failures/errors. All162source/harness hashes remain unchanged. The native source-grid editor image was inspected; the anchor labels fit. VerifyOnly and actual deployment accepted the exact receipt.

The same executable passes87/87real two-track workflow checks on Ain't Nobody and Better Off Alone, covering fresh analysis, placement, linked edits, save/reopen with sample-identical playback, and rendering. The historical first-profile-marker comparisons are additional observations of existing cue behaviour, not the definition of a valid downbeat anchor or independent musical truth.

All24compared timing/bar/key/decoded-head facts match deployed0.43 on seven previously used development recordings. Both complete benchmarks retain the same musical-review cases: Nude has no known downbeat, and Bottle needs grid/bar review. There are no strict execution failures. `development-equivalence-05.json`, the source ZIP/manifest, focused red/green reports, complete regression receipt and real-workflow report retain provenance. Reserved recordings remain excluded. The change does not establish universal beat-one accuracy or resolve ambiguous keys.

After verification/deployment,37owned runtime AppData targets were cleared per the user's development preference: analysis records, manual corrections/library snapshots and backups, audio preparation caches, legacy analysis archive and recovered mixes. Original music/source/evidence and layout preferences were excluded. See `state-reset.json`. Future detector updates use the same reset policy; ordinary launches reuse current valid analysis. Evaluate with fresh mixes so old captured clocks cannot mask detector changes.

## Earlier timing repair

These timing improvements were introduced in accepted0.38.0-64b7c4339b30, after1,513 checks across22 suites, exact source/harness verification and the seven-recording comparison. Subsequent releases retain them; FINISHING-PLAN.md identifies the current launcher. Previous releases and launcher backups remain available.

A recording can agree with a scalar tempo in one passage while changing tempo or shifting phase elsewhere. Phase calibration does not prove that one fixed grid explains the whole source. The candidate retains independent physical onset measurements even when a global pulse fit fails, then compares local timing against the proposed fixed grid. Measurements have one owner; scalar fits borrow them, and the same parsed evidence supplies the research report.

Timing agreement and occupied grid predictions are separate facts. Missing beats and simple subdivisions do not alone prove a tempo change. A conflict needs sustained, independently supported local timing that contradicts the global grid. Silent or insufficiently measured intervals retain unknown support. Model weights, preprocessing and existing scalar tempo/key thresholds are unchanged. New physical-corroboration checks determine when a fixed grid needs review.

Sources with a corroborated tempo or phase conflict receive the existing **beat grid needs review** state. Automatic preparation leaves them unsynced and does not publish a confident all-source bar phase. The reported scalar BPM remains an estimate; the flag must be considered with it. Existing explicit manual corrections remain caller choices. A single BPM override cannot repair source drift; multiple source-owned correction anchors remain future work.

Saved arrangements retain their captured playback clocks and source spans when refreshed detection facts require review. The new detector facts use analysis revision8, so stale revision7 automatic results are regenerated. Existing correction values and arrangement geometry are retained; the session format remains19.

## Measured scope

The preserved40-second constructed source changes from120 to150BPM. Version0.37 incorrectly called it steady after phase calibration. The candidate measures a local physical fit of149.99994BPM, with48 inlier attacks among49 observed rises. The neural local timing agreement is about.781 and occupied predictions about.488: their product explains why the earlier combined-score check failed to flag it. The candidate marks the recording for review and retains an unknown bar phase. A separate125ms phase-edit construction also receives review; steady percussion and missing alternate attacks remain steady.

Construction tests exercise those positive/negative cases, automatic-placement exclusion, preserved saved clocks, quiet-interval unknowns and cancellation. They are source-timing truth, not musical labels or evidence of commercial-engine parity.

Frozen0.37 and the exact candidate were both tested on the same seven previously used development recordings. All compared tempo, phase, confidence, review, bar and key facts and decoded PCM heads are exactly equal. Both benchmarks finish with the same musical-review cases and no structural execution errors. The eight reserved recordings remain untouched. This bounded comparison does not validate every recording or resolve the previously known beat-one/key disagreements.

Evidence is under `.local/rhythm-conflicts-27/`: the controlled source manifest and0.37 failures, frozen candidate/source hashes, `development-equivalence-02.json`, paired benchmark manifests and the full gate. The optional `--automatic-control` flag on `--rhythm-research` adds source-owned local fits and physical events, with explicit nulls for unavailable support. It does not admit the experimental recovered AUFTAKT decoder into automatic analysis. The separate REA findings for AUFTAKT, TONART, BARBEATQ, ELASTIQUE, MixMeister and DJ.Studio remain in the reconstruction/audit documents.

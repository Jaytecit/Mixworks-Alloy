> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# DJ.Studio comparison on real music — 18 October 2026

Follow-up implementation and measured candidate results: [detection and alignment candidate](DETECTION-V032.md). The original v0.31 audit below remains unchanged as the baseline.

**Mixworks 0.31.0 is not yet reliable enough for unattended beat, downbeat and key detection across this library.** The earlier four-recording checks did not establish general musical accuracy. This audit reproduces concrete tempo errors and exposes substantial key and bar-phase disagreement. It does not change the detector or deploy a new release.

## Measured results

77 complete recordings, **7 hours 6 minutes** of music: 22 melodic house/techno, 19 dance, 11 techno, 11 electronic/house/funk, 10 house, 3 electro and 1 electronic. The sample is the entire existing DJ.Studio library, with reference tempos from 120 to 137 BPM and durations from 2:02 to 8:44. It is wider than the previous four-track test, but does not represent every genre or tempo in the user's larger library.

| Measurement | Result |
|---|---:|
| Source audio identity verified against DJ.Studio | 77 / 77 |
| Fresh Mixworks analyses, independent of cache/manual corrections | 77 / 77 |
| BPM within 0.1 of DJ.Studio | 73 / 77 |
| Same key as DJ.Studio's own stored detector result | 34 / 77 |
| Same key as the stored Mixed In Key value | 35 / 77 |
| DJ.Studio and stored Mixed In Key agree with each other | 46 / 77 |
| Mixworks key unknown | 2 / 77 |
| Mixworks downbeat unknown | 30 / 77 |
| Available downbeat agrees with DJ.Studio within 50 ms at the 95th percentile | 25 / 47 |
| Available downbeat exceeds that tolerance | 22 / 47 |
| Large phase disagreements, at least half a beat | 10 / 47 |
| Mixworks marks its pulse/grid as requiring review | 14 / 77 |

These are **agreement measurements, not accuracy percentages**. DJ.Studio is a useful independent comparator; its labels are not human-annotated musical truth. The 22 downbeat disagreements include 12 smaller timing/drift differences as well as the 10 large phase differences. None of the 30 unavailable downbeats are counted as an agreement.

## Tempo discrepancies

| Recording | Mixworks BPM | DJ.Studio fixed grid | Fresh native AI estimate |
|---|---:|---:|---:|
| Arc Mode (Mark Broom Remix) | 64.4540 | 129 | 128.988 |
| Chilli (Original Mix) | 65.2973 | 130 | 129.979 |
| Nude (Solomun Remix) | 83.3336 | 125 | 125.000 |
| Crazibiza & Effendi — Play Right | 124.8699 | 125 | 124.916 |

The first three are substantial rhythmic interpretation failures. Arc Mode's local windows mostly choose half tempo, with two near 129. Chilli has four windows near 130 and five near 65. Nude has one near 125 and eight near 83.33: its strongest repeated pattern spans three intended beats. Its incorrect global tempo is nevertheless marked stable by Mixworks. Play Right is a smaller disagreement, with several local windows also at approximately 83.33.

The producing boundary is `Source/Analysis.cpp`: each window retains a single winning autocorrelation period, the track selects a median, and the harmonic correction only tests the half-period pulse. It has no explicit competing 3:2 hypothesis or sustained multi-hypothesis metrical selection. An alias can therefore become the majority instead of being reconciled with the actual beat pulse. This also makes the `variableTempo` flag mix genuine tempo change with disagreement about rhythmic level or phase.

## Key discrepancies

Enharmonic spelling and the different numerical key encodings were normalized to Camelot before comparison. Against DJ.Studio there are **16 parallel major/minor disagreements, 7 relative major/minor disagreements, 18 different-tonic disagreements and 2 unknowns**. These are not just sharp/flat display differences.

Examples where both saved reference sources disagree with Mixworks:

| Recording | Mixworks | DJ.Studio and stored MIK |
|---|---|---|
| Bel Canto (Paul Anthonee Redition) | D major / 10B | D minor / 7A |
| Airsand — Emergency Exit | F major / 7B | F minor / 4A |
| Disclose Yours (Dub) | Gb major / 2B | F# minor / 11A |
| American Boy — Sandro Puddu | A minor / 8A | E minor / 9A |

Both reference sources agree against Mixworks on 23 tracks; six of these have Mixworks confidence at least 0.75. American Boy reaches approximately 0.98. That value is a profile-score heuristic, not a calibrated probability of correctness.

The current owner, `tonality()` in `Source/Analysis.cpp`, pools normalized spectral peaks and scores fixed major/minor profiles, including harmonic suppression and local votes. There is no independent trained model, calibrated confidence or adjudicated real-music evaluation behind the score. The audit establishes the disagreement and the scoring limitation, but does not establish that every reference label is correct or isolate the responsible instrument on every track. Manual musical adjudication remains necessary before treating reference keys as regression truth.

DJ.Studio's visible key-source setting is **Mixed in Key**. Its own stored keys and `mikKey` were therefore kept separate. The MIK values already existed in its database; no fresh licensed MIK analysis was executed or inferred from the presence of its logo.

## Downbeat and timing findings

The comparison uses DJ.Studio's saved selected grid, whose beat number is zero-based and repeats bars every four beats. All 77 selected grids match their saved automatic grid, so no selected-grid/manual-edit divergence was observed in this snapshot. There are 76 fixed grids and one AI grid.

Decoder alignment matters: on **55 recordings Mixworks PCM is 1,105 samples / 25.057 ms later** than DJ.Studio PCM; on the other 22 the measured offset is zero. Every offset was measured by cross-correlation of decoded audio, with correlation greater than 0.999998. The comparison corrects each offset before scoring bars throughout the track. It excludes the first/last five seconds and reports the 95th percentile of distance to the nearest Mixworks four-beat bar. This checks repeating bar phase, not which audible section ought to be labelled the beginning of the song.

Large disagreements include Biohazard, Sean Finn's Ain't Nobody, Deecross's Rock It, Anyma's Now Or Never and Pictures Of You, Bamboleo, Better Off Alone and Riders on the Storm. Some are approximately one beat; others are approximately two beats. These remain after decoder-offset correction. Other disagreements are tens of milliseconds and should not be confused with an entire beat or bar error.

The 30 missing downbeats follow from either an untrusted pulse grid or insufficient/conflicting accent and structural votes. The latter is deliberate uncertainty, but its frequency is unacceptable for the requested automatic workflow. Simply removing the thresholds would manufacture certainty without resolving musical ambiguity.

Historical MXM first-marker checks used in the earlier four-track test only establish agreement with those markers. They do not independently authenticate musical beat-one labels. The disagreement on Sean Finn and Better Off Alone makes that limitation material.

## How the comparison was verified

- Opened the installed DJ.Studio application, restored its existing autosave and inspected its settings and 77-track mix. No mix tracks, source audio, account, purchase or analysis settings were changed by this investigation.
- Snapshotted the relevant local library records and beat structures before measuring. No account tokens or unrelated profile data were copied.
- Used the installed DJ.Studio audio decoder to reproduce **all 77 stored WAV SHA-256 identifiers exactly**. Raw MP3-file hashes and the legacy MP3-frame parser are not the identity format used by this desktop import path.
- Ran the existing Mixworks real-track probe on every source, forcing fresh analysis and rendering production-decoded PCM. All fresh BPM, key and downbeat results reproduce the initial cached/read-only run exactly. This is not a stale-cache issue.
- The real-readiness probes produce **424 passing and 44 failing checks**: 14 pulse/grid readiness failures and 30 unavailable downbeats. The last recording is paired with itself to satisfy the existing two-track harness; the comparison deduplicates it to 77 sources. These are observed failures, not an all-green verification claim.
- Invoked the installed local native AI beat detector on 14 actual recordings, including the major discrepancies, The Bliss and Exrela. These are raw model results with one fold, not the full renderer's transient-retuned final grid. They corroborate the large tempo discrepancies; their downbeat choices sometimes disagree with the fixed grids and are not promoted to truth.
- The native AI probe returned beat arrays, stability and timing fields, **but no key field**. Static worker code expecting `result.key` did not establish working native AI key inference. The application's actual main analysis path retains the fixed detector's key result separately.

Installed shell package: DJ.Studio 4.2.15. The active downloaded frontend build is `1823b9d5ce41ab66810c7263a2f75f017a48bbd5`. The prior REA ASAR investigation remains relevant, but the installed shell and downloaded frontend must be identified separately. Observed file hashes, including the existing ASAR/unpacked integrity discrepancy, are retained; publisher integrity is not asserted.

## Reproduction and next implementation priorities

The [complete track table](../../.local/djstudio-benchmark/comparison.csv), [machine-readable comparison](../../.local/djstudio-benchmark/comparison.json), [provenance](../../.local/djstudio-benchmark/provenance.json), fresh probe reports and immutable database snapshot are retained under `.local/djstudio-benchmark/`. Original music is unchanged; full temporary probe WAVs were reduced to 30-second PCM heads for decoder verification. No proprietary detector/model is copied into Mixworks.

`tests/real-music-comparison.json` records digest-bound comparison labels and the current baseline; they are explicitly not human-annotated correctness targets. Recompute the report with:

```powershell
python scripts/compare-djstudio.py .local/djstudio-benchmark --output .local/djstudio-benchmark/comparison.json
```

The next detector change should retain and compare metrical tempo hypotheses before committing to a grid, distinguish rhythmic ambiguity from genuine variable tempo, and evaluate beat-one inference on an adjudicated real-music set. Key detection needs a stronger estimator and calibrated uncertainty, assessed on separate development and holdout tracks. Do not tune thresholds until this same set merely agrees with one commercial program.

The user's new product direction is recorded in [WISHLIST.md](../../WISHLIST.md): prioritize DJ.Studio's visible transition band and nearby lock for editing, retaining verified useful MixMeister workflows. Implementing that interface is separate from this completed measurement audit; reliable timing remains the prerequisite.

> Current-build clarification (19 October 2026): the automatic-control admission experiment described below is not compiled or called by 0.53. Its three source files are preserved under `.local/archive/auftakt-assessment/`. Experimental results remain historical; they do not establish shipped recovered-engine admission.

> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Recovered rhythm and automatic analysis — 18 October 2026

`Source/RhythmAssessment.*` adds an experimental compatibility boundary between the recovered AUFTAKT observations and Alloy's existing automatic analysis. `--rhythm-research ... --auftakt-model ... --compare-automatic` runs both on the same decoded PCM. It reads no cached analysis, comparison labels, sidecars or manual corrections. The default automatic analysis and recovered decoder remain separate from this opt-in assessment.

## Why conditioning is necessary

The recovered decoder agrees with the vendor demo on the two retained constructions, including the tempo-step double-time error. Its recovered neither/39 observation likelihood can reward weak intervening pulses. Changing those recovered probabilities or restricting its tempo bounds to fixture labels would conceal the distinction between reconstruction parity and musical accuracy.

Alloy therefore keeps the original events and full probability matrix and separately assesses a candidate using the independently measured automatic pulse. A stable control with confidence at least .5 is necessary. For each measured pulse, the assessment selects the maximum ordinary-plus-downbeat observation within `min(50 ms, .12 beat)`, independently of bar phase. At least 64 examined pulses and .8 supported-pulse coverage are required. These are conservative engineering gates, not calibrated accuracy probabilities. The output candidate is a `SourceBeatGrid` that the production source mapper and metronome can consume; it never supplies a variable-tempo event map.

Bar assessment pools downbeat probabilities at each of four pulse positions. Eight-bar sections must provide sustained evidence; a final partial section is examined if it contains two complete bars. A decisive section needs a .5 mean and .15 separation from its runner-up. At least two decisive sections and half the examined sections must agree with the global proposal. Any decisive contrary section withholds the bar. An existing acoustic conflict or disagreement with the automatic bar also withholds it. Missing independent bar evidence yields `candidate-only`. Only `corroborated` populates the candidate's downbeat; all other cases preserve `-1` and retain the separate proposal when available.

This assesses **4/4 compatibility**, not meter identification. A supported pulse is not proof of beat one. Observation validation now has one owner in `validateRhythmActivations`, reused by decoding and assessment. Malformed control bar values are errors, distinct from the explicit `-1` unknown sentinel.

## Real recordings

All six recordings were selected from the existing development split and verified by SHA-256. No protected holdouts were opened. Complete original recovered output remains in `beats`/`classProbabilities`; `automaticControl` and `recoveredAssessment` carry the new measurements.

| Recording | Pulse support | Bar result | Evidence requiring caution |
| --- | ---: | --- | --- |
| Arc Mode — Mark Broom remix | 602/643 | section conflict | Four decisive sections disagree with the global bar proposal |
| Chilli | 803/878 | corroborated | Agreement between methods, without independent musical annotation |
| Nude — Solomun remix | 980/1009 | control conflict | Existing `signal-conflict` remains authoritative; proposal .222431 s is unadjudicated |
| Ain't Nobody — Sean Finn / Funkera | 582/628 | section conflict | Two contrary sections; proposed 1.804392 s versus automatic .844418 s |
| Miracle — extended mix | 807/885 | corroborated | Agreement between methods, without independent musical annotation |
| Better Off Alone — radio edit | 521/585 | control conflict | Proposed 1.021202 s versus automatic .537328 s |

No half/double-time interval counts were observed in these six complete raw real-track paths relative to the automatic pulse. Off-pulse events were **22 / 47 / 6 / 0 / 28 / 20** at the assessment tolerance. These are cross-method differences, not annotated false-positive counts. The proposed stable pulse comes from the existing control; this is not a new BPM-accuracy result.

The retained historical profile markers give another comparison without entering either detector: automatic Sean Finn/Miracle/Better Off Alone anchors differ by approximately **+4.691 / +9.820 / +9.754 ms**. The recovered Sean Finn proposal differs by **+964.664 ms**, and Better Off Alone by **+493.628 ms**. Their rejected bar proposals therefore cannot silently replace those existing anchors. Historical source/profile association and decoder-origin equivalence remain unresolved; these are not new listening annotations.

The constructed tempo change produces **76 double-time intervals** relative to its control. The existing automatic analyzer incorrectly marks its 120.0248 BPM scalar fit stable, but only **14/81** control pulses have recovered support. The assessment returns `insufficient-support` and supplies no grid. This is an additional safety boundary, not a fix for the automatic analyzer's variable-tempo classification. Synthetic tests separately establish that a correctly supported independent tempo removes half/double-time errors from the candidate grid, including consumption by the real source mapper and metronome.

## Reproduce

Use a frozen bundle with adjacent Beat This model/runtime, the explicitly selected local recovered model, and a JSON array of absolute development-recording paths:

```powershell
python scripts/assess-recovered-tracks.py --exe <absolute-frozen-Mixworks.exe> --model <absolute-recovered-model.json> --paths <absolute-paths.json> --output <new-evidence-directory>
```

The runner retains invocations, source/model/executable/report hashes, fresh automatic control and assessment decisions. It refuses non-development sources, duplicate recordings and existing output directories. Exit **2** means complete execution requiring independent musical review; exit **1** means invalid or incomplete execution. It never declares musical admission. Individual native probes use:

```powershell
& <absolute-Mixworks.exe> --rhythm-research <absolute-audio> --auftakt-model <absolute-model.json> --compare-automatic --report <new-absolute-report.json>
```

## Integration still required

The recovered engine now has a native, tested comparison/admission boundary using the actual automatic pipeline and production source-grid type. Default automatic insertion, cache revision, persistence, arrangement and playback engine selection have not changed. Independent beat-one adjudication is still necessary for the conflicting recordings. The tempo-change fixture also needs a source-owned variable beat map rather than scalar projection. Do not use cross-model agreement as permission to overwrite those unknowns or promote the recovered engine globally.

## Verified checkpoint

Final evidence is `.local/real-rhythm-11/`. Frozen `bundle-03/Mixworks.exe` has SHA-256 `d1fb102f558ff25c6404c31254330c536e2161cfbfcc80223ba43ee3ba8a14f0`. All **976 native checks / 18 suites pass**, including 69 focused rhythm checks and real gain VST3/Vital. Eleven Python harness/scorer tests and three runner boundary checks pass. Both retained demo comparisons pass, and the Beat This control is unchanged. `real-03/manifest.json` retains all six complete comparisons; `final-real-stability.json` verifies unchanged raw events/probabilities. `deploy.ps1 -VerifyOnly` accepted the final regression receipt without deployment. Exact sources and recovery hashes are archived under `.local/source-baselines/20261018-real-rhythm-assessment-03/`.

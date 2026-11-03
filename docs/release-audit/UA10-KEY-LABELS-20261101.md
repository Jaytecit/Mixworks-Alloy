# Key-check labels — ua10

**Update:** [ua13](UA13-FOCUSED-VERIFICATION-20261101.md) successfully builds this
change and passes23 classification/sorting checks. Native table pixels inspected.
Pending build/runtime history below is superseded for that focused scope;
automatic musical accuracy and production activation remain separate.

Jay observed Review on almost every track and requested examination of whether
the labels distrust the analyser unnecessarily. The finding supports changing
the labels, without claiming all detected keys are musically correct.

## Evidence and decision

The initial read of `%LOCALAPPDATA%/Mixworks/Analysis-v1` found42 current saved
records: all had an accepted `analysis.key`,41 had `keyConfidence` below.55.
The median score was.14628, range.002805–.778956. The detector's score multiplies
the winning margin, passage consistency and profile agreement. It is explicitly
not a calibrated probability. The UI nevertheless used the.55 cutoff to present
the accepted detections as requiring Review.

The detector already distinguishes an accepted `key` from a tentative `keyGuess`.
Use that distinction for the user-facing status:

- **Detected:** accepted automatic key and finite evidence in its valid0–1 range.
- **Review:** tentative fallback or invalid evidence.
- **Unknown:** no usable key.
- **Manual:** the user's chosen key.

All accepted detections use one neutral text colour, rather than retaining
score-dependent warning colours. Hover help explains automatic origin and offers
Key by ear without telling the user that every result needs checking. Sorting
continues to put Unknown/Review ahead of Detected/Manual.

The second read found45 records,44 Review and1 Detected under the old rule; all45
would be Detected under the new rule. Records were arriving during inspection,
so these are timestamped snapshots, not a fixed library census. Per-record hashes,
key/guess/manual/score values and old/new classifications are retained in
`.local/ua10/key-records.json` and `label-impact.json`. Only JSON was read; no audio
decoded, musical ground truth inferred, cache modified or rescan started.

The source changes do not lower `keyReviewConfidence`, alter key values, promote
anything to Manual, change saved formats or weaken automatic harmonic/retry
policies. Existing records acquire their new display classification when the
changed application is eventually run; no bulk data rewrite is necessary.

## Verification and continuation

`Workstation.cpp` implements labels/help/colour. `AnalysisRefreshTests.cpp` extends
the bounded review-sorting branch with accepted-key score boundaries, fallback,
invalid and manual classifications and a low-score accepted native table row.
Independent source snapshots and diffs are under `.local/ua10/`.

**Not built or run:** the one guarded build preflight refused at1858MiB available,
below its2500MiB start floor. No compiler, native test or new screenshot was
produced. Source/whitespace checks do not establish runtime acceptance. Resume a
guarded single-worker build when resources allow, then the focused review-sorting
check and inspect its native pixels. The same tree also contains ua09's pending
destination repair; its focused checks and save/export journey still need to run
before activating the combined candidate. No broad gate or analysis run needed
for these label changes.

Launcher remains ua08 `9770639eb18c`. Production applications and ongoing analysis
were left running. Full readiness, musical key accuracy and ua09 verification
remain open. [skills.sh](https://skills.sh/) was consulted; repository-native
inspection and bounded verification remained the appropriate method.

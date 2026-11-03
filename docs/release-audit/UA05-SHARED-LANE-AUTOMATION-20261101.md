# Shared lane automation — ua05, 1 November 2026

Focused checkpoint complete. Implements the accepted shared lane curve plus
per-track transition fade choice in [the readiness plan](USER-READINESS-20261101.md).
This is a native preview with scoped verification, not whole-plan acceptance or
launcher activation.

Ordinary Volume, Bass/Mid/Treble, Pan and Filter selection and controls use the
lane. The selected track remains selected; its transition fades, stems and VST
parameters keep explicit per-track addresses. Only the selected parameter is
shown. The owner selector starts with lanes/master, followed by selected-track
fades and stems. This is the native editing preview, not a broad layout rewrite.

Default handles: start, eight project bars after start, eight bars before end,
end. Short clips merge inner handles at the beat midpoint. Coincident points
merge. Handles begin at the lane manual value without enabling automation or
creating fades. New clips in authored curves inherit sampled existing values;
bent/exponential/gain-space interpolation is preserved. Untouched handles follow
geometry/tempo. First edit freezes the curve's existing nodes in project time;
deleted nodes never regenerate for those clips. Gaps retain existing interpolation
and held endpoints. Removing a clip removes untouched generated handles only.
Per-track transition fades can fall/rise separately within the same shared lane.

Session 46 stores handle provenance, including edited/deleted-handle suppression.
Pre-roll rebases this metadata; ripple edits preserve the resulting authored
curve. Plugin ownership and source clocks are unchanged. Lane controls, MIDI,
automation recording and performance take events must agree on lane ownership.

Independent evidence/source checkpoints are under `.local/ua05/`. The previous
verified candidate remains `.local/ua04/candidate.exe`. Normal launcher and user
music/sessions/caches remain untouched. Single-worker guarded compilation and
bounded native functional checks only; full regression, performance, listening,
physical MIDI/ASIO and newcomer acceptance remain open.

## Candidate02 result and current repair

Candidate02 `cc0f2cb238bcf6418ae2a345fbdd0ecd353fd379d2ad0c91edde0a9f9b1132e8`:
47 lane checks passed, actual editor pixels inspected. Solo rerun stopped with
Windows stack overflow 0xC00000FD after its audio assertions. This is a regression,
not a complete Solo pass. The failed process/report and exact executable survive.
No further tests were run on that candidate. Two serial builds passed; the second
initially waited at its memory preflight, then ran after memory recovered.

Candidate03 moves handle provenance out of every AutomationCurve and into the
Project's lane-only map; Clip and AutomationCurve return to their prior size.
The visible/saved semantics remain the same. This reduces unnecessary nested
object/stack growth; the cause remains unconfirmed until the failed workflow is
rechecked. Added focused Touch ownership and lock protection checks. Build pending.

Historical audit triage now distinguishes15 records with a proved obsolete
version conjunct, superseded selected-track builtin-control expectations, or the
identified implicit lane-to-clip cutoff routing defect. Remaining records are not
assumed obsolete/passing. Protected downbeat-reference folders were absent at
both documented paths when checked read-only; no observations were recovered,
changed or fabricated. Six-source evaluation still requires independent review.

## Verified current candidate03

`978806ad21427d65667224a749301da0afe911fac11cb91f7ad4e601a6e8eec7`:

- Shared lane automation49/0, including Touch, lock protection, MIDI and matching
  performance display, native node drag, Undo/Redo, source-backed reopen,
  independent overlap/other-lane PCM and take capture/replay.
- Lane-only stem Solo40/0: the exact previously crashing focused workflow passes.
- Live transitions30/0: current lane scaffolding included in expected Undo state.
- Visual coverage24/0, including section-aware44-track saved schedules.

All native checks used verification.run_checks serially, below-normal, without
stress, with zero retention errors. Build03 passed the same memory guard. The
candidate02 failure remains in solo-01; passing rerun is solo-02. No threshold,
expected Solo behavior or assertion was weakened. The earlier failure involved
object/stack growth; lane-only heap-backed bookkeeping removes that growth.

Actual GPU worker: eight saved-show snapshots (beginning/middle/last blend/end),
plus resource-empty state after both shows end. Pixels inspected in
`.local/ua05/gpu-01/show-samples.png`; runtime and scene hashes retained. Section
schedule has133 scenes and81 eligible looks through1072s; image-aware schedule
uses12 compatible looks across44 scenes. The initial image sample is dark at0.25s.
Three-frame seeks with synthetic PCM establish functional rendering at later
positions, not continuous show, musical listening/pacing, fullscreen, startup
settling, production resolution or performance qualification. No benchmark ran.

Native preview screenshot: `.local/ua05/lane-02/shared-lane-automation.png`.
The owner selector says Lane1/all tracks and ordinary selected parameter is the
only lane curve. Lower Editing controls remain scrollable; a broad usability
rewrite and human observations are still pending. No physical gestures, MIDI or
ASIO test. Source/persistence/audio checks are not whole-rule acceptance.

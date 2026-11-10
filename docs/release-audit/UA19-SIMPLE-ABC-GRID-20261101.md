> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Simple A/B/C source-grid correction — 1 November 2026

Jay requested a distinct fixed marker, audio dragged beneath it, and sequential
Set A / Set B / Set C buttons as the first correction method. Other methods
remain available as secondary tools. The preceding conversation approved the
workflow, including explicit bar-count confirmation and independent C validation.

## Result

- The editor opens on Simple: A / B / C. A white/copper centre marker stays fixed
  across scrolling, zoom, playback and source-edge padding. Dragging navigates
  audio, pauses source audition and leaves existing marks and grid untouched.
  Shift gives tenth-speed navigation; arrows always move1ms, even after selecting
  10ms grid edits in Advanced. Clicking recentres the selected audio; Space plays.
- Set captures the actual audition clock and pauses it. Later marks require
  earlier marks; non-increasing marks are refused without changing the draft.
  Replacing A/B clears later marks. Mark/count/confirmation edits have Undo/Redo;
  source navigation does not create grid history. Escape cancels navigation.
- A sets beat one. Complete bars A to B give BPM = bars *240 / elapsed seconds.
  Suggested counts use the current tempo, require manual confirmation and can
  be corrected without moving the marks. C independently checks the A/B grid,
  displaying B/C BPM and C's offset. It does not fit a variable-tempo map.
- Simple Save requires three ordered marks, confirmed counts, unchanged measured
  grid facts, valid20–400 BPM and C within30ms. A wrong count can agree with other
  wrong counts; this numerical check is not proof of musical accuracy. Audible
  phrasing and bar counting remain necessary. Grid click starts off while finding
  downbeats; enable it for alignment checks before saving.
- More tools / Advanced exposes existing precise controls and guided Tempo,
  Beat one and Check drift pages. They share the draft. Advanced edits invalidate
  a Simple success if the measured grid no longer agrees. Save for this track
  persists existing BPM/anchor fields and leaves the mix clock/placement intact;
  Save + rebuild retains its whole-mix consequence confirmation.
- A/B/C marks are temporary editor measurement aids, not new persisted metadata.
  No source audio, detector, saved format, analysis revision, cache or library
  data is changed by this workflow update. No compatibility layer or reset.

## Verification and identities

Final executable SHA256:
`91f5a141564ecf334a95eb9471b2eb68e3d632eb70f22ec634f2fc948d2daa26`.
Version remains0.55.65. Source checkpoint, archive hash, executable, pinned model/
runtime identities, build logs and native reports survive independently of Git
under `.local/ua19/`; `final.json` binds `verified-source.zip` to the tested bundle.

Four serial single-worker below-normal builds succeeded. One initial native run
passed104/0; visual review led to source-only audition by default, clearer C
mismatch text and an overview capture. A targeted review then fixed inherited
10ms arrow precision and updated secondary-tool test entry points. A local test
update script initially expected four reopen call sites; its assertion found
three, wrote nothing and was corrected before the final build. No native failure
was hidden or retried. Intermediate source snapshots and build logs are retained.

Final `--grid-alignment-only` through `verification.run_checks`: **109 checks,
0 failures**, no infrastructure/retention errors, stress disabled. Report and
invocation hash are recorded in `grid-final-result.json`. The owned270,104-byte
audio fixture was removed by standard retention. No production cleanup.

Checks include fixed-marker source edges; actual source-clock capture; ordinary/
Shift/arrow navigation and Escape; ordering; incorrect suggested count corrected
to the independent128 BPM oracle; C120ms mismatch with Save refusal; confirmation
invalidation; Undo; replacing A; switching to/from Advanced; absent detected BPM;
actual source-record save/reopen; mix placement preservation; failed save; rebuild
confirmation; unchanged original audio bytes. Broader grid-correction and key-ear
test callers explicitly open secondary tools; these broader suites were not run.

Ten final980x780 native captures were inspected: empty Simple, aligned close-up,
three-mark overview, C mismatch, all three secondary guided pages, Advanced B and
overview, and waveform fine adjustment. Marker and waveform stay distinguishable;
counts, results and footer actions fit without scrolling. These are real editor
pixels and injected handler checks, not physical mouse/keyboard acceptance.

## Boundaries and continuation

No full regression, stress, performance, detector, physical device, musical
listening or novice acceptance. No source preparation/library scan or AppData
reset. Real constant-tempo tracks must still be evaluated by ear. The30ms check
does not establish detector or real-song accuracy.

Normal launcher remains ua18 `out/releases/0.55.65-3c1e3f3eb943/Mixworks.exe`.
No activation or production process control. Production was initially PID15232
on ua18 and was absent at the final inventory; no user session was closed by this
task. The local preview is built and focused-verified, awaiting user judgement.

Workflow documentation: [Source grid alignment](../SOURCE-GRID-ALIGNMENT.md).

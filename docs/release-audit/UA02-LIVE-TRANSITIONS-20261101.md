> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# ua02: live transition placement

1 November 2026. Continues UA08 with minimal functional verification while Jay
uses the computer. No speed/frame-rate/load/stress tests, scan, subagent or physical
mouse/keyboard/device interaction. No bridge changes or broad suite were needed.

## Repair

TransitionUI.cpp formerly reused `canInsertArrangement()` for Add Transition;
that predicate excludes playback. A local guard now allows linking already-prepared
overlapping tracks during normal playback while retaining busy, export, rehearsal,
pending placement/tempo/load and automation-write restrictions. Song insertion
retains its stopped-only guard. Model validation still rejects invalid/removed/
locked/conflicting targets. Button and callback share the new guard. Feedback names
the pair and actual start time. Recording/write-mode Add remains unavailable with
an explanation; wider Record/Perform acceptance is still open.

Focused transition selection/Previous/Next scroll to the pair without seeking during
playback. Stopped navigation still seeks to the transition start. Preset application
and DSP/persistence algorithms were not rewritten. Only TransitionUI.cpp,
TransitionNavigationTests.cpp and SelfTest.cpp changed. The new
`--live-transition-only` selector returns before broader timeline checks.

## Evidence and limits

Final candidate SHA256:
`b13a673a362d0460e51a8d365e258fd110e0269536a0154bf4b07119a86da5e0`.
**29 checks / zero failures, infrastructure or retention errors**, through
`verification.run_checks`, below-normal, stress disabled. Exact report:
`.local/ua02/focused-03-result.json`.

- Native Add None/Apply Crossfade and default Add Crossfade stopped/playing;
  chosen pair/frame, unrelated geometry, one-step history and no seek.
- Actual audio callback produces nonzero PCM and advances the exact sample clock;
  focused selection/Next preserve playing position; stopped navigation seeks.
- Independent per-lane PCM confirms outgoing/incoming fades and unchanged unrelated
  lane audio. Undo/Redo restores the full project.
- Source-backed save/reopen reproduces transition PCM. The ordinary offline export
  renderer admits and renders it; this is not a complete WAV-file export test.
- Busy/export/tempo/preparation/write guards and removed late targets reject
  without changing project/history/playback.

Actual native 1250x800 pixels were inspected directly in
`.local/ua02/focused-03/live-transition-playing.png`: linked pair and enabled Add
are visible. Some preset controls require scrolling; retain that under UA05.
The fixture stops the UI timer, so the toolbar time label is stale; callback
assertions establish clock continuity, not that label. No physical gestures,
listening, hardware or wider-mode acceptance is inferred from native callbacks.

Three serial incremental builds/runs were necessary. Original failures remain:
run01 **7/1** omitted the fixture selection's primary ID, so real refresh cleared
its pair; run02 **29/3** inspected obsolete per-clip storage instead of the current
`Project::effectSettings` owner. Its independent PCM checks already passed. The
corrected assertions retain the intended selection/node requirements; production
behaviour was not weakened. Run03 is the only final passing result. Memory guards
did not stop a build; no build/test overlap or shared-header rebuild occurred.

## Preservation and continuation

Independent exact-byte source archives/manifests, diffs, candidate executable,
runtime hashes, captures and all attempts: `.local/ua02/`. Ordinary retention
removed owned fixture WAV/session files after process exit. Original music,
production data/apps and normal launcher untouched. Candidate **not activated or
release-qualified**. No owned jobs remain.

UA08 is **repaired awaiting verification** of its full user workflow; no whole-rule
PASS or full regression. Next bounded work: lane-only stem Solo (UA07), with tagged
independent lanes. Keep same-lane transition/automation semantics and panel scrolling
in the editing milestone; do not resume performance testing.

Native component/bridge screenshots can be opened directly with the image viewer,
without a browser. The HTML draft's local-file browser navigation remains rejected;
no server/alternate-browser workaround was attempted. This capture does not verify
that separate HTML draft.

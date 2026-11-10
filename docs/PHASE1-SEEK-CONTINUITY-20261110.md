# Phase 1 seek and scene preparation

This checkpoint addresses destination setup during seeks and native Studio scene
boundaries. Current verified candidate, activation and limitations are recorded
in [HANDOVER](../HANDOVER.md); phase ordering stays in
[FINISHING-PLAN](../FINISHING-PLAN.md).

## Cause and implementation

The 0.55.86 isolated baseline's cold seek to 20 seconds added one buffering
callback. Its seek reset the live stretch renderer and left destination window
creation to the callback. Native Studio's `prepare` hook did nothing, and seek
reset destroyed its current compiled assets. A bounded actual-GL check reproduced
four failures: no precompile, compilation at a boundary, recompilation on seek,
and loss of the prepared successor across a gap.

Live seeks now snapshot their destination and use one low-priority worker to
prepare original-source blocks and stretch windows for overlapping clips and
stems. Old playback continues during preparation. The message thread revalidates
request, clock, source and placement identities, then swaps the prepared renderer.
Displaced stretch streams, programme snapshots and audition engines retire after
unlocking. Newer destinations supersede old requests; transport changes cancel
pending preparation. Failures preserve the current cursor and playing state.
Offline/native-test seeks retain a synchronous prepare/commit path.

Studio sends an eight-second lookahead for the next actor on each layer. The
renderer retains at most one prepared actor per layer and loads at most one
resource after each delivered frame, or during paused idle time. Background
textures are uploaded during preparation. A speculative failure leaves the
current layer available and is not repeatedly retried until its identity changes.
Activation reports a real load failure if that actor is actually requested.
Gaps keep their prepared successor but draw no scene. Seeks reset feedback and
analysis while retaining compiled assets. Native Studio holds its last accepted
picture until a current-generation response arrives; frame facts remain attached
to the original picture. Its embedded preview now records distinct actual paints.

## Focused verification entry points

Use `verification.run_checks`, serially and with stress disabled:

- `--seek-continuity-only`: bounded original-source and independent resident-PCM
  comparisons for raw, stretched/pitched, reverse, loop/slip, stems and three
  overlapping sources. Checks old output before commit, first destination output,
  exact clock, preparation windows, async supersession/cancellation, stale/foreign
  publication and unavailable sources. Returns before the broad streaming suite.
- `--live-transition-only`: existing editor/transport/history callback checks.
- `--studio-workspace-only`: integrated native Studio workflow and renderer.
- `scripts/verify_studio_continuity.py <bundle-directory> <report-file>`:
  actual 160x90 GL output, preloading, resource reuse, gaps, failures and all four
  layers. `--source` selects the repository adapter with bundled engine modules.
- Bridge changes also use client/harness tests and `scripts/verify-bridge.py`.

Evidence is retained in `.local/phase1-seek-scenes-1110`. The initial expanded GL
fixture used an invalid `studio/vector-tunnel` key; the bundled key is
`studio/tunnel`. That incomplete run is retained separately from the corrected
24-check source pass. Three build invocations were stopped before linking to batch
the async owner and final edits; no .87 executable was produced by those stops.

.87 passed seek90, live72, Studio26 and GL24 checks, actual isolated cold-seek,
moving-scene and mouse-ruler observations, and the corrected 44-check live bridge
journey. It was not activated. Jay deferred another full build. Source .88's later
menu and native bridge-test edits passed MSVC syntax-only compilation of their four
changed C++ files, using the existing .87 configuration. No objects or executable
were generated; native menu/bridge tests remain unrun on those source changes.

## Beat grids menu feedback

Jay clarified that Beat grids itself was slow with one or no selected tracks.
The production catalogue was counted read-only (5,527 entries); no snapshots or
music were changed. An owned fixture at that count used unused fake source paths,
not 5,527 audio files. The actual menu remained absent after 3,250 ms; the small
control had already displayed it at 388 ms. These are click-plus-observation
timings, not precise paint timestamps. The menu had queried `analysisTargets(1)`
merely to enable its review action, constructing and quadratically deduplicating
all qualifying tracks. Source .88 instead stops at the first qualifying track
and scans selected rows only for the needed booleans. Actual analysis commands
are unchanged. `--beat-menu-only` checks choices and busy/playback/export guards
with metadata only; execute it and inspect actual menu response at the next build.

The .87 native bridge failures were reproduced on .86: the old fixture searched
only direct children for the now-nested scene lane. Its source lookup is repaired.
The live bridge fixture also lacked aligned OUT/IN markers and incorrectly expected
an owned-preset replacement to request authored-curve confirmation. Correcting
that fixture produced a 44-check .87 pass. Separate failed authored/collision
fixture attempts remain retained; they did not change production behaviour.

## Limits

This is a bounded continuity checkpoint. Short snapshot/publication locks remain.
Hosted plugin resets still use the existing serialized ownership and need separate
qualification. ProjectM reloads feedback on seeks, and speculative GL preparation
still runs on the owned renderer context after frame delivery; unmeasured shaders
or slow sources can cause longer holds. Local generated-fixture output establishes
neither physical driver/MIDI/ASIO behaviour nor real-track listening acceptance.
Sustained audiovisual delivery and the wider W04/W14 journeys keep phase 1 open.
Jay reports a long delay opening Beat grids (10 November), and clarified that
Beat grids itself is the problem rather than View. Reproduce that menu opening;
menu construction or prior focused passes alone cannot close that observation.

## Activated 0.55.88 focused checkpoint

The pending menu/native-bridge changes now have linked runtime evidence. Menu11,
seek90, live72, Studio26, composite94, shaped105 and bridge39 passed on .88, plus
GL24 and live bridge45. `audioCallbacks` reads the existing engine counters without
taking the render lock; isolated Save/Export defaults to the owned sessions folder.
Actual menu opening at 5,527 metadata entries took 554/470 ms including click and
capture with none/one selected; native menu construction was 0.040 ms. In 11.95
seconds, actual playback delivered 322 paints and 1,193 callbacks with no added
source waits, missing samples, lock misses or overruns across view/control/Undo,
seek and a scene boundary. A separate actual ruler seek also kept playback active.
Normal UI export produced a 40-second stereo 24-bit WAV; sampled PCM and
source-backed reopen matched the separate renderer. See [Handover](../HANDOVER.md)
for identities, retained evidence and the separately failing native export fixture.
This is bounded local acceptance; physical listening, sustained delivery and wider
refusal/recovery journeys remain open.

# Mixworks current handover - 10 November 2026

## Current outcome

**10 November native export-test repair and bar-jump continuity verified in 0.55.90.**
The complete local bundle is `out/releases/0.55.90-2b8fb2bac1ea`; `out/Launch.cmd`
selects it after the retained activation checks. Previous .88 launcher rollback:
`out/Launch.cmd.bak.version-0.55.90-20261110`. Studio remains 0.1.18.
Executable SHA-256: `2b8fb2bac1ea9f93b717d285a3c63c92bc1b4794236cf702f28491248e813cac`.
The visible title and Windows FileVersion/ProductVersion all match 0.55.90.

The native export-test stack failure is fixed. Four large Workstation owners and
nine Engine owners in its shared dispatcher now live on the heap, retaining their
original lexical lifetimes. MSVC's frame fell from 876,416 to **121,424 bytes**;
the executable's stack reserve stays **1 MiB**. The previously crashing complete
save/export journey now passes **57 checks**, including its chooser, source-backed
save/reopen and rendered file/PCM results. Destination safety111 and stable export-
target/admission75 also pass. The earlier failure was not an export DSP defect.

Mix-bar jumps now release their caller's audio render lock after computing the
destination, before seek preparation, viewport changes and UI refresh. The live
refresh-boundary probe produces matching nonzero PCM (maximum error 0) and an
advancing exact callback clock, with no new lock misses. Its deliberately held-
lock control still produces silence and a held clock. Direct busy/export bar
commands explain the recovery action and preserve the session; export disables
the corresponding menu groups. Structural track moves retain their existing
transaction lock scope and remain separate phase-1 work.

Final candidate passed **441 focused native checks**: export57, destination111,
export-target75, bars34, live74 and seek90. Runs were serial and below normal
priority through `verification.run_checks`, stress off, with clean retention.
The actual exact-bundle muted-device journey delivered **321 distinct Studio
paints and 1,221 callbacks over 12.19 seconds**, including forward/backward four-
bar jumps and scene boundaries. It added zero lock misses, source waits/missing
samples, callback overruns, oversized blocks or export-silence callbacks.
Save/source-backed reopen reproduced the same 1,024-frame PCM probe. Title and
moving pixels were inspected; the owned app was saved and closed gracefully.

The separate .89 diagnostic bundle first proved export57/destination111/target75,
live74 and seek90, but its newly isolated bar suite exposed two older fixture
errors (32/2). One expected an unenabled pair movement group; the other injected
bad plugin automation into shadowed raw Clip.controls rather than canonical
Project.effectSettings. Final .90 explicitly configures both fixtures, retains
all refusal/atomicity assertions and adds two setup assertions. No production
movement-group or effect-cloning implementation was changed to satisfy them.
The .89 source, bundle and failed reports remain retained and were not activated.

Evidence: `.local/phase1-export-final-1110` (final source/bundle manifests,
reports, screenshots, stack-frame evidence and activation); diagnostic failures:
`.local/phase1-export-repair-1110`; original .88 failure evidence remains intact.
Owned `.local/pf1jump90` was closed normally and its one exact-hash generated WAV
removed. Production apps, original music, sessions, analysis/corrections/caches
and protected references were untouched. No detector reset or public release.
This is focused local acceptance, not a full regression/stress gate, physical
MIDI/ASIO, real-track listening or sustained qualification. Phase 1 remains open
for structural-edit lock scopes and broader W01/W04/acute W14 acceptance; keep
phase 2 deferred. Catalogue-scale reopening remains scheduled under phase 3.

## Prior .88 focused checkpoint (superseded above)

**10 November phase 1 focused delivery verified in 0.55.88.**
The complete bundle is `out/releases/0.55.88-8709abe02fcd`; `out/Launch.cmd`
selects it after the retained local activation checks. Its previous .86 launcher
is preserved as `out/Launch.cmd.bak.version-0.55.88-20261110`. Studio stays 0.1.18.
Executable SHA-256: `8709abe02fcdc14169187cfec9311536fd363aa55bb24256aaafd3b6037bb48b`.
Visible title and Windows FileVersion/ProductVersion all match 0.55.88.

This delivers the pending prepared-seek/Studio-lookahead and Beat grids changes,
alongside the analytical fades from .86. The actual Beat grids menu appeared in
an owned 5,527-entry metadata catalogue with no selection and one selected track:
click-plus-capture observations were 554/470 ms, versus the prior menu still
absent at 3,250 ms. These include automation/capture overhead, not precise paint
latency. Native construction measured 0.040 ms; no analysis was run.

Passed **437 focused native checks**: menu11, seek90, live72, Studio26,
composite94, shaped105 and bridge39. Supplemental actual GL24 and live bridge45
also passed. Runs were serial, below normal priority, stress off, with clean
native retention. Client/harness tests passed 20 each (one client skip). These
are focused results, not a complete regression receipt.

On the exact staged bundle, actual muted device playback delivered 322 distinct
Studio paints across view changes, a control/Undo, a seek and scene boundary over
11.95 seconds. Its 1,193 callbacks added zero over-budget callbacks, lock misses,
source waits or missing samples. An actual ruler click landed at 26.276 seconds
while playback continued, also without added lock misses/waits. Read-only bridge
`audioCallbacks` exposes the existing counters; this does not measure driver
underruns or physical output. Inspection confirmed moving pictures and .88's title.

The **normal UI WAV export is now demonstrated**: actual chooser/Save/Last segment
produced 40 seconds of stereo 24-bit / 48 kHz audio (1,920,000 frames). Its first
1,024 stereo frames matched the separate export-renderer probe within 1.188e-7.
The saved source-backed mix reopened with identical probed PCM. Bridge Save/Export
dialogs now default to the owned sessions folder, rather than the desktop folder.
One desktop input lost focus; its no-file outcome was inspected before a fresh
attempt. The successful attempt selected the actual accessible Last segment button.

**Remaining limitations:** the separate `--save-export-workflow-only` native
fixture crashes with stack exhaustion (0xc00000fd) after the same first three
checks on both .88 and the unchanged .86 baseline. Its compiled test frame reserves
876,416 bytes against a 1 MiB executable stack, making fixture stack pressure the
leading explanation; no debugger backtrace was captured. Both failed runs survive;
no native export42 pass is claimed. Repair its large stack-local test owners before
the next native build. Actual normal UI/export PCM evidence above is independent.
After the large-library fixture, reopening the small mix delayed a read-only status
reply beyond 15 seconds; the eventual successful response/state were inspected,
and Open was not repeated. The Library intentionally retained its 5,527 entries.
Catalogue-scale open/recovery work remains under W14/phase 3.

Evidence/source identities, build log, failed attempts, payload manifest, scoped
activation receipt and screenshots are in `.local/phase1-final-1110`. Owned
`.local/pf1final88` was saved and closed gracefully; its two exact-hash generated
source/export WAVs were removed after comparison. Production apps, music, sessions,
analysis/corrections/caches and protected references were untouched. No detector
reset, public binary release, full gate, stress, listening or physical MIDI/ASIO.
Phase 1's local implementation is substantially verified, but intermittent real
listening, sustained delivery and wider W04/W14 acceptance remain open. Do not
advance to phase 2 on the strength of these bounded observations alone.

## Prior 10 November source checkpoint (superseded by .88 above)

**10 November phase 1 source checkpoint; next full build deferred by Jay.**
The launcher still selects **0.55.86** below. Source `CMakeLists.txt` now says
**0.55.88**, but no .88 executable has been built or activated. Jay said another
full build is unnecessary at this stage if focused checks are good. Keep updates
in plain English: explain music jumps, loading the next visual and menu response
before technical evidence; Jay explicitly asked what the technical updates meant.

The separate .87 diagnostic bundle is `out/releases/0.55.87-647e37f837db`, SHA-256
`647e37f837dbe5633cc9a3e64366a3c858ae696940d795fa8a29c2432c3a3cc0`.
Its title and Windows metadata match .87. It passed **90 seek, 72 live transition,
26 Studio and 24 actual-GL checks**, serially below normal priority through retained
checks, stress off. Navigation now prepares destinations on one background worker
while old audio continues; it revalidates before swapping and retires resources
after unlocking. Cursor-dependent structural edits retain synchronous transactions
and their older lock scope. Studio reuses compiled assets across seeks, prepares
one upcoming resource per frame, and keeps the last picture until the new one arrives.

An isolated .86 cold jump added one buffering callback; the .87 staged copy's
20/4/16/8-second jumps added none. Actual moving Studio pictures continued through
a seek and scene boundary. An actual mouse ruler click landed at 25.5704 seconds
with playback running. Owned `.local/skphase1g` was saved and closed gracefully;
its hash-recorded generated audio was removed. Original music, running production
apps, retained analysis/corrections/caches and protected references were untouched.
No detector reset, listening/physical MIDI/ASIO or sustained qualification.

Jay reported a slow **Beat grids menu**, clarified View was unrelated, and said
one or no Library tracks were selected. A read-only catalogue count was 5,527.
An owned metadata-only fixture at that count reproduced a menu still absent at
3,250 ms; the one-track control had appeared at 388 ms. The menu unnecessarily
built a complete, quadratically deduplicated review-target list. Source .88 uses
existence checks instead. Its new `--beat-menu-only` 11-check branch is **unrun**;
actual menu acceptance awaits the next linked build. Four changed C++ files passed
MSVC syntax-only checks using the existing .87 configuration; no objects or new
executable were produced. This is not runtime verification of the menu fix.

Supplemental .87 bridge tests found obsolete direct-child scene lookup (36/2,
reproduced identically on .86) and an outdated live fixture. Source tests now walk
the real hierarchy; the live fixture has aligned OUT/IN and checks replacement of
an owned preset without an authored-curve warning. The corrected .87 live bridge
journey passed **44 checks**, saved and closed. Repaired native bridge tests await
the next build. Client/harness tests passed 20 each, with one client test skipped.
Failed attempts and owned-child termination reports remain retained. Four historic
invalid encoding bytes in this file were repaired; document checks then passed.

Evidence/source identities: `.local/phase1-seek-scenes-1110`; details in
[seek/menu checkpoint](docs/PHASE1-SEEK-CONTINUITY-20261110.md). Three compile
invocations were stopped before linking while batching .87 changes; the final .87
build passed. **No additional full build started after Jay's latest instruction.**
At the next agreed build checkpoint, build current source once, run focused menu/
seek/live/Studio/bridge checks, and inspect menu response and moving pictures before local
activation. Phase 1, normal UI WAV export, sustained delivery and wider W04/W14
acceptance remain open. Do not confuse source .88, diagnostic .87 and launcher .86.

**10 November composite-fade/editor-continuity milestone verified in 0.55.86.**
The complete local bundle is `out/releases/0.55.86-978ec641a86a`; activation
selects it through `out/Launch.cmd`, preserving the previous 0.55.84 launcher in
`out/Launch.cmd.bak.version-0.55.86-20261110`. Studio remains 0.1.18.
Executable SHA-256: `978ec641a86a895ce8e2279d61830e4ea491b1f304f01f70bb1fc6b428b9e859`.
The title-area version and Windows FileVersion/ProductVersion are 0.55.86.

The adaptive composite fitter is removed. Mixed authored/generated and
overlapping gain/EQ fades now use bounded analytical product/minimum expressions.
Only authored nodes and real fade/clip boundaries remain; a composed curve can
have several such boundaries, but has no sampled fitting nodes. Equal-endpoint
overlap crests and discontinuous cuts remain explicit. Session53 and portable
preset7 preserve these expressions, including slices, insertion and tempo maps;
derived endpoint caches are recomputed on read. Manual/overlapping cutoff
combinations that cannot share one filter retain an actionable, transactional
refusal. They are not silently converted to a different processor topology.

Automation bend, endpoint drag, amount, add/delete and precise-node edits prepare,
validate and notify the UI outside the callback lock; replaced projects retire
outside it. A short snapshot/publication lock remains. Six bounded callback
observations at actual editor notification boundaries produced matching nonzero
PCM, advancing clocks and no new lock misses. The held-lock negative control
produced silence and a held clock, establishing the contention failure mechanism.
This removes the long materialisation/notification lock scope; it does not prove
that every snapshot or unrelated editing/preparation path is free of contention.

The final candidate passed **456 focused native checks: composite94, direct105,
live72 and Auto Arrange185**, zero failures/retention errors, serially through
`verification.run_checks`, below normal priority, stress disabled. Coverage
includes fixed/ramped tempo, authored curves, overlapping clip minima and band
products, exact cuts, sparse knots, persistence, portable presets and source-backed
reopen/export-rendered PCM. Volume PCM is compared with the original layered
DSP; EQ composition checks establish control values, not arbitrary serial-filter
or plugin-topology equivalence.

The exact staged bundle ran in owned `.local/pf205586` (PID40540), with protocol,
root, version and capabilities checked. Actual mouse editing retained four real
nodes. The actual UI Undo and Redo restored the exact saved curve states while
muted playback remained active and its clock advanced. View/zoom changes also
retained playback. Final pixels were inspected; the isolated session was saved
and its app closed gracefully. Normal UI WAV export remains unverified: its
chooser opened, but desktop keyboard automation did not change the filename;
the attempt was cancelled without writing a file. Native real-exporter/reopen PCM
checks passed. No physical MIDI/ASIO, listening, sustained visual continuity or
complete regression/stress gate was run. W01/W04/W14 and phase 1 remain open;
next investigate remaining seek/scene-preparation and broader interaction paths
on a bounded real audio/visual workload, with actual output evidence.

Evidence: `.local/phase1-composites-1110` holds source archives for .85/.86,
build logs/identities, payload manifest, scoped activation receipt, native reports,
UI saved-state comparisons and screenshots. Initial .85 Auto Arrange185/1 and
the identical .84 baseline failure are retained: the obsolete DJ-set assertion
demanded "Bass handover" instead of current aligned-cut/filter treatment. Only
that assertion was corrected for .86, with active-volume and genre constraints
retained. An unsupported bridge `undo(redo=true)` attempt was replaced with the
successful actual UI history checks; the bridge exposes Undo only. Generated
native audio/temporary arrangements were removed by retention; two extra owned
source-audio copies were hash-recorded and removed after close. Production apps,
original music, retained analysis/corrections and protected references were
untouched. No detector revision/reset or public release.

**10 November phase 1 direct-fade milestone verified and activated in 0.55.84.**
`out/Launch.cmd` selects `0.55.84-a5d13b64035b`; Studio remains 0.1.18.
Fresh, separate generated gain, constant-power, EQ and filter fades retain exactly
two editable endpoints and evaluate their stored formula directly. There are no
sampled interior points in these treatments. Native shape data includes source/
tempo mapping, insertion/slicing and held hard-cut spans, portable placement and
persistence (session52, portable preset6). Auto Arrange's straight-line pruning
cannot flatten a native nonlinear shape. Existing authored curves remain protected.

The exact candidate passed **105 direct-fade checks and 72 live-transition
checks**, serially through `verification.run_checks`, with no failures or retention
errors and stress disabled. Coverage includes independent gain/frequency values,
constant and ramped tempo maps, preset placement, insertion/slicing, hard-cut
save, Undo/Redo, actual source-backed save/reopen and export-rendered PCM. The live
suite covers zoom/scroll and node-editor callback PCM/sample clocks, contention's
real silent-output control, manual-curve refusals and live take Undo/replay.

The complete staged bundle then ran in the fresh owned `.local/pf105584` bridge.
Protocol/root/PID/capabilities and advancing muted playback through view changes
were asserted. Inspected the actual title and two-endpoint curve. An actual mouse
drag changed the incoming endpoint from 8.0 to 8.5 source seconds; its saved curve
still had two nodes. UI Undo restored the exact original saved curves. The owned
session was saved and gracefully closed. Visible title and both Windows version
fields are 0.55.84. Temporary source copies for this UI measurement were removed;
production apps, music, sessions, analysis, corrections, caches and references
were left untouched.

Evidence, failed attempts, exact bundle identities, source checkpoint and
activation receipt are under `.local/phase1-fades-1110`. The launcher rollback is
`out/Launch.cmd.bak.version-0.55.84-20261110`. 0.55.82 was stopped before linking;
0.55.83's fixture writer needed a compile repair, then its invalid fixed-tempo
map/zero-width transition fixture caused failed checks and an unchecked test-UI
crash. Its executable, partial report and clean retention receipt are retained.
The 0.55.84 fixture is corrected; a source review also repaired zero-width shape
slices after a hard-cut edit before the final executable was produced.

**Phase 1 and W33 remain open.** Mixed authored/procedural or overlapping fade
composition still uses the prior fitter and needs its own coherent treatment.
Actual intermittent audio/visual continuity, scene preparation and the wider
W04/W14 refusals/recovery journeys remain unqualified. This is focused local
preview evidence, not a complete regression/stress gate, real-track listening or
physical MIDI/ASIO qualification. Continue phase 1 before phase 2.

**10 November additional requests recorded at the 0.55.81 checkpoint.**
Jay requested two-node shaped auto fades, a preview detach/attach toggle with
highest/native-resolution fullscreen on the second monitor, and random live
scene/MilkDrop swaps disabled while timeline elements drive the visuals. Audio
and visual continuity is a cross-phase requirement. `FINISHING-PLAN.md` now owns
the ordered nine-phase queue, including every prior outstanding W item and new
W33-W35. The attached FLkey panel/tutorial screenshots are references for phase 5.
The 0.55.81 bundle was completed before phase 1 started above. W34/W35 remain
phase-2 work; W33 has only the scoped direct-fade milestone described above.

**10 November Studio access / varied arrangement activated in 0.55.81.**
`out/Launch.cmd` targets `0.55.81-80783525860d`; independent Studio launchers
select 0.1.18. Open Visual Studio from the visual timeline or View menu.
Mixworks keeps clip placement, timeline automation editing and the right-hand
preview; duplicate visual parameter/output controls were removed. Jay's later
preview request explicitly retains that preview, now using the full panel
height with aspect ratio preserved. Studio edits the selected native clip,
including 128-bar cycles, playhead points and native Undo/Redo, and owns the
separate clean output window.

The arranger shuffles all 207 bundled scenes across families, dealing each
scene once before repeating. Each new run gets a fresh order. MilkDrop
imports are shuffled too. Backgrounds hold for 192 beats, MilkDrop changes
on offset 64-beat phrases, GLSL follows music/section changes with a
128-beat maximum hold when markers are sparse, and sprites enter in
32-beat accents separated by gaps. The four lanes share show endpoints
without sharing all interior boundaries. Original / MilkDrop is a real
0-1 blend, preserving nested interaction between its endpoints. Connected
mode uses the current MilkDrop lane; independent mode uses Prism Current.
Sprite Bounce is one bounce per audible beat; other repeating motions
also use musical periods. Native playback supplies the absolute grid
beat, including seeks and tempo changes, so animation follows the heard
track after any intentional mix-tempo adjustment.

Final 0.55.81 passed 26 focused native checks and 11 bounded Studio checks,
including four beat-clock unit tests, both blend endpoints, mailbox edits,
cycles/points, clean output and simulated transient file-read denial. The
exact independent 0.1.18 bundle passed five actual Studio UI/renderer checks
on its Python 3.10 launcher. Inspected final 0.55.81 title/preview pixels
and the linked 0.1.18 surface from a fresh isolated bridge. Actual UI drag
in the earlier 0.55.79 journey saved blend .8100558659; Studio Undo restored
.35. Closing the final Studio left the owned native app available; the
owned native session was then saved and gracefully closed.

Earlier attempts are retained: 0.55.79 passed 27 native checks but its
independent blend had a missing projectM argument; an early pixel oracle
incorrectly compared different simulation frames. 0.55.80 passed 26
native checks but its new transient-message check found an unapplied
literal text replacement. These are corrected in the final payload.
Evidence, exact commands, source checkpoints and manifests are in
`.local/studio-access-1110`. The previous Mixworks launcher is preserved as
`out/Launch.cmd.bak.version-0.55.81-20261110`; previous independent launchers
are in that evidence directory. Future Python-only Studio changes should
run the bounded source/UI checks before the native build: version defines
currently cause a broad native recompile.

The development bridge freezes only the executable/model/ONNX runtime.
Visual checks supplemented fresh owned bundles with the matching immutable
visual runtime/Python payload and independent hash manifests. An early
queued open timed out during setup; its eventual successful response and
actual state were inspected without retrying the mutation. `.local/sv1110`
and `.local/sv1181` were saved and closed. Production instances, music,
analysis, corrections, references and caches were left untouched.

This is focused local-preview evidence, not a complete regression receipt,
real-track listening or physical MIDI/ASIO qualification. Connected Studio
currently offers mouse/keyboard clip controls; independent Studio retains
its FLkey surface. The linked preview is 640x360; the new highest/native-
resolution detached fullscreen requirement is W34 in phase 2, not part of
this delivery. Two-node auto fades (W33) and random live swaps (W35) are also
queued, not implemented. `FINISHING-PLAN.md` is the single ordered work queue.

**6 November Alloy Visual Studio integrated in 0.55.78.** The dedicated
Visual Studio tab sits beside Controller and Transitions and can detach
into a separate window. The music lanes no longer contain the visuals lane.
Studio has an independent bar ruler, horizontal scroll and zoom, four clip
lanes (backgrounds, MilkDrop, GLSL and sprites), a preview and a clean output
window. Use the lane's Add button or double-click an empty part of its row.
Move or trim a block directly. The bundled Prism Current preset drives Fault
Ledger, Tidal Culture and Vector Tunnel inside their shaders. Other GLSL
scenes can blend the preset as a live layer. Imported media lanes accept
still images. Auto arrange visuals uses music handovers and suitably spaced
section edges. Audio Auto Arrange can build the visual lanes in the same pass.

A selected clip exposes parameter sliders, point automation and a 128 bars
checkbox per continuous or discrete parameter. The sweep starts at minimum
at clip entry, reaches maximum at 512 mix beats and returns to minimum at
1,024. Turning the checkbox off holds the current value or resumes stored
keyframes. Use + Point at the playhead, Ctrl-click the selected curve to
edit it and Shift-click to remove an interior point. Session format is 51.

Automatic Balanced, Bold and DJ set joins prefer a shared section boundary,
with a nearby bar or beat fallback. Gain changes through the existing 4 ms
anti-click ramp. Bass, mid, treble, level matching and conditional filters
shape the join. Explicit Smooth and Constant power styles remain available.

The 16 focused native checks passed on executable 90487dc0179d. Checked
layer rendering, MilkDrop pixel contribution, automation persistence, aligned
audio gain handover and panel docking. Inspected the docked and detached UI
captures. The first link was held while the synthetic fixture gained a valid
downbeat. The first verification wrapper used an older Python and exited
before launching a native test. Evidence and source checkpoint are in
`.local/studio-in-alloy-1106`. Real-track listening and the full gate are
outside this local preview. The prior launcher is retained as
`out/Launch.cmd.bak.version-0.55.78-20261106`.

**6 November nested MilkDrop comparison rendered.** Jay found the 200-scene
library too plain and asked to compare Fault Ledger, Tidal Culture and
Vector Tunnel with the same preset and track. The 15-second
[comparison](.local/studio-milkdrop-1106/MilkDrop-Nested-Comparison.mp4)
shows the hybrids above the originals. One projectM instance renders
**Prism Current** once per frame for all three hybrid scenes. An original
128 BPM electronic loop drives both rows. Fault Ledger channels the preset
through fissures, Tidal Culture feeds the preset's brightness into its
reagents, and Vector Tunnel maps it into the tunnel walls.

Inspected the rendered frames, checked the stored reagent difference and
decoded all 360 video frames with the 44.1 kHz audio. The clip, frame
captures, metrics and source checkpoint are in `.local/studio-milkdrop-1106`.
The comparison uses `scripts/render-studio-milkdrop-preview.py` and the
Studio 0.1.15 projectM runtime. Use Jay's judgement on these three before
expanding the library or adding an interactive control.

**6 November Studio window drag update activated.** The
[scene library launcher](visual-studio/Launch-Scene-Library.cmd) now opens
0.1.15. Both connected displays reported 96 DPI. The 0.1.14 render tick
consumed a queued Tk mouse message in an isolated Win32 probe. The hidden
GLFW context now lets Tk dispatch the Windows thread queue. The same probe
left the message for Tk, rendered the launcher scene and confirmed the
0.1.15 title. Six focused checks passed. A physical drag across the two
monitors remains for Jay to try. The open instance was left running.

The complete 207-scene payload is staged. The before and after checks,
source checkpoint, hashes and previous launcher are in
`.local/studio-window-drag-1106`.

**6 November Studio 200-scene preview staged.** Open
[Launch-Scene-Library.cmd](visual-studio/Launch-Scene-Library.cmd) for 0.1.14.
The new family selector groups the 25 families, eight scenes each. Studio
originals has the seven older scenes. This delivery adds 168 siblings to the
existing 24 anchors and eight Fault Ledger scenes. Each new scene has its own
layout and structure profile and a different Nested treatment within its
family. Treatments include cutouts, height, pigment, repeated impressions,
flow, fragments, beat relays, traces, shadows, spectral separation and fibres.

Jay requested the whole library in one delivery and will test it himself.
Scene tests, captures and launching the candidate were skipped. The staged
bundle is in `.local/visual-studio-releases/0.1.14`, with payload hashes and
the source checkpoint in `.local/studio-scene-library-1106`. Author sibling
changes in `visual-studio/authoring` and regenerate with
`scripts/build-studio-families.py`. Use the picker and the same Nested sprite
to compare treatments and record Jay's observations against the
[scene rulebook](docs/STUDIO-SCENE-RULEBOOK.md).

**6 November Studio Fault Ledger family preview staged.** Open
[Launch-Fault-Ledger.cmd](visual-studio/Launch-Fault-Ledger.cmd) for 0.1.13.
The first eight picker entries are Fault Ledger. They keep the geological
subject and vary the Nested mechanism across alpha fracture, brightness
relief, RGB veins, repeated stamps, directional motion, detached fragments,
beat delays and a persistent fossil trace. The
[rulebook](docs/STUDIO-SCENE-RULEBOOK.md#fault-ledger-nested-treatments)
records each treatment and requires mechanism variation within a family.

Jay will evaluate the complete family. Scene tests, captures and launching
the preview were skipped. The previous launchers are available. The 0.1.13
bundle is in `.local/visual-studio-releases/0.1.13`. Payload hashes and the
source checkpoint are in `.local/studio-fault-ledger-1106`.

**6 November Studio 25-family preview staged.** Open
[Launch-25-Anchors.cmd](visual-studio/Launch-25-Anchors.cmd) for Studio 0.1.12.
It starts at Fault Ledger and includes one original GLSL anchor for every
family in the [Studio scene rulebook](docs/STUDIO-SCENE-RULEBOOK.md). The
picker lists the family names. Each anchor has a procedural scene, a
music-driven action and an artwork-specific Nested route. Sediment Clock and
Barograph use stored state with clear controls. The other anchors use
procedural motion.

Jay requested all 25 together and will test them himself. Skipped scene
tests, captures and launching the candidate. The main Studio launcher still
targets verified 0.1.11. The 0.1.12 bundle is in
`.local/visual-studio-releases/0.1.12`. Payload hashes and the independent
source checkpoint are in `.local/studio-anchors-25-1106`. Use Jay's feedback
on the anchors before authoring the 175 siblings.

**6 November Studio Tidal Culture activated.** `visual-studio/Launch.cmd`
opens 0.1.11. Tidal Culture is an original GLSL scene using Biopsy_V2 as a
visual reference. Its full-frame two-reagent culture is advected by bass
and a diagonal pressure front on detected kicks. Nested artwork supplies
a local dose using its alpha and brightness. Its trace keeps moving as new
artwork starts feeding the field. Sprite imprint adjusts the dose and
Restart clears the field. Highs brighten colony edges.

13 focused checks passed on the exact staged Studio bundle. The kick changed
the stored reagent concentrations, and matched inputs verified that artwork
affects the culture through evolution. A four-second no-media run remained
active in every corner. Inspected Demo pulse frames without art and with two
nested sprites. The final candidate restores dense, varied growth after the
sparse and near-uniform early trials. The launcher title and 78 payload hashes
match. Longer real-track evaluation remains open. Captures, source checkpoint,
draft evidence and the rollback launcher are in `.local/studio-tidal-culture-1106`.

**6 November Studio reactive backdrops activated.** `visual-studio/Launch.cmd`
opens 0.1.10. Jay wants Studio scenes to fill the frame with reactive material
in addition to their nested-sprite treatment. Contour Bloom now sends a pulse
through an etched topographic field. Waxfall sends heat down a field of molten
currents. Both respond to bass and highs as well as kick events. The **Backdrop**
control sets the level of the field separately from the sprite effect. Leave
the central composition readable, and check future scenes with and without art.

17 focused checks passed on the staged Studio bundle. Both backgrounds showed
visible detail in all four corners and reacted to kicks and band levels without
a sprite. Inspected both scenes with Nested artwork under Studio Demo pulse.
The launcher title and 76 payload hashes match. Fresh look files are needed
for these new shader revisions. Real-track evaluation remains open. Captures,
source checkpoint and rollback launcher are in `.local/studio-reactive-backdrops-1106`.

**6 November Studio Waxfall activated.** `visual-studio/Launch.cmd` opens
0.1.9. Pick **Waxfall** and open a transparent PNG in **Media > Sprite >
Render as > Nested**. This original GLSL scene uses REDRUM as a visual
reference. Each detected beat sags the artwork and deposits its pigment at
downward-facing edges. Shed wax falls and fades as the artwork reforms. A
swap changes the source immediately, with existing drips allowed to finish.
Melt depth, Gravity, Drip life, Rivulets and Wax warmth tune the effect.

17 focused checks passed on the staged Studio bundle. Checked quiet and
paused frames, visible sag, colour isolation, falling and fading wax,
artwork swaps and repeated Demo pulse response. Inspected Alloy Vinyl and
Molten Droplet renders. The launcher title and 76 payload hashes match.
Real-track evaluation was outside this checkpoint. The source checkpoint,
captures and rollback launcher are in `.local/studio-waxfall-1106`.

**6 November Studio Contour Bloom activated.** `visual-studio/Launch.cmd`
opens 0.1.8. Pick **Contour Bloom**, set **Media > Sprite > Render as > Nested**,
and use **Open media...** for a transparent PNG. The original GLSL scene
seeds teal-to-copper contour waves from the current alpha boundary on detected
kicks. Swapping artwork immediately changes the outline without reloading the
scene. A built-in outline is available when there is no Nested input.

15 focused Studio checks passed on the staged bundle. The fixture's kick
front advanced 37 pixels, and a paused swap immediately moved the wave to the
new silhouette. Inspected renders of Crystal Star and Alloy Vinyl with
Studio's demo PCM and verified the launcher title and 74 payload hashes.
Real-track listening and physical controller use were outside this check.
Evidence, source checkpoint and rollback launcher are in
`.local/studio-contour-bloom-1106`.

**Later scene references.** Jay likes Synesthesia's REDRUM for how the sprite
appears to melt, and also wants Biopsy_V2 as a reference. Use original titles
for Studio scenes. Installed REDRUM blends the media mask into fresh splats,
then propagates and decays the deposited pigment. Biopsy_V2 uses persistent
simulation and blur passes, with media luminance influencing its shading.
The shipped controls and REA evidence references are recorded in
`.local/studio-contour-bloom-1106/synesthesia-references.json`.

**6 November Studio MilkDrop controls activated.** `visual-studio/Launch.cmd`
opens 0.1.7. On `.milk` presets, Bank 1 has Trail length, Zoom, Spin, Warp
and Beat sensitivity. Bank 2 has Nested Injection, Beat push, Angle and
Position X/Y. The controls are also available in Scene > Feedback and
Media > Nested sprite. Nested input controls apply to supported GLSL scenes
as well. The projectM 4.1.8 patch applies live feedback adjustments after
preset equations. Response can vary with custom shader behaviour.

The serial projectM build and 18 focused Studio checks passed. Verified
all four feedback changes with a preset that uses per-frame and per-vertex
code, visible trails with Injection at zero, matched Nested and Overlay
renders, both controller banks through simulated MIDI, and the 0.1.7
launcher title and payload. Live evaluation across Jay's presets and
physical FLkey operation remain open. Captures, runtime and launcher hashes,
source checkpoint and prior launcher are in `.local/studio-live-controls-1106`.

**6 November Studio 0.1.6 activated.** Jay declined the three Alloy ports.
Orbit Theatre, Inkstone Live Warp and Liquid Torus were removed from the
Studio picker and bundle. Future Studio scenes should be designed from
scratch around nested sprites. The four remaining bundled scenes opened,
and Vector Tunnel rendered the transparent Alloy Vinyl PNG in Nested mode.
The launcher opened the staged 0.1.6 title and the bundle hashes match.
Evidence, source checkpoint and the previous launcher are in
`.local/studio-nested-direction-1106`.

**Earlier 6 November Studio 0.1.5 port trial.** The trial added Orbit Theatre, Inkstone Live Warp
and Liquid Torus. Choose Media > Sprite > Render as > Nested. Orbit Theatre
projects the sprite onto a moving plane. Inkstone uses alpha to bend and
stencil the live marbling. Liquid Torus samples the sprite in both shading
and geometry. Each has a Music response control.

All 32 focused Studio checks passed, with 8 native link checks per scene.
Alloy 0.55.77 sent the actual post-master PCM and mix clock from its native
callback over private loopback ports using a generated bass fixture.
Inspected the three running previews, matched Nested/Overlay and live/silent
frames, and confirmed Torus relief changes the silhouette with artwork colour
at zero. The first harness run sampled the waiting label before the UI tick.
The final run waited for the label and tested the exact staged payload.
Whole-track listening and full regression coverage are outside this checkpoint.
The launcher title, command and 77 payload hashes match. Evidence, source
checkpoint and the previous launcher are in `.local/studio-ports-1106`.

The earlier shortlist from the review of 205 Alloy Originals included Resonant Iris,
Enamel Scales and Prism Rosarium. Oilprint, Mercurial Echo and Broken Spectrum
need closer work on simulation state. Faithful particle scene ports need
instanced rendering. Reference stills are in `.local/studio-port-reference-1106.jpg`.

**6 November Studio sprite routing and MIDI handoff activated.**
`visual-studio/Launch.cmd` opens the verified standalone Studio 0.1.4
bundle. **Media > Sprite > Render as** selects Overlay or Nested for an
image or video. Overlay composites the sprite over the rendered frame.
Nested uses the existing patched projectM pre-warp feedback hook, or a
Studio scene's active GLSL media or feedback sampler. The Media panel
explains the route available for the loaded scene. Vector Tunnel now
maps the sprite onto its wall panels. NestDrop's separate FX 00-99 bank
informed Studio's original Still, Drift, Orbit, Spin, Pendulum, Bounce
and Beat Pulse options.

**MIDI Input On/Off** in Studio's header releases or reopens the selected
Windows port. To control Studio alone, uncheck the FLkey input in Alloy's
**Tools > Audio / MIDI settings** before enabling it in Studio.

31 focused desktop checks passed. Inspected Overlay and Nested captures,
including visible MilkDrop trails after removing the sprite, and the
compact Studio layout. The launcher opens the staged 0.1.4 title and its
bundled projectM. MIDI was verified with simulated WinMM input. Physical
FLkey handoff and complete regression coverage are pending. Alloy stays
on 0.55.77. Evidence, hashes and the source checkpoint are in
`.local/studio-sprites-1106`. The 0.1.3 rollback launcher is in
`.local/visual-studio-releases/0.1.3/Launch-rollback.cmd`.

**6 November Shrouded and Controller wheels activated.** Alloy 0.55.77
(`5a1394a0e994`) is selected by `out/Launch.cmd`. Alloy's Controller adds
mouse-operated Pitch and Mod wheels. Each selector chooses a mixer lane and
cutoff, pan, bass, mid or treble. Pitch springs back after a drag and Mod holds.
Studio 0.1.3 opens from `visual-studio/Launch.cmd`. Its graphite and copper
Controller follows the Alloy layout, with scene and bank labels driven by the
actual mappings. Either Studio wheel can target a scene parameter, and
assignments follow each scene during the run.

Choose **Studio scenes → Shrouded**. Its eight reference controls, Cam Look
and a five-look Palette selector are available in the Scene panel. The FLkey
pitch and modulation wheels start on Cam Look X and Y. Tunnel independently
changes the feedback. Studio uses its own beat wave and procedural feedback
field. The installed Synesthesia descriptor and REA compatibility-renderer
captures informed the design. REA PE evidence
`ev_d8753c8446aef6c5ba8ef0625233a6896462ea644a4b962c2efb3e5802af93c1`
matches the prior native analysis host SHA-256. Reference file hashes and
renders are in `.local/shrouded-studio-1106`.

Seven focused Alloy checks and 31 Studio desktop checks passed. Inspected
the captured Controller, title, Shrouded frames and both Studio window
sizes. Verified software MIDI input, wheel retargeting and isolated loopback
audio. Jay confirmed the live audio feed after enabling **View → Share master
audio with Visual Studio**. Physical FLkey and full regression coverage remain
pending. The staged bundle, metadata and launcher hash match. Retained
reports, screenshots and source checkpoint are in `.local/shrouded-studio-1106`.
Rollback is `out/Launch.cmd.bak.version-0.55.77-20261106`.

**6 November Studio tempo and scene diagnostics activated.** Alloy 0.55.76
(`222192b558db`) is selected by `out/Launch.cmd`. Studio 0.1.2 opens from
`visual-studio/Launch.cmd`. Enable View > Share master audio with Visual
Studio in Alloy and choose Alloy live in Studio. Studio displays the lead
clip's detected BPM and uses Alloy's mix grid for the visual beat wave.
The GLSL uniforms expose both source and mix BPM. Vector Tunnel and Glass
Garden Music response run from 0.5 to 6. Manually opening Inkstone's Alloy
passes shows format guidance. Studio-authored multipass scenes use Studio's
own format.

Eight native and 23 desktop checks passed. Verified exact native master
samples, distinct 128.5 detected and 123 mix BPM in the UI and GLSL, both
response ranges, the long WAV workflow and both Inkstone diagnostics.
The visible Alloy title, Windows metadata, staged executable and launcher
match. Physical audio output and a full regression pass were outside this
checkpoint. Evidence and source snapshot are in `.local/studio-tempo-1106`.
Rollback is `out/Launch.cmd.bak.version-0.55.76-20261106`.

**6 November Studio live audio and long-mix checkpoint.** Studio 0.1.1
streams rendered mono/stereo mixes from disk. Alloy 0.55.75 (`aa138c5dc24b`)
is staged for the next launch. Enable View > Share master audio with Visual
Studio in Alloy and choose Alloy live in Studio. The independent local feed
shares the post-master signal before monitor mute and opens no second audio
output. Rollback is `out/Launch.cmd.bak.version-0.55.75-20261106`.

Seven native checks and 14 desktop checks passed through the retained runner.
Verified a ten-minute WAV above the old 200 MB decoded cap with a simulated
output device, exact Alloy master PCM over loopback, and visible scene
response. The captured Alloy title and Windows metadata both show 0.55.75.
Evidence, payload hashes and the source archive are in
`.local/studio-live-1106`. Hardware listening and
full regression remain outside these checks.

**6 November standalone Visual Studio preview.** Open
`visual-studio/Launch.cmd` for Studio 0.1.0. It runs in its own process with a
GLSL renderer, the pinned local projectM runtime and the Scene / Meta / Media
control layout from Jay's reference. Signal Loom, Vector Tunnel and Glass
Garden were authored for Studio. Studio starts with those three scenes and
opens GLSL and MilkDrop files on demand. It does not share Alloy's library,
session or visual catalogue. The Alloy launcher continues to select 0.55.74.

The verified Alloy FLkey 49 Custom Mode messages drive Studio. Pots and
pad banks follow each scene's descriptor. The faders target Meta and Media,
and pitch bend and modulation control the selected XY pad. Connect from
Studio's MIDI dialog. The desktop check passed 28 cases covering the real
window, control changes, mouse XY, MIDI message routing, look round trip,
GLSL variants, media, and a fresh MilkDrop test preset. Physical MIDI and
audible file playback are pending. Evidence and source checkpoint are in
`.local/visual-studio-1106`. Usage and supported formats are documented in
`visual-studio/README.md`.

Jay will choose the Synesthesia references for the next batch and supply
their scene names and control screenshots. Use the REA skill at that point
to investigate the chosen behaviour, then author Studio scenes from scratch.

**6 November sprite structure batch activated.** The launcher selects
V. 0.55.74 (`d7da62c14924`). Visuals / scenes > Originals now offers a
remembered Sprites folder and Reset for the bundled folder. This preference
is written beside workspace settings as `visual-sprites-folder.txt`. It
supplies automatic transparent sprites across mixes, with a pinned image
taking priority. Retain and Retained only controls were removed. All 205
Originals remain available. Historical shortlist data is preserved.

The batch routes coverage, filtered luminance, edge currents and premultiplied
pigment into the scene families. These inputs shape geometry and terrain,
steer persistent fragments and liquid, feed cells and reactions, break
signal regions, and seed or advect pigment in retained optical passes.
Changed 153 shaders. Inkstone keeps its existing Live Warp default and
selectable Retained Flow. The static gallery stills are from 0.55.73.

One below-normal, single-worker build completed. Windows file and product
metadata, compiled title version, staged payload and launcher hash agree.
Runtime tests and visual review were omitted at Jay's request. The open
session was left alone. Rollback is
`out/Launch.cmd.bak.version-0.55.74-20261106`. The exact payload, source
checkpoint, activation receipt and per-scene route map are in
`.local/sprite-structure-1105`.

**5 November Originals shortlist review activated.** The next launch uses
V. 0.55.73 (`25be14698975`). Retain checkboxes in Scene overview and Selected
scene preview share a shortlist saved separately from the arrangement.
Retained only filters the Originals browser. The initial 61 selections match
Jay's earlier review. Existing workspace choices take precedence over that seed.
Inkstone opens on Live Warp, with Retained Flow still selectable.

Refreshed 1,388 reference images across all 205 Originals and their named looks
using the current renderer, the same Alloy Vinyl sprite and a demo pulse.
The single-worker build, 9 native shortlist checks and 69 native visual
arrangement checks passed. Inspected the overview, enlarged preview and title
capture. Windows metadata, the displayed version and the launch target match.
Rollback is `out/Launch.cmd.bak.version-0.55.73-20261105`. The source checkpoint,
preview manifest, hashes, captures and receipt are in
`.local/retained-review-1105`. The open session was left running.
The next visual task is one library-wide pass after Jay chooses what to keep.

**Inkstone review decision.** Jay chose A, Live Warp, for a whole track.
Use Live Warp as the default in the next verified Inkstone build.
Keep Retained Flow available as the comparison look.

**5 November Inkstone pilot ready for visual review.** Reference 07 was chosen
for the treatment across the lettering, ring and open space. Inkstone is one
new Original with live warp, retained flow and a music-response-off look.
Coverage stencils the pigment and bends the warp at softened edges. Visible
RGB shades the material. A decaying pigment buffer adds spreading echoes
behind the live stencil. The paper and whole-frame finish are outside that
buffer. All existing scene assets retain their packaged identities.

The private 16-second reel and reference observations are in
`.local/pigment-study-1105`. The reel compares Oilprint, live warp, retained
flow and the same flow with ALLOY Vinyl. Its second half isolates the music
response on the same excerpt. Inspected 640x360 motion frames around the bass
return. All 16 renderer and reel assertions passed, including exact prepared
textures for both pinned PNGs, equal live stencils in A and B, source fit,
wake reset and matching source audio in both halves. The reference evidence
consisted of stills. Whole-track judgement remains open.

The isolated single-worker V. 0.55.72 candidate (`ce19b210ef1a`) has matching
Windows and visible title versions. The original waveform run caught the
detail-loading message at scale 800. The unchanged retry passed 45/45 and the
0.55.71 control also passed. The native visual-arrangement suite passed 68/69.
Its source-filter assertion still expected 204 Originals, and now expects
205 in the working source. Nine Python catalogue checks passed. The corrected
native assertion is unbuilt. Keep the 0.55.71 launcher until a new patch
version passes its scoped checks. Both failures, the retry and control,
source archives, and exact bundle hashes are retained in the study root.

**5 November palette update activated.** The next launch uses V. 0.55.71
(`49d1666ce632`). Copper and slate replaces Solar fire. Teal and silver
replaces Electric neon. Those looks, Acid lime, Jewel tones and Laser
primaries now blend at most 38% with the existing scene colours. Updated
397 gallery reference images and 80 scene descriptors. Matched captures
show less recolouring and byte-identical scene output before the finish.
The build, 36 matched renderer assertions and 45 scoped native checks passed.
Inspected title capture and Windows metadata both report 0.55.71. The
launcher and staged executable hash were verified. Rollback is
`out/Launch.cmd.bak.version-0.55.71-20261105`. Evidence, source checkpoint
and payload hashes are in `.local/palette-replacement-1105`. The open session
was left running. Full regression and subjective show review remain open.
Jay re-saved Metallic Orbit in Paint and reported a clean Explorer thumbnail.

The separate scene study prompt and ten source references are in
`.local/sprite-scene-handoff-1105`. Jay specified reactive domain warping,
frame-to-frame feedback and procedural material inside artwork-defined shapes.
The proposed pilot holds the material and warp steady while comparing live
warp with retained flow, then swaps the pinned sprite.

**5 November selector and Lantern Plankton comparison.** Jay identified the
pinned file as Copper Teal Wordmark - ALLOY Vinyl.png. The original PNG,
Windows Shell thumbnail and Alloy prepared texture looked clean for Vinyl.
Metallic Orbit showed red blotches in the Shell thumbnail only. Standalone
Lantern Plankton in 0.55.70 produced identical frames with and without the
pinned Vinyl cutout. It responded to an opaque copy as background input.
Solar fire at 100% produced the warm cast even with no image. The angular
composition in the user capture did not reproduce in standalone Lantern.
Oilprint used the pinned cutout and showed no comparable red blotching with
the final palette bypassed. Inspected evidence under `.local/sprite-input-1105`
in selector-check, vinyl-lantern and vinyl-oilprint.

**5 November Oilprint colour clarification.** Jay confirmed the liquid/text
distortion is intentional. The concern was the strong red and yellow. In
the packaged 0.55.70 renderer, default Solar fire at 100% covers the frame
in those colours even with zero media textures. With no sprite, the clean
martini and Metallic Orbit, switching Palette amount to 0% restored a broad
colour range. Each pair had byte-identical scene data before the palette
pass, including the distortion. The matched stills and report are in
`.local/sprite-input-1105/palette-control`. This result covers Oilprint.

**5 November MilkDrop sprite control.** The 0.55.70 renderer fed byte-identical
uploaded RGBA textures to Oilprint and a local projectM feedback fixture for
both Metallic Orbit and the clean Copper-Cyan Martini. The fixture warped
both in feedback without Oilprint's hard split outlines. Repeated the alpha,
RGB smoothing, aligned-channel and palette probes with the new sprite.
The first two made small changes. Alignment removed the offset copies while
preserving liquid shear. Palette off changed colour while split edges stayed.
This isolates a cause in Oilprint after upload. The comparison, hashes and
render probes are in `.local/sprite-input-1105/milkdrop-control`.

**5 November sprite input comparison.** Oilprint in the 0.55.70 packaged
renderer used the same six seconds of music, controls and timing for the
Martooni logo, Metallic Orbit and two Orbit derivatives. Removing alpha
codes 0-63 barely changed the streaks. Smoothing RGB softened some detail.
An isolated Oilprint shader probe aligned the RGB samples and made the
lettering cleaner with the original PNG. Disabling the final palette
changed the colours while leaving the separated edges. This identifies a
contributor in Oilprint. The Synesthesia scene was not reproduced.
Comparison video, inspected stills, hashes, probe and a tested clean
Copper-Cyan Martini PNG are in `.local/sprite-input-1105`.

**5 November delete/replace cleanup activated.** V. 0.55.70
(`dc432fb0b217`) is selected by `out/Launch.cmd` and the desktop shortcut.
Deleting or replacing a whole track removes its broken pair and preset nodes,
including guards, and clears manual nodes inside the former frame on the
surviving track. Its OUT/IN marker and manual nodes outside that frame are
kept. A newly added track starts clean. Undo restores the original pair.
Already-orphaned automation from an earlier deletion has no frame to clean.

The packaged build passed 191 preset and 69 focused range checks through
`verification.run_checks`. Native Delete and Add were exercised in both
pair directions. The surviving render matched untreated PCM. The broader
selection audit returned the same 10 failures out of 172 on V. 0.55.69 and
V. 0.55.70. Retained both reports, the source checkpoint, bundle identities,
and inspected replacement capture under `.local/delete-preset-1105`. The
capture, Windows metadata and launcher confirm V. 0.55.70. The prior launcher
is `out/Launch.cmd.bak.version-0.55.70-20261105`. Real-mix listening and the
complete regression gate remain open.

**5 November versioned local activation.** The Mixworks desktop shortcut
now selects V. 0.55.69 (`cf00b85f59c8`) through `out/Launch.cmd`. The title
area, Windows file and product versions, launcher target and executable hash
agree. The packaged candidate passed 45 focused native checks through
`verification.run_checks`. This source differs from the earlier 320-check
candidate only in the CMake version. Evidence and source checkpoint are in
`.local/version-activation-1105`. The previous launcher is retained as
`out/Launch.cmd.bak.version-0.55.69-20261105`. The standing rule to bump the
version and activate each verified local app update is in `AGENTS.md`.

Deleting a paired track can still leave preset nodes on its surviving
partner. This version includes the marked-placement fix and Match Mix
feedback. Deletion cleanup and real-mix acceptance remain open.

**5 November explicit transition presets and Match Mix feedback.** Marked
placement aligns OUT/IN with an empty treatment. Explicit built-in and saved
presets start at the join. Their named header moves the paired nodes and its
right edge changes duration. Remove preset preserves alignment and manual
nodes. Session format 50 persists ownership. Earlier unlabelled curves keep
their existing data. The visual browser shows a working message before Match
Mix plans scenes and reports the result in the same window.

Release candidate `b28cb9a1ac8f` passed 179 preset-node, 72 live-transition
and 69 visual-arrangement checks serially through `verification.run_checks`.
Inspected native captures for the named Crossfade and Match Mix message.
Source archive, bundle hashes, reports and captures are in
`.local/preset-ownership-1105`. This is a local candidate. Physical gestures,
real-mix listening and the complete regression gate remain open.

**5 November timeline-first editing checkpoint.** Right-click menus now
separate highlighted audio, segments, empty lanes, transition pairs and
automation. Details opens on demand, and the optional Transitions dock keeps
pair editing and A/B/C. The ruler shows selection policy and contextual entry
points. The console strip shows automation recording mode and arm state.

Release candidate `aefd78399ef1` passed 69 direct-range and 76 live-transition
checks via `verification.run_checks`. The isolated mouse walkthrough copied
six seconds, pasted on lane 3 at 32 s, undid the paste and applied Bass swap
from the frame menu with the dock closed. On the final build, the full A/B/C
controls fit at 1920x1080 and muted slot A rehearsal ended at its original
12 s cursor, paused. Candidate hashes, source archive and scoped reports are
in `.local/timeline-tools-1105/final`. Native captures are in `.local/tt1105/evidence`
and `.local/ttf1/evidence`. Both fixtures were saved and closed. Real-mix listening,
compact and second-monitor use, and physical keyboard context routing remain
open. This is a local preview candidate.

**5 November timeline tools audit.** W03 recommends target-specific menus,
on-demand precision and automation inspectors in place of the permanent Edit
details tab, and an optional Transitions editor for pair tuning, navigation and
A/B/C rehearsal. Source review and a native walkthrough on candidate
`776a73ac4531` are recorded in [the audit](docs/TIMELINE-TOOLS-AUDIT-20261105.md).

**5 November single track automation checkpoint.** The waveform buttons open
the track's Volume, Bass, Mid, Treble and Filter curves. Preset, Auto Arrange
and manual nodes share the same curve through and beyond each transition.
The mixer uses manual built-in controls. Previously saved mixer-lane points
stay in the document as inactive data. In Arrange, Touch and Latch write to
the playing track on the armed lane. The selected playing track wins overlaps.

The single-worker Release candidate `776a73ac4531` passed 185 Auto Arrange
checks and 10 focused playback and recording checks via `verification.run_checks`.
Inspected the native Filter capture showing preset nodes and a selected exterior
node on one line. The source checkpoint, bundle identity, reports and capture
are in `.local/track-curves-1105/final`. The executable is in
`build/detection-Release`. The production launcher still selects the earlier
Originals build. Complete regression and physical control checks remain open.

**5 November Originals refinement activated for the next launch.**
The launcher selects
out/releases/0.55.68-15fb3b0b3d13-originals-refinement/Mixworks.exe.
The versioned package contains the four-scene colour, surface and kick
refinements. From that package, 17 native kick checks through run_checks and
34 sprite and MilkDrop GPU checks passed. Inspected the native kick capture.
The focused receipt, full payload hashes and source checkpoint are in
.local/refine-kick-1105. The previous nested-relief bundle and the launcher
backup out/Launch.cmd.bak.kick-routing-20261105 are available for rollback.
Real-track accuracy, sustained fullscreen and complete regressions remain open.

**5 November Originals kick routing checkpoint.** The four preview scenes
reserve large motion for confirmed programme kicks. The audio callback stamps
the existing visual feed. Its worker scans consecutive 10 ms stereo windows
and publishes event times. The renderer uses a 45 ms rise and 225 ms recovery
on the programme clock. Torus now travels continuously between events.

The single-worker Release build passed. The candidate passed 17 native checks
through verification.run_checks, 17 focused Python tests and 18 renderer
checks. The muted native callback delivered a visible Reactor hit 74 ms after
the labelled off-grid onset and kept the bass-only section calm. The
four-scene fixture confirmed three kicks, zero events in the breakdown, and
identical kick geometry at matched 30/60 Hz timestamps. Inspected native
captures and the four-scene sequence. The source snapshot, reports and video
with synthetic soundtrack are in .local/refine-kick-1105. The executable
SHA256 starts 15fb3b0b3d13. Real-track annotation and drum-stem comparison
are next. Sustained fullscreen and broader scene rollout remain W15.

**5 November Originals surface checkpoint.** Ravine uses a continuous valley
with a selectable antialiased Lattice look. Nuclei trace 22 whole spheres.
Reactor, Nuclei and Torus use independent pigment and optional seam-aware
relief. Torus's camera keeps its opening in view through the sampled full
impacts. The Reactor-to-Torus join kept a pinned sprite with exact endpoints.

The isolated runtime passed 20 surface and 52 colour/coverage GPU checks and
15 focused Python tests. Inspected the matched 720p and 1080p camera frames
and normal views. The fresh shared sculpture program build took 688 ms versus
279 ms in the baseline probe. Steady 1080p render and readback medians were
7.5–12.8 ms on the candidate. These are bounded samples. Runtime, timings and
moving comparisons are in .local/refine-surfaces-1105. Kick routing is next.
Packaged desktop and sustained fullscreen performance remain open under W15.

**5 November Originals colour and coverage checkpoint.** The four refinement
scenes now default to sandstone and slate, mint, amber and cobalt. The vivid
palettes remain available. Sprite pigment preserves soft edges, while the
height field and displacement tint give faint pixels less weight. The coverage
change also applies to the other five scenes using the shared sculpture shader.

The isolated renderer passed 52 GPU checks and 23 focused Python tests.
Inspected the matched captures. At 480×270 an alpha-12 matte produced identical
frames and a uniform alpha-24 matte left the tested geometry unchanged. The
Reactor-to-Torus morph kept its pinned sprite and matched both endpoints.
The runtime snapshot, captures and reports are in .local/refine-colour-1105.
This is a renderer preview. Surface shaping, kick routing, full-resolution
motion, desktop packaging and show acceptance remain open under W15.

**5 November Originals investigation and refinement plan.** The selected
nested-relief bundle reproduced the purple cast on all four reported scenes
with no sprite loaded. Bypassing the shared palette removed it in the probe.
Faint alpha above the existing cutoff still affected material and height.
The 720p captures show Ravine's authored triangulation and sculpture artifacts
that persist after colour grading is bypassed. A synthetic bass-only note
maxed the hit envelope, and steady bass produced a separate grid-timed
sculpture response. Evidence is in `.local/scene-refine-1105`.
[Refinement plan](docs/ORIGINAL-SCENE-REFINEMENT-20261105.md) stages the four-scene
preview, kick routing, optional stem comparison and rollout under W15.

**5 November Auto Arrange mixing checkpoint.** Balanced, Bold and DJ set
combine equal-power volume curves with the bass and filter treatments selected
for the handover. Auto Arrange trims song levels from cached RMS and peak
measurements. New pairs are movement-locked, including in Auto Arrange
timeline, and the curves remain editable. Extensions preserve earlier lock
choices and clip gains. Completion details count the techniques used.

The below-normal single-worker Release build passed 177 focused Auto Arrange
checks through verification.run_checks. A synthetic 12 dB source mismatch
rendered within 2 dB across the song bodies. Sampled handover blocks peaked at
0.160 of full scale and remained audible. Inspected the native completed timeline
with its Locked badge and Bass and filter handover status. Retained the source
checkpoint, build identity, logs and captures in .local/auto-blend-1105.
The candidate is in build/detection-Release. Real-music listening and the full
regression gate are pending. Production playback and the launcher were left alone.

**5 November nested MilkDrop, sprite rotation and reactive relief activated.**
`out/Launch.cmd` selects `out/releases/0.55.68-0301291dbb2f-nested-relief/Mixworks.exe`.
The packaged runtime passed 34 sprite GPU checks before the atomic launcher update.
`.local/vnest/activation.json` binds the bundle, payload hashes and focused evidence.
The previous Library startup bundle and launcher backup remain available.
Pinned and automatic cutouts share the same input route. Automatic sprites change
every 256 beats (64 bars in 4/4), blending during the last bar. Background
selection has its own cadence. The projectM 4.1.8 patch calls the host before the
preset warp, so its own feedback transforms and decays the injected material.
The pinned archive, repository patch and bundled dependency record identify the
DLL build. Authored presets are read without changes.

Distance-field and camera morphs now cover eight sculptures and eight solid
instruments within their respective groups. The 52 compatible layered relief
scenes morph across 13 profiles by combining height, coverage and material
coordinates before shading. These reliefs can use the same sprite for height and
pigment. Molten Contours lifts its own image-derived height on bass attacks and
releases rising, dispersing image-coloured vapour. Relief Depth Off removes both
responses at the geometry pass. The remaining scene pairs still use feedback.

The serial Alloy and projectM builds passed. 208 focused native checks through
run_checks, 15 Python checks and 34 focused sprite GPU checks passed. The GPU
probe confirmed two relief joins and one solid join start from exact live
outgoing pixels with no feedback tail. The Molten probe compared quiet, kick
and disabled-relief geometry. Inspected the moving frame sequence and sprite
capture. MilkDrop retained a changing sprite trace after injection stopped and
rotated at the 64-bar boundary. Local evidence and exact source/bundle identities
are in `.local/vnest`. Remaining work includes field adapters for other pairs,
the full imported preset collection
and sustained full-resolution desktop cadence.

**5 November visual continuity checkpoint staged for review.** The installed
The Library startup bundle was selected at this checkpoint. The baseline GPU
probe reproduced the first overlap frame repeating the previous frame byte for
byte, followed by a long softened trail. The worker had been ignoring its
preload request. It now sends the live frame before preparing one upcoming
pipeline. The eight sculptures share a compiled shader, and a join within that
family interpolates signed distance, camera and controls in one live render.
Direct changes use two seconds. The other routes still use feedback, start an
overlap from the live outgoing frame and recover detail over 1.25 seconds.

The image picker pins a cutout across compatible Originals. A rotating sprite
pool now uses the same selection for each compatible scene at a given beat.
Sprite coverage stays active through joins. Near-transparent alpha codes 0-12
are removed before becoming material. Choosing a sprite while paused refreshes
particle pigment without advancing position. Sculpture bass affects shape and
camera, mids steer the turn and treble articulates the rim. This is a first
scene-family checkpoint. Cross-family parameter transfer, projectM sprite input,
original-scene curation and full-resolution desktop cadence remain open.

`.local/v1105` retains the before/after captures, candidate runtime identities,
source checkpoint and 22 Python plus 65 focused GPU checks. The Opal Reactor to
Chromatic Torso overlap began with the exact live outgoing pixels, visibly
changed shape and retired without a tail. The direct path reused the prepared
pipeline and held exact paused pixels. The intermediate candidate spent 4.99 s
preparing a second sculpture shader. The shared-program candidate prepared that
second scene in 4.51 ms in the bounded probe. First-family cold compile and
continuous desktop frame delivery remain unverified. The moving capture is
`.local/v1105/verified-join/join.gif`.

**5 November Library startup repair selected for the next launch.**
`out/Launch.cmd` selects `out/releases/0.55.68-0d1a41825c0b-library-startup/Mixworks.exe`,
SHA256 `0d1a41825c0b9d2a62d3a50f045333bcd7a337d6780f76c6c1771d9244e6990b`.
First launch migrates the original catalogue transactionally to
`%LOCALAPPDATA%/Mixworks/library.sqlite3` and keeps `library.json` in place.
The 128 MiB guard applies only to reading the old JSON. The active store
commits changed rows, with separate path and snapshot tables, and has no
128 MiB aggregate cap. Existing analysis revision and session format stay
current. The scanner keeps independent per-source analysis and Library saves.

Startup restores saved rows and checks sources in bounded batches. Initial
discovery skips rows already covered by that check. Later scans use file and
analysis stamps before opening records. A watched folder change queues an idle
pass, with periodic fallback. Tools lets Jay choose one root and its subfolders
or restore Windows Music. The Library notice separates checked, added,
refreshed and saved rows. Scan additions no longer dirty the mix. See
[Music scans](docs/IMPORT-SCAN.md).

Single-worker below-normal build passed after correcting a JUCE compile
error. The owned native startup and import scan fixtures passed 13 and 6
checks through run_checks. They verified migration with a correction, cache
reuse, scope, watch-triggered editing, committing a stopped scan, reopening
four tracks without reprocessing, and reporting/retrying failed imports.
Inspected the actual Library capture. The first-run migration on Jay's full
catalogue, large-library timing and complete regressions were outside this
focused check. Production was never launched or modified by verification.
Source checkpoint, build logs, receipts, capture and prior launcher identity
are in `.local/scan06`. Previous tempo-aware bundle remains available.

**5 November finishing plan resumed.** The selected Library startup executable
passed 37 focused demo/tutorial checks and 14 isolated desktop checks through
`verification.run_checks`. The Help guide generated all three optional demos,
saved and resumed its placement step, and advanced through a Library title
double-click, waveform range drag and muted playback. Bridge save enabled Finish.
Bridge reopen restored the clip and matched the prior rendered sample probe.
The owned app closed normally and the guide and timeline captures were inspected.
`.local/finish1105` retains receipts and the first driver's Help-menu failure.
First-launch, chooser, export and newcomer checks stay in W07.
[Demo record](docs/history/FINISHING-IN-PROGRESS-20261104-DEMO.md).

The selected executable also passed all 76 live-transition checks. During six
callback blocks across zoom and scroll, output matched an independent renderer
and the sample clock and project stayed stable. Source-backed reopen reproduced
the rendered PCM. An intentionally held lock produced silence and held the clock.
Two ordinary node-editor refreshes kept PCM and clock continuous. This bounded
device-free fixture leaves Jay's intermittent report open. See
`.local/finish1105/live.json` and the callback observations under `live/`.

[Finishing plan](FINISHING-PLAN.md) reflects the current build and the later
Library, movement, arrangement and visual checkpoints. Continue W03/W04 with a
contextual edit, Undo and refusal messages unless W01 becomes reproducible.
The six older failures from scan05 remain in W14/W28 pending a focused recheck.

## Earlier 5 November tempo-aware movement checkpoint

**5 November tempo-aware track and IN/OUT movement selected for the next launch.**
The launcher then selected `out/releases/0.55.68-f07f6e973183-tempo-aware-movement/Mixworks.exe`,
SHA256 `f07f6e973183c4da70ce992d7a960e1675eed5d7121e70aacbe6cc80c325b3aa`.
The previous build rejected single-track, IN, OUT and movement-locked pair drags
when the restored tempo ramp changed the local shape. Ordinary gestures now
retime beatmatched clips on their captured clock and keep the project tempo
markers and source automation. A movement-locked pair shares a beat displacement.
Explicit Ctrl-selected groups retain their rigid clock rule. If an earlier
live playback clock disagrees with the destination, save and reopen first.

The isolated real mouse fixture reproduced all four refusals on the prior
binary. On the selected build all four gestures succeeded with coincident joins,
and the 140/132 BPM markers, rendered crossfade, one-step Undo and save/reopen
passed. 52 focused native and 12 desktop checks passed through run_checks.
Exact source, hashes, activation, failed baseline and inspected captures are in
`.local/io37`. Verification used synthetic audio and muted transport. Existing
session and analysis formats are unchanged. Production playback was left alone.

## 5 November startup library investigation

Automatic scanning is enabled. A read-only audit found 6,637 supported files
under Windows Music, including 4,959 in one collection. The saved catalogue
contained 3,190 paths and 2,230 revision-11 snapshots, leaving 960 saved paths
to hydrate. The file was 304,846 bytes below the 128 MiB limit. Of 3,447 Music
paths absent from the catalogue, 313 had separate revision-11 analysis records.
The audit checked record presence and revision. The latest scan report recorded
three missing-file failures and two audio-read failures.

Startup publishes the saved catalogue and queues source checks. It also starts
recursive Music discovery after about two seconds when idle. Discovery repeats
one minute after a completed pass and reparses analysis records for known files.
Restoring an entry without a snapshot takes the reader path, which reads
metadata and hashes the audio twice even when reusing analysis. The per-source
analysis write and debounced catalogue write are independent. Added/refreshed
counts reflect live rows. A catalogue save reserializes the whole snapshot and
rejects the entire update at the cap, including new paths. Scan additions also
mark the mix dirty.

The selected Library startup repair above implements incremental catalogue saves,
migration, scan roots, source checks and separate reporting. Representative scale
and migration of Jay's full catalogue remain W14.

Evidence in `.local/scan05`. The selected candidate's isolated native library
suite confirmed small-catalogue save/reopen and display before source checks.
It recorded 121 passes and six failures beginning with an arrangement
volume-fade refusal and including the later arrangement-admission fixture.
The audit did not time a full startup. Production data and playback were untouched.

## Earlier 5 November BPM display checkpoint

**5 November BPM display precision checkpoint.**
Bundle `out/releases/0.55.68-bb9d3466a7e9-bpm-display/Mixworks.exe`,
SHA256 `bb9d3466a7e9bd6fbb15728a8c2d77b0ce63f0b2d645951ce300fa5a22f2248d`.
Timeline, tempo strip, transport and Library display rounded integer BPM.
Source BPM/Key tools and the tempo marker editor display up to three decimals.
Saved tempo and the exact measured grid retain full precision, including Undo/Redo.

Single-worker below-normal build passed. 111 focused grid checks passed through
run_checks. Inspected actual 140 BPM timeline/Library/transport capture and
128.372 BPM Advanced editor capture. The isolated bridge saved the original
139.56789 BPM to the tempo marker and captured source clock. Source, captures,
retention and activation evidence are in `.local/bpm36/final`. The same hash
passed the separate linked-drop handoff checks in `.local/io36/verified.json`.
No production session, analysis cache, session format or detector was changed.

## Earlier checkpoints

**5 November linked Library drop fix is selected for the next launch.**
The launcher then selected `out/releases/0.55.68-408d060fbef3-linked-library-drop/Mixworks.exe`,
SHA256 `408d060fbef358701cffeed69a820515f7fb017955a64f9d34a7e7e1f721de89`.
Jay's open candidate34 timeline showed separate IN/OUT markers, no pair button
and a tempo-curve movement refusal. Reproduced with real mouse gestures in an
isolated 140/138 BPM fixture. A Library drop in another lane across a track with
an explicit unpaired OUT now creates an unlocked pair at that OUT and keeps the
established tempo. A drop elsewhere retains independent placement. During
playback an aligned drop must start ahead of the playhead.

One below-normal, single-worker build and 49 focused native checks passed.
The rebuilt desktop fixture verified the actual drop, independent title drag,
locked movement, unlock, both marker gestures and save/reopen. Captured pixels
and exact positions are in `.local/io35/desktop/w/evidence`. Source checkpoint,
native receipt and activation record are in `.local/io35`. Scope is local
workflow verification with muted transport. Existing unpaired placements need
a new drop to use this behavior. Session49 and analysis revision are unchanged.
Candidate34 and the previous launcher are retained. The production session
remains open on candidate34 and was observed read-only.

## 5 November linked Library drop tempo and crossfade repair

The next-launch `bb9d3466a7e9` BPM-display bundle includes the drop repair.
Candidate35 forced the marked OUT join to None and suppressed the incoming
tempo target. A real Library drag reproduced both failures. The drop now keeps
the authored OUT and exact incoming IN, uses the preset captured at drop time,
and holds the established grid through the join. It ramps to the incoming BPM
when the source has room, otherwise adds an editable marker at the existing
tempo. Existing linked drops need a fresh drop to generate the treatment.
Movement remains subject to the existing tempo-curve guard.

On the exact next-launch bundle, 49 focused native checks and five desktop
checks passed through retention with stress off. In an isolated 140/132 BPM
fixture, the join stayed at 36 seconds, the incoming marker reached 132 BPM,
rendered PCM confirmed the crossfade handoff, and the pair and markers survived
save/reopen. Inspected the native timeline capture. Evidence and failed baseline
are in `.local/io36`; exact source is in `.local/bpm36/final/source.zip`.
Verification used synthetic tracks with known BPM, with no listening or full gate.
Production playback and data were left alone. Session49 and analysis revision
are unchanged.

## Previous sprite-treatment checkpoint

**5 November sprite-treatment expansion complete locally on candidate34**,
SHA256 `507d7d4ac273f3b1dd3912836da9302590a172e79a00d5e7aae51aaddc8c4460`.
Bundle `out/releases/0.55.68-507d7d4ac273-sprite-methods/Mixworks.exe`.
Source/UI0.55.68, session49, analysis revision unchanged.

Jay requested the three sprite treatments in more scenes. They are now available
independently in **33 Originals**: Molten Contours plus all eight implicit
sculptures, eight solid instruments, eight landscapes and eight liquid surfaces.
Scene context menus already derive availability from control schemas; these now
expose Relief depth / Domain warping / Displacement colour, each Off/Gentle/Strong.
Each added scene has five new method presets (three alone and two combinations).
The previous six looks retain the three controls off; Auto Arrange keeps them off.
No particle/MilkDrop/flat-scene overlay or transition sprite injection was added.
Supported scene names and access are listed in docs/VISUAL-SCENES.md.

Shared sampling warps premultiplied artwork before reading it. Colour is applied
only at surface shading; it never feeds back into the height or simulation source.
Relief follows each scene's existing structure: implicit distance fields, solid
surfaces, terrain/cliff height and liquid optical height/normals. The liquid
surface treatment is embossing, not a new mesh extrusion renderer. Zero depth
skips extra height sampling. Molten's separate relief-plate shader retains its
existing method behaviour; its duplicate shared uniform declaration was removed.
Opacity/noise handling from33 remains, and original files are byte-identical.

Added160 preset thumbnails (32 x5), preserving existing reference stills. Manifest:
`previews/expanded-sprite-methods-20261105-manifest.json`. Procedural PCM,12 settling
frames/look with bounded forward increments, no audio device. The preview tool now
accepts several named presets and caps sparse frame steps so references do not
reset retained simulations on each capture. Inspected four-family contact sheets,
individual GPU/preset captures, native browser output and an eight-second silent
runtime example. This is a scoped functional/visual preview, not whole-show FPS
or exhaustive artistic acceptance.

**Verification:** one successful single-worker /MP1 below-normal build;273 native
checks (Original workflow208, visual arrangement65) via verification.run_checks,
35 Python checks and453 GPU checks (32-scene expansion392; nested routing28;
all four engine feedback directions33). Every added scene produces distinct
relief/warp/colour/combined outputs, holds exact pause, leaves pre-shading fields
unchanged for colour-only, and has exact no-op output without sprites. Six
representative combined surfaces retain exact alpha0-4 noise invariance. Native
catalogue/presets, disabled defaults, all204 saved-scene schemas and Undo pass.
Zero native infrastructure/retention errors; stress off. No full regression,
physical/device/listening or sustained720p acceptance. Initial GPU proof harness
failures are retained: explicit-image reporting was confused with automatic
selection, then a shaded finish target was wrongly classified as a geometry field.
The corrected harness retains all intended assertions and passes392/0.

Evidence/source/runtime identities: `.local/sprite-methods-20261105`; native source
checkpoint `.local/fw04/candidate-34-expanded-sprite-methods-source.zip`. Candidate33
is preserved;34 copies changed files and hardlinks only unchanged resources.
No app restart, public release or production cache/analysis/correction/recovery
reset. Previous random scheduling and slow-forward MilkDrop repair remain. The
separate library128MiB snapshot defect remains deferred.

## Earlier 5 November scene-local sprites - candidate33

**5 November scene-local sprite follow-up complete locally; next-launch bundle is
candidate33**, SHA256 `b608d839707ad4f3843ac81424693904bbc7732d7f459a391ddb2633e940b4d1`.
The launcher then selected `out/releases/0.55.68-b608d839707a-nested-sprites/Mixworks.exe`.
Source/UI0.55.68, session49, analysis revision unchanged. The user's running
application, session, devices and original artwork were not controlled/restarted.

Jay clarified that the two Martooni PNGs also show discolouration as wallpapers,
just less. Their alpha1–4 fractions are about0.4%/1.4%, versus36% for ALLOY Spectrum.
Paint correctly hides those almost-transparent RGB bytes. The original80-scene
reference manifest used Martooni.png itself, so source identity alone did not
explain the rough previews. Additional runtime contributors were boosted opacity,
straight-RGB filtering and sharp image data feeding implicit-surface normals.
Runtime now preserves authored coverage, filters premultiplied source textures,
and gives sculpture geometry a broader mip footprint than colour. Alpha1–4 remains
clear; source files are byte-identical. Intentional scene recolouring stays enabled.

Sprites now enter only supported Original scene geometry/materials (particles,
implicit sculptures, solid instruments, landscape surfaces and Molten relief).
Removed the shared recurrent sprite layer, including automatic MilkDrop injection.
Flat image effects and transitions choose no fresh sprite. An outgoing logo can
persist only inside that scene's rendered generative handoff. Existing background
image roles remain separate. Presets/projectM private feedback are unchanged.

Molten Contours is now a lit image-heightfield plate with bevelled luminance,
animated column warping and displacement-driven neon. **Relief depth**, **Domain
warping** and **Displacement colour** have separate right-click Off/Gentle/Strong
menus, three individual presets and two combinations. Editing one leaves the other
two intact; Undo and saving retain them; Auto Arrange keeps disabled methods off.
This establishes all three independent methods in this scene, not universal new
controls on every Original or a reconstruction engine for MilkDrop.

Regenerated491 named-look/reference stills across the80 affected scene group plus
Molten Contours using the exact current shaders and Martooni.png, with owned
procedural PCM, no audio device. Manifest: `previews/nested-sprites-20261105-manifest.json`.
Inspected contact sheets, relief/warp/colour captures, native browser output and a
six-second silent motion preview. This refresh is reference imagery, not live-show
or whole-library performance acceptance. Red remains where it is authored palette
or visible artwork; the repair targets fringes and unintended amplification.

**Verification:** successful single-worker below-normal build;206 native checks
(Original workflow141, visual arrangement65) via verification.run_checks,34 Python
checks,117 GPU checks (three supplied sources x28 plus mixed-engine feedback33).
Exact GPU invariance to alpha0–4 noise in relief/sculpture/solid geometry for all
three sources; separate-method output, pause, source retirement and all four engine
handoff directions pass. Native controls, Undo, saved204-scene schemas and actual
browser captures pass. Zero retention/infrastructure errors; stress off. Candidate32
retains its141/1 result: a stale assertion required six presets for every image
scene; corrected for Molten's11, unchanged functional assertions all pass on33.
No full gate, hardware/listening, 720p FPS or exhaustive artistic acceptance.

Evidence/failures/runtime-source archives: `.local/red-transition-20261105`;
native snapshots32/33: `.local/fw04`. Previous31 bundle is immutable and preserved;
new33 copies changed resources and hardlinks unchanged files. No production data,
analysis/corrections, cache/recovery or protected-reference reset. Candidate31's
random scene scheduling and slow-forward-frame MilkDrop repair remain included.
The deferred library128MiB snapshot issue remains separate.

## Earlier 5 November visual follow-up — candidate31

**5 November visual feedback follow-up complete locally; next-launch bundle is candidate31**,
SHA256 `f051e58a0ccda615bc12afc0c2695b8eb1f87cbe046302647eaa8b7cffd4dc59`.
The launcher then selected `out/releases/0.55.68-f051e58a0ccd-visual-recovery/Mixworks.exe`.
Source/UI0.55.68, session49, analysis revision unchanged. The current user app was
not restarted, controlled or modified; its existing scene history stays live.

Jay reports red sprite fringes in Originals (MilkDrop looks OK). His ALLOY Spectrum
PNG has red/yellow RGB quantisation noise at alpha1–4; Paint/thumbnails hide it.
The sprite visibility lift and image-driven surface normals amplified it. Runtime
upload now treats these bottom four alpha codes as clear, before feedback. Source
artwork is unchanged. A failing Python regression passes; actual Chromatic Torso
and Image Gallery GPU comparisons now have exact zero output difference between
the original image and a reference with only that near-transparent noise removed
in memory (before: up to228/255 difference,501 changed Torso pixels over20/255).
This isolates that fringe; it does not remove colours intentionally derived from
visible artwork or prove every Original scene's artistic appearance.

Auto Arrange/Match mix now chooses randomly among the least-used eligible scenes
before repeating, retaining source selection, family/motif spacing, timing and
particle limits. Previously random jitter.18 was overruled by energy/style ranking.
Native seeded coverage produced15 distinct openings across32 independent seeds;
a48-scene show uses all24 eligible choices in each complete pass. Generated choices
remain saved/stable during playback. Candidate30's1/65 failure is retained: small
consecutive LCG seeds were correlated; candidate31 uses a deterministic seed stream
and retains the same >=8 opening-variety requirement.

Jay also reported about2fps on MilkDrop **suksma - my time slips away, soon i die,
enjoy pizza roam** (`2a0938e0...102c`). A forward gap over.3s incorrectly destroyed
and recreated all scene instances. Engine seek/stop/open already publish explicit
generation changes. The Python worker now resets for changed generation/backwards
time, retaining state through slow forward frames. Exact-preset bounded640x360
probe: six.5s-spaced frames previously created six instances, about198–258ms each;
now one instance advances six frames, about1.6–2.1ms after initial190ms startup.
This demonstrates removal of the reload loop, not live desktop/720p/whole-show FPS.
Physical output under Jay's actual session still needs observation after relaunch.

**Verification:** one-worker below-normal builds;236 focused native checks passed
(visual arrangement65, Auto Arrange171) through run_checks, stress off, zero
infrastructure/retention errors.22 Python checks and38 actual GPU feedback checks
passed, plus the two exact sprite-noise comparisons and named-preset before/after
probe. Native visual-lane and GPU captures inspected. No full gate, stress,
whole-library performance, hardware or listening qualification. Previous29 bundle
and original sprite hashes preserved; new31 bundle copies only changed executable/
runtime files, hardlinks unchanged resources. Evidence/source hashes/failures:
`.local/sprite-fringe-20261105`; native source snapshots30–31: `.local/fw04`.
No production music, analysis, corrections, cache/recovery or reference reset.
The later Library startup checkpoint replaces the capped active JSON store.

## Earlier 5 November Auto Arrange repair

**Auto Arrange repair complete locally; candidate29 is superseded by31 above**,
SHA256 `8c1fde0151a15a0ae268e6ac640ad39928011731f8f98dbf829383a65a78de42`.
The launcher selected `out/releases/0.55.68-8c1fde0151a1-auto-arrange/Mixworks.exe` at that checkpoint.
No application was launched/restarted or production session/data changed.
The reported echo also occurred in another player; Jay identified his headphones
and confirmed it resolved. No audio-processing change was made for that report.

Auto Arrange's catalogue-only planning called unchanged-position movement through
the new ready-audio guard, stopping at one track. No-op moves now validate without
requiring loaded PCM; actual movements retain readiness/protection checks.
The real GUI workflow additionally exposed loss of source-owned automation at
commit: the generated `effectControls` now travel with the arrangement. Node
compaction and assertions now use that active storage. Fixtures explicitly choose
Create rather than the current default Extend and inspect current status/details.
Source/UI0.55.68, session49, analysis revision unchanged.

**258 focused checks passed** serially through `verification.run_checks`, below
normal/stress off, zero infrastructure/retention errors: copied real catalogue5,
Auto Arrange171, pair movement45, carried demo28 and tutorial9. Candidate22's exact
catalogue reproduction placed1/10 despite1742 eligible sources; candidate29 places
10/10 from the same starting song. Native GUI creation/extension, prepared
playback readiness, actual curve ownership, cancellation and Undo/Redo passed;
completed two-track screenshot inspected. This does not establish real-music
listening or a complete regression. Evidence and original failures26–28:
`.local/echo-20261105`; source checkpoints26–29: `.local/fw04`. Candidate29's
demo/tutorial native passes superseded the unrun25 checks. The selected-build
demo journey is recorded above.

**Earlier observed defect:** the running UI reported the library snapshot's
128 MiB storage limit. The unchanged retained snapshot had2230 analysed entries
while the live library showed3244 sources. This is a separate catalogue persistence
follow-up, not lack of compatible music; do not clear/reanalyse music to address it.
Original catalogue and all analysis/corrections/caches/recovery/reference evidence
remain untouched. A hashed independent snapshot supports the planner reproduction.

## Previous 4 November checkpoint

**Session ended at Jay's request,4 November2026.** Resume from
[FINISHING-PLAN.md](FINISHING-PLAN.md) when requested.
After completing partial linked-range editing, Jay prioritised urgent Snap,
IN/OUT, pair movement locks and transition removal, then a seekable library-title
preview strip. **The urgent implementation and library preview are complete locally.**

Corrected contract: OUT changes in its stationary track; incoming audio moves to
keep IN aligned. IN shifts incoming audio beneath the previous fixed OUT, keeping
the join fixed (confirmed). Next append aligns to the last explicit OUT. Snap
uses only the timeline grid after free/fine/on; no prior offset or neighbouring
pulse becomes a snap reference. Pair movement buttons replace LINK; consecutive
locked pairs move together by headers. Unlock the active pair for IN/OUT alignment.
Advanced Protect edits is separate. Remove transition offers keep current mix or
clear frame treatment, with explicit consequences, Cancel and Undo.

Source/UI0.55.68, **session49**, analysis revision unchanged. Candidate12 passed
**219 focused checks** (pair45/live70/direct68/trim36), serially through run_checks,
stress off, zero infrastructure/retention errors. Candidate11's43/2 real marker
gesture failures exposed range selection intercepting anchors; fixed and retained.
Candidate13 is the same behaviour plus a verified title-spacing fix. SHA256:
`6b21201004d5f0acf9b2b88de23ee736d8968e566d0d1a148c9a522dc5000c55`.

Fresh isolated bridge `.local/uq13/w` PID26668 verified actual pair-only movement,
Undo, project-grid downbeat5.5s/beat11, reopen, readable controls and exact stereo
PCM before/after translation. Transport stayed stopped; no listening claim. App
saved/gracefully closed, PID exited, ownership-checked retention completed.
Source checkpoints, original failures, counts, captures and oracle measurements:
`.local/fw04`, `.local/uq13/w/evidence` and the
[dated core completion record](docs/history/FINISHING-COMPLETION-20261104-CORE.md).

**Library-title preview implemented and verified locally:** candidate15 SHA256
`7082f66d62a48401b68b0333e59b85b6d3577cfd31c7a0eb155efc735a8e40bd`.
Readable title/seek strip, pointer and keyboard seek, same-source decoder reuse,
latest async seek, recycled-row/focus-loss cancellation. Arrange pauses mix for
master audition; Record/Perform uses configured headphones. Native seek27,
preview27, library39 =93/0; required bridge native31/live27 =58/0. Actual decoded
early/late/private PCM and real isolated-app seeking observed, captures inspected.
Candidate14's26/1 accessibility fixture failure is retained; hidden peer repair
passed15. Python bridge harness17 OK/1skip. `.local/fw04` and `.local/qb15` retain
source/tooling, failures, reports and zero-error ownership retention. Owned app
saved/closed and exited. [Usage](docs/LIBRARY-PREVIEW.md), dated core record.
No computer-use skill. No physical gesture/listening/hardware qualification.

Jay requested faster builds while no other processes were running: use normal
build priority for this session, still one worker and serial essential checks.
Candidate15 incremental compilation changed only two implementation files; tests
remain below normal/stress off. Continue core workflow roadmap after this feature.

**W02/W05 live transition/cue implementation accepted locally on candidate22**,
SHA256 `357f8c561ffdf04f07754ac7e3cc39e1d76e01d77e4e805777f0cca5d7777d81`.
Whole-preset replacement now clears actual source curves; sharp ordinary EQ cuts
run at their exact sample in the shared processor; timeline curve inventory reads
actual controls and deduplicates shared lines. Source/GUI inspections uncovered
these defects despite earlier passing checks. Off-tick PCM error.2532635 is now0
at all four tested block sizes. Preset172/live76/shared-lane49 =297/0; bridge
native36/desktop37 =73/0; harness20 OK/1skip, zero infrastructure/retention errors.
Actual performed take curves, playback/capture/cue clock, confirmation, save/reopen
and graceful owned close/restart/close observed; captures inspected. Finalising a
take restores the pre-take arrangement; its performed project event is the oracle.
All earlier failures and corrected-oracle provenance are retained in `.local/fw04`
and `.local/qb16` through `.local/qb22`, detailed in the dated core record.

**Earlier W03/W07 checkpoint.** Candidate25 built with readiness and save/reopen
diagnostics after candidate24's two failures. Its planned run was superseded by
later passing candidates. [Demo record](docs/history/FINISHING-IN-PROGRESS-20261104-DEMO.md).

**Launcher updated by explicit latest-successful-build request:**
`out/Launch.cmd` targeted `out/releases/0.55.68-357f8c561ffd-core-workflow/Mixworks.exe`,
verified candidate22, independent of unverified25 in build/. Backup:
`out/Launch.cmd.bak.63` (previous save-defaults target). Source22/evidence identities,
focused-local activation record and resource hashes preserved in
`.local/fw04/activation-22.json` and the release's `local-preview-verification.json`.
Shared model/runtime files use hard links; original visual runtime/Python preserved
from the previously selected local bundle, supplementary visual resources retained.
No public package, complete regression receipt or new qualification is claimed.
The app was not launched/restarted. No owned build/test/app process remains after
wrap-up. Normal build priority was a session-local speed override; retain the
standing low-disruption policy on resumption unless Jay changes it.

W02/W05, W31/W32 and the partial linked-range implementation are recorded in the dated
completion record. Other W03 newcomer/shortcut/accessibility, W06 cross-renderer,
W07 genuine first-user chooser and W08 caption/DPI acceptance remain open. W01's
intermittent zoom/audio issue has no specific passage and is unconfirmed. One
bounded actual callback experiment proved lock-contention silence and exact PCM
after node-editor refresh lock scopes were shortened; model construction still
uses some locks. Do not call that a broad dropout fix.

No full regression, stress/performance, physical MIDI/ASIO or musical-listening
qualification. No production process/session, original music/video, protected
reference, scan or analysis/cache reset changed during that checkpoint. Purchases,
agreements, public distribution and final-qualification prerequisites remain
restricted. Continue independent authorised roadmap work when Jay resumes the task.

## Save preference and historical implementation baseline

New mix saves/Save As and mix-related WAV, take/live-master, playlist/track-list
and transition-preset exports start in the current user's **Desktop/Mixworks**:
`C:\Users\jayte\Desktop\Mixworks` here. **File → Default save folder** offers
Choose and reset. It persists in workspace preferences outside musical Undo;
named layouts do not change it. One-off file choices do not change the default.
Ctrl+S updates the current file. Directory creation happens on use; errors explain
the problem and how to change the folder. Cancel/write failure preserves the
previous choice. MIDI profile export retains its separate profile workflow.
[User guide](docs/EXPORTING.md#default-save-folder).

The earlier launcher selected `out/releases/0.55.68-f2b641134513-save-defaults/Mixworks.exe`.
It has now been replaced by verified candidate22 above. This historical save-folder
baseline used source/UI0.55.68 and executable SHA256:
`f2b6411345131abd60065ecdf920b162bdec4ac279fd43b9f0cc508c1163a4c8`. Previous smooth-feedback bundle retained;
backup `out/Launch.cmd.bak.save-defaults-20261104`. No production app was started,
stopped or restarted. A read-only process inventory observed Mixworks PID30824;
this is a dated observation, not a permission to control that process.

One single-worker, `/MP1`, below-normal build passed. **57 focused native
save/export checks passed**, serially through `verification.run_checks`, stress
off, zero failures/infrastructure/retention errors. Actual folder and save chooser
pixels inspected. Packaged bridge in fresh `.local/sv04` verified protocol/root/
PID/capabilities, saved/reopened an empty isolated session, captured inspected UI,
saved again and gracefully closed. No full regression, stress, performance,
physical gesture/hardware or musical listening acceptance claimed.

Independent evidence: `.local/docs-roadmap-20261104` contains original document/
targeted source archive, original task inventory, build/native logs, exact focused
report, candidate source ZIP/hashes, staged payload hashes and activation receipt.
Immutable runtime/model assets reuse exact-file hardlinks; mutable media settings
are copied. No user music/video, sessions or caches are added to source commits.
No commit/push or public package performed.

## Carried-forward implementation and evidence

The selected bundle retains the preceding visual runtime: **204 original scenes**
(203 with six named looks plus Jumping Techno Tunnel), the approved punch direction,
completed 60-scene energy/coverage review (38 changed, 22 retained), transparent
sprite evolution and all Original/MilkDrop generative joins. Sharpness recovery
smoothly restores incoming live detail before inherited feedback retires. Runtime
module hashes match the prior smooth-feedback bundle; no visual/DSP/detector or
session-format change was made for save preferences.

Prior visual evidence is candidate-bound: `.local/transition-recovery-20261104`
(36 recovery GPU, 38 existing GPU, 14 Python checks) and
`.local/milkdrop-sprites-20261104` (239 native, 38 GPU, 29 Python). These were not
rerun or promoted to whole-product acceptance. [Behaviour and limits](docs/MILKDROP.md).
Scene results: [energy review](docs/SCENE-ENERGY-20261104.md). Artistic judgement,
representative whole-show/hardware and sustained performance remain W15/W30.

The completed 60-second promo revision remains ready for creative review at
`.local/p62/deliverables/Mixworks-Alloy-Forge-Your-Mix-60s.mp4`, SHA256
`8543c68ce26adaa0a9af7786dd315213611482664f08833635f8a8e2ab480e1c`.
Prior exact decode/provenance evidence remains there; no new listening approval or
public upload. Original Sonilo themes and alternatives are preserved.

The [pre-consolidation handover](docs/history/documentation-audit-20261104/HANDOVER.md)
retains full chronological evidence, creative decisions and failures. Its old
activation/next-session/reset/scan labels are historical. The current roadmap
controls continuation; [release-audit status](docs/release-audit/STATUS.md) retains
whole-rule qualification, not a competing work order.

## Operating constraints and continuation

Use this folder's Git repository. Preserve production sessions, analysis,
corrections, caches/recovery, original media, protected downbeat references and
independent baselines. No cleanup/reset/rescan follows from documentation or save
preferences. Do not resume deferred NestDrop collection scanning or historical
bulk music recovery without an agreed window. Custom crates remain plan-later,
implement-later. Constant-tempo 4/4 music accuracy remains the analysis priority.

Batch coherent changes; build with one worker below normal priority, run essential
focused tests serially through the retention harness, stress off. Numeric RAM
guards are removed; stop owned testing for actual responsiveness problems. No
routine full gate/stress/capacity/frame benchmark. Before controlling Alloy, read
[development bridge](docs/DEVELOPMENT-BRIDGE.md), use a fresh short owned root,
verify identity/capabilities, await outcomes, save before close and inspect pixels.

No whole-product release qualification or distribution clearance is claimed.
[Personal Beta Licence](LICENSE) and [third-party release prerequisites](docs/LICENSING.md)
remain. Hardware/ASIO/FLkey/private-output listening, independent music accuracy,
full current regression and clean/compliant packaging are W11–W14/W28/W30.

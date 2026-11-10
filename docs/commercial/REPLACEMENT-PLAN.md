> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Commercial dependency replacement — implementation record

Visual planning has moved to [Alloy visual direction](../VISUAL-DIRECTION.md).
The dated visual milestones below are historical records, not an execution order
or permission to expand the library. The current gate is one convincing scene
with reusable foundations for 100 distinct scenes. No accepted proof exists yet.

Commercial release obligations remain governed by [LICENSING](../LICENSING.md)
and the clearance register. This visual documentation reset does not resolve them.
The [previous plan](../history/COMMERCIAL-REPLACEMENT-PLAN-BEFORE-VISUAL-RESET-20261102.md)
is preserved intact as history.

## 1. Historical single-scene implementation milestone

Files: `visuals/original/scenes/prism-bloom/{scene.json,main.glsl}`,
`scripts/original_visual_gl.py`, `scripts/original_scene_player.py`,
`scripts/verify_original_visual.py`, `scripts/visual_scene_worker.py`,
`scripts/visual_compositor.py`, `Source/VisualScenes.cpp`, `Source/SceneLane.cpp`,
`Source/SceneLaneTests.cpp`, `Source/SelfTest.cpp`, `Source/Workstation.h`,
`CMakeLists.txt`. Evidence: `.local/original-visual-01/`.

- [x] Preserve before-source checkpoints; build an original Prism Bloom shader
  using analytic layers, antialiased edges, dark-to-light range and slow rotation.
  Inputs are resolution, scene seconds, project beat, independently calculated
  PCM energy bands and an optional user image. No extracted helpers/scene assets.
- [x] Implement a bounded GL context/target and player with the existing frame
  envelope: zero/two actors, 360p/720p/1080p, pause/seek, transition and native
  presentation. Validate clocks, actor keys, controls, PCM size and image bounds.
- [x] Require the original runtime and its packaged `original-scenes.json` marker;
  catalogue key `original/prism-bloom`. Stage only owned runtime files and scene
  content in `original-visualiser-runtime`; never copy prototype helper assets.
  Existing installed preview remains untouched. Its venv supplies libraries only
  during this milestone; replacing that development dependency is task 4.
- [x] Add a focused native `--original-visual-only` journey covering catalogue,
  placement, Undo, saving/reloading, actual mixed PCM, GPU picture and pause/seek.
  Add bounded GPU verification for sharp output, audio response, unchanged paused
  frames, media preservation on failure, image handling and two-actor transitions.
- [x] Build with one worker below normal priority. Run the focused native
  check through `verification.run_checks`, serially with Python GPU checks.
  Capture native integration and original 1080p output; inspect actual pixels and
  create a short muted animation using an authored synthetic audio input.
- [x] Record exact source/candidate hashes, test scope, dependency imports and
  observed limits; show Jay the scene for judgement before making more scenes.

Acceptance: recognisable deliberate composition; clean edges at native 1080p;
visible but controlled music response; no random full-frame flashing; repeatable
pause/seek; scene timeline works; no original Synesthesia code/assets/imports in
the new path. Quality is assessed on its own merits, not exact scene-copy parity.
No 60 Hz guarantee follows from a short capture or software timing result.

Execution result: candidate `b8ad5e40a5d850755ad22b36051502cd97cb71aaf30a39656beeb6461cf5f7d6`;
native14/0 and GPU15/0, source/bundle manifests and inspected captures in the
evidence directory above. One main build and a short final incremental packaging
build, with tests serially afterward. Installed preview unchanged. Jay's visual
acceptance remains pending. Disk save/parse was checked, not a fresh-process
session reopen; fullscreen controls/physical gestures/real music remain untested.

## 1b. Visual feedback and architecture decision — 2 November 2026

Jay tried Prism Bloom with real music: response was correct, with no apparent
slowdown or errors. Its visual complexity was insufficient. Treat it as a verified
renderer proof, not the artistic target. References named by Jay: **Deeper V2,
Glassier, Voronoi Geode**; broadly he values the collection's complexity and
interesting movement over flat pulsing shapes. He asks whether we can recreate
the scene construction approach without taking supplied scenes directly.

Yes, the next visual milestone should implement our own composable scene system.
Published [scene-format documentation](https://www.syn.live/docs/ssf/ssf.html)
describes shaders plus scene configuration, controls and optional scripting;
[pass documentation](https://app.synesthesia.live/docs/ssf/json.html) describes
multiple render targets and previous-frame feedback. These are useful functional
references, not a grant to copy bundled scenes or extracted helper code.

Next authorised direction, after the waveform defect is resolved:

- Specify our own pass descriptor and uniform names. Add bounded intermediate
  targets, explicit pass order, ping-pong history and deterministic reset on seek.
  Use owned CPU parameter controllers initially; a JavaScript engine is not
  necessary merely to achieve complex motion.
- Add independently implemented smooth audio envelopes, separate frequency bands
  and beat/onset signals. Map them to camera/material/shape parameters at different
  time scales rather than driving every effect with the same loudness pulse.
- Develop one original spatial scene before a larger pack: procedural cellular
  structure with genuine occlusion/parallax, changing camera perspective, layered
  material detail and reflective/refractive-looking lighting. Use original geometry,
  mathematical functions and colour design; do not translate existing shaders.
- Use HDR intermediate lighting and restrained bloom/tone mapping where needed.
  Add feedback only when it serves the composition. Preserve bounded memory and
  preview quality settings; measure the chosen scene on the actual GPU without
  asserting performance from Prism Bloom's simpler workload.
- Show real moving captures for Jay's judgement. Success means spatial depth,
  evolving internal detail and musical motion, not merely a more elaborate flat
  outline. Exact visual parity or permission to redistribute reference scenes is
  not claimed. Compare appearances without reading/extracting their shader code.

This is a scene-authoring architecture direction; it is not a request to duplicate
Synesthesia's proprietary runtime or a completed legal clearance. The existing
plan's owner/licensing/package obligations still apply.

**Spatial preview implemented, 2 November 2026:** Prism Bloom now demonstrates a
cellular shell with a separate internal lattice/core, occlusion, camera parallax,
material bands and glass-like environment lighting. The collection remains one
scene. Own ordered pass descriptors, bounded HDR targets, tested optional
ping-pong history/reset and CPU bass/mid/treble/onset envelopes are implemented.
This composition uses four lighting/bloom/tone-map passes without feedback trails.
See [runtime and descriptor notes](../../visuals/original/README.md).

One incremental single-worker below-normal build; native14/0 through retained
`verification.run_checks`, GPU23/0 and controller/descriptor3/0, run serially.
Native browser and1080p captures plus four motion samples inspected. Eight-second
960x540/15fps WebP uses authored synthetic PCM, with no music. Exact source and
bundle evidence: `.local/spatial-visual-02/`; native executable remains the current
waveform-fixed `93d7187d5d0d19afbb41b7c0d9a35db064e4796a4e5c921d341c802e5c444e69`
because this change is in the packaged Python/GLSL runtime. No activation or user
preview restart. No full gate, sustained/1080p performance claim or real-track
acceptance. **Jay's artistic judgement is the next decision; pack expansion stays
deferred.** Fine-edge shimmer and whether the material/movement feel sufficiently
rich should be assessed in motion, not inferred from passing checks.

## 1c. Three-style audition and reference study — authorised 2 November 2026

Jay judges the spatial scene closer to the target, but its motion and audio
response dull. He explicitly requests2–3 additional contrasting scenes and an
opened Alloy preview. During implementation he also requests REA/a close study
of one reference and delegates the choice. Selected Voronoi Geode. This supersedes
the earlier hold on pack expansion and no-reference-code-reading direction for
this specific authorised study; it does not authorise copying/distributing source.

- [x] Add Neon Run, Velvet Current and Ember Mosaic with distinct geometry,
  materials, movement and audio mappings; preserve Prism Bloom for comparison.
- [x] Replace saturated visual level mapping with a soft knee; add faster
  envelopes and accumulated transient-driven motion. Verify identity switching,
  all three band responses, image colouring, pause/seek and distinct-scene blends.
- [x] Use REA on the reference controller/package and directly inspect shipped
  configuration, shader and thumbnail; record observations versus unknowns in
  the [construction study](VORONOI-GEODE-STUDY.md). Author the reconstruction using
  procedural material instead of the installed images or copied scene/helper code.
- [x] Build and verify: native26/0, base GPU23/0, pack GPU31/0, unit5/0. Retain
  source/bundle/capture evidence in `.local/visual-variety-03/`. Original native
  save-fixture failures retained; a second serial build/run corrects that fixture.
- [x] Open `.local/pv03` with a private copy of Zero B's Lock Up from the previous
  preview library; cycle all three new scenes every24 seconds with2-second blends.
  Save the audition and leave it stopped, unmuted, with the scene browser visible.
- [ ] Jay judges visual direction, musical response and reference closeness.

No exact pixel/motion parity, complete native audio-detector reconstruction,
sustained-performance result, full regression or distribution clearance claimed.
Unrelated Sprites artwork in the source tree is preserved and excluded from the
final candidate runtime. The unused early Ion Storm prototype is retained locally.

## 2. Commercial research boundary

Files: `CMakeLists.txt`, `Source/Main.cpp`, `Source/SelfTest.cpp`,
`Source/RhythmDecoder.{h,cpp}`, research registration/callers.

- [ ] Move AUFTAKT feature/network/filterbank and research CLI/test inputs behind
  an explicit private-research target. Remove recovered decoder branches from the
  commercial target without changing the Beat This! frontend/postprocessor.
- [ ] Retain original research and evidence; inspect commercial source and binary
  manifests for recovered paths/constants. Verify the ordinary detector against
  the preserved candidate with an appropriate release-checkpoint scope.

Acceptance: production detection unchanged on the selected corpus; commercial
target does not compile recovered research. Historical legal review remains open.

## 3. SoundTouch compliance without a purchase

Files: `CMakeLists.txt`, `Source/ElasticAudio.{h,cpp}` if an ABI wrapper is needed,
packaging scripts, `THIRD-PARTY-NOTICES.md`, `docs/LICENSING.md`.

- [ ] Select a SoundTouch-only shared target retaining float samples, current
  version, optimisation and stretch settings; avoid global shared-library changes.
- [ ] Package exact library source/modifications/build instructions and LGPL text;
  align application terms with replacement and debugging rights.
- [ ] Verify a rebuilt compatible DLL can replace the delivered copy. Compare
  render duration/output, pitch/stretch, live playback and missing-DLL errors.

Acceptance: supported music workflows and sound preserved; actual replacement
works; source/notices accompany the qualified package. A DLL alone is insufficient.

## 4. Standalone original visual distribution

Files: original runtime, CMake packaging, new pinned runtime manifest/installer,
`scripts/verify_visual_package.py`, third-party notices.

- [ ] Pin a minimal Python/NumPy/Pillow/GLFW/PyOpenGL runtime with native notices.
  Do not ship PyAV/FFmpeg, OpenCV, sounddevice, dukpy or pip unless a documented
  implemented feature actually requires them. Original PCM analysis needs no
  microphone or desktop capture; procedural scenes need no JavaScript interpreter.
- [ ] Remove the prototype-venv path from commercial startup. Verify on a clean
  Windows test environment without Synesthesia or development source folders.
- [ ] Confirm package allowlist, hashes, offline startup, GPU failure messages,
  child ownership/exit and bounded images/targets. Review embedded native licences.
- [ ] Replace compatibility-collection-specific visual suite fixtures and port
  their applicable UI/lifecycle/fullscreen coverage to the original backend.
  The focused14-check journey does not substitute for that broader release scope.

Acceptance: an independently launchable original renderer with exact dependency
rights/notice inventory. This is separate from qualifying the whole application.

## 5. Historical visual expansion checklist — superseded

Files: `visuals/original/scenes/`, original catalogue and automatic-look metadata.

- [ ] Apply Jay's feedback to Prism Bloom before expanding. Add deliberately
  different fluid, geometric and electric scenes, each with its own provenance.
- [ ] Define bounded controls, energy profiles and still-image integration for
  automatic arrangement; validate transitions between distinct scenes at all
  quality settings and inspect full-screen output.

Acceptance: useful variety and coherent music response, judged with Jay. Exact
Synesthesia presets, names, shaders and artistic output are not a target.

## 6. Optional models, owner actions and release closure

- [ ] Establish exact HTDemucs weight permission; if unavailable, compare clearly
  licensed replacements for separation quality/runtime before offering stems.
- [ ] Resolve tonal-profile data permission and complete transitive runtime
  notices. Keep video export outside the launch offer until separately qualified.
- [ ] Jay identifies the legal seller and confirms JUCE Starter eligibility;
  Jay executes the free ASIO agreement. Agent prepares records, not signatures.
- [ ] Update clearance register, qualified package manifest and licence materials;
  complete the project's release gate on the final candidate and obtain scoped
  professional advice on outstanding IP/history/brand/patent questions.

No later milestone is represented as completed by the single-scene demonstration.
## Historical 100-recipe implementation

The earlier catalogue expansion is recorded in [VISUAL-LIBRARY-100](VISUAL-LIBRARY-100.md).
Recipe counts did not establish visual quality. Its previous instruction to expand
before proving one scene is superseded by the current visual direction.

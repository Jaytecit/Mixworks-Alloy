# Scene identity refinements — 3 November 2026

Jay authorised the first changes from the [full scene audit](SCENE-SIMILARITY-AUDIT-20261103.md): plain defaults for the 28 sixfold scenes, distinct city scenes and distinct ribbon-feedback scenes. The five approved image instruments remain byte-for-byte unchanged.

## Result

| Scope | Behaviour |
| --- | --- |
| 28 relief scenes | Default, Signature and Silk drift use the authored plain view. The optional Prismatic look and manual reflection choices remain. Geometry, palettes and musical behaviour of these scenes are otherwise unchanged. |
| Afterhours Skyline | A fixed-height waterfront dolly, a readable skyline and stable roofs. Bass lights window districts, mids sweep facade/reflection detail, treble adds roof and water glints. |
| Dreamline Metropolis | A low street canyon with taller crowned towers and a clear road. Mids steer the view and light billboards, treble advances traffic, bass accents the road and forward motion. |
| Graphite Wake | A thin calligraphic stroke with a long lateral ink wake. Mids change curvature and ink strength; treble articulates filaments. |
| Ember Advection | Separate rising ember streams with short trails and faster decay. Bass drives burst strength/rise height, mids deflect jets and treble adds hot accents. |
| Opal Smoke | Broad soft billows with slow upward advection, diffusion and persistent history. Mids roll/replenish the volume; bass adds local pressure. |

The two city shaders are expanded from `visuals/original/shared/city-instruments.glsl` using `CITY_KIND` 0/1. The three feedback shaders expand `ribbon-instruments.glsl` using `FLOW_KIND` 0/1/2. Expanded `main.glsl` files ship directly; no new runtime preprocessor or shared renderer change is required. These are authored shader effects, not a new physical traffic/particle/fluid engine. Descriptor audio mappings, motion notes and camera-clearance notes describe the new mechanisms.

Chromaflora (`neon-culture`), Kinetic Bloom (`chromatic-shoal`), Lucent Ranges (`terrain-prism`), Mercurial Echo (`liquid-memory`) and Broken Spectrum (`signal-fracture`) retain their exact scene files, controls and looks. Shared renderer/controller scripts, all other scene directories and the 203-scene catalogue are unchanged. The library still has 1,218 named looks. Existing saved per-scene overrides are preserved; this change does not rewrite a running or saved arrangement's selected reflection method.

Eighty-six affected reference images were refreshed: Signature/Silk for the 28 default changes and all six looks for the five revised scenes. The other four looks of each default-only scene keep their authored treatments. The scoped manifest is `visuals/original/previews/scene-identities-20261103-manifest.json`.

## Verification

- One successful single-worker, below-normal-priority build. The executable remains SHA-256 `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`; this update changes shipped shader/descriptor/reference assets, not C++ behaviour. Runtime asset identities distinguish the new candidate.
- The focused GPU review passed **159 checks** over all 33 changed scenes: render success, finite light state, moving appearance, exact pause, four pre-finish signal conditions for the five new mechanisms, and distinct mid/bass response for wake and smoke. Three-time samples and all 33 endpoint images were inspected. All 30 refreshed looks of the five revised scenes were inspected together.
- **14 runtime tests** and **50 native visual checks** passed. The native suite used `verification.run_checks`, with artifact retention enabled and stress disabled. No full regression, stress, capacity or benchmark gate was run.
- Fresh `.local/sd1`, protocol 1, owned PID25592, verified root and capabilities. The selector showed 203 scenes and refreshed reference pixels. At 360p, all five new mechanisms and six representatives of the plain-default families delivered moving native frames. Their 11 scene/control records survived exact save/reopen. Actual native pixels for the five mechanisms plus woven/paper defaults were inspected; smoke was additionally inspected after 2.7 seconds of real playback to let its history develop.
- The owned app was monitor-muted before playback, saved as `closed-reviewed`, and gracefully closed. Its copied excerpt/artwork are deliberately retained for reproducible review. Production apps, music, settings, caches, analysis and protected downbeat references were untouched.

Evidence is retained independently of Git in `.local/scene-distinct-20261103`: before/final source archives and hashes, staged runtime `candidate-01`, GPU report/motion/band sheets, reference images, build/runtime/native reports, native delivery/save/reopen facts and seven inspected native screenshots. The isolated bridge workspace retains its own session and command evidence.

The initial native setup incorrectly assumed monitor mute was already enabled and stopped before playing. The real checkbox was then enabled in the owned app and bridge state confirmed it. The original failure log remains. A first occluded window capture was rejected; only foreground-validated native pixels were retained. Neither issue required application source changes.

## Delivery and limits

Source and build assets are ready for preview; the ordinary launcher and existing immutable images80 release bundle remain unchanged. No production instance was restarted, no commit/push or public binary/package release was made.

The comparison movie is `.local/scene-distinct-20261103/preview/Alloy-Scene-Identity-Preview.mp4`: 30 seconds, 640×360/30, 900 frames, stereo. It uses a continuous owned music excerpt and six seconds per revised scene, with an explicit offscreen-renderer label. Complete decode passed. The movie is a review artifact rather than a recording of the native UI; native evidence is retained separately.

This establishes the stated short visual/musical mechanisms, not every source image, long feedback evolution, native mouse editing, listening acceptance or sustained display performance. A feedback scene starts faint after a seek/reset and develops during playback. Jay's next useful judgement is the balance of skyline/canyon speed, ember density and smoke persistence. The remaining audit groups (image stages, glass, liquid, signal and others) remain deferred.

## Afterhours travel follow-up

Jay accepted the other refinements but reported that Afterhours did not visibly travel across the city. Its `.18 * travelTime` lateral camera term was too slow at the default motion setting: approximately .55 world units over six seconds. It now uses `1.4 * travelTime`, retaining the fixed camera height/depth, stable horizon and all lighting responses. Only Afterhours' main shader and the `CITY_KIND == 0` recipe are changed; Dreamline's checked-in shader and every other scene directory remain byte-for-byte unchanged from the accepted refinement candidate.

The earlier generic moving-image check also counted colour/lighting changes and did not establish perceptible camera travel. This correction adds silent pre-finish geometry renders and a same-input, side-by-side 12-second movie. Visible displacement, finite state and exact pause were checked; six current look references were regenerated and inspected. Image-correlation estimates in the exploratory probe are not used as camera-motion acceptance because parallax and repeating windows create ambiguous matches.

Evidence is `.local/afterhours-travel-20261103`, staged runtime `candidate-01`; movie `Afterhours-Before-After.mp4` is 1280×360/30, 360 frames and stereo, with complete decode and encoded sample inspection. One incremental single-worker build passed. Native integration, runtime units and broader suites were not repeated for this camera-only shader change; the earlier receipts retain their original candidate scope. Production app/data, launcher and release bundle remain unchanged. Jay's acceptance of the other refinements is recorded in HANDOVER.

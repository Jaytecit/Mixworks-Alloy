# Stage and glass identities — 3 November 2026

Jay accepted the Afterhours travel correction and asked to continue with the next audit group: Pulse Gallery, Orbit Theatre, Resonant Iris, Rhombus Foundry and Enamel Scales. All other scene directories and shared renderer/controller modules remain byte-for-byte unchanged, including the five original approved image instruments, the city/feedback refinements and the 28 plain-default changes.

| Scene | Distinct appearance and behaviour |
| --- | --- |
| Pulse Gallery | One frontal lightbox with a readable source image. Bass changes frame depth/light, mids chase around the frame and treble accents its corners. |
| Orbit Theatre | The image itself travels on an orbiting perspective plane, changing yaw/roll, with three independent satellites. Mids advance the orbit; treble advances/flares satellites. |
| Resonant Iris | Hard shutter leaves occlude/reveal a source plane. Bass opens the aperture, mids index/highlight the leaves and treble lights the mechanical rim. |
| Rhombus Foundry | Large staggered diamond glass panes travel laterally across one stationary, continuous image. Bass tilts/refraction, mids advance the panes, treble lights straight bevels. |
| Enamel Scales | An oblique view of overlapping curved scales, with the complete image behind them. Bass hinges individual scales open; mids send a row wave and treble lights curved rims. |

These are authored projection/relief effects, not a new physical glass or shutter simulation. The stage instances expand `visuals/original/shared/stage-instruments.glsl` with `STAGE` 0/1/2; the glass pair expands `glass-instruments.glsl` with `GLASS_KIND` 0/1. Older image-stage/relief recipes remain available for their historical or other instances; do not regenerate these five from the older recipes. No runtime/preprocessor changes, new controls, saved-format changes or scene key/title changes. Defaults and Signature controls/audio mappings are updated; other named look identities and explicit view/image treatments remain. The catalogue still contains 203 scenes and 1,218 looks.

The glass pair deliberately retains one continuous source through the moving carriers. The image is not duplicated separately in each tile. Enamel projects the entire image/carrier plane obliquely with matched aspect, so the perspective difference does not stretch or discard part of the source. Iris intentionally occludes portions of its image as the aperture closes; Gallery preserves a frontal view and Orbit carries the source on its plane.

## Evidence and scope

Evidence root `.local/stage-glass-20261103`; current frozen runtime `candidate-03`, previous `candidate-02`. Before/final source snapshots, hashes and generated review artifacts are independent of Git. Only the five main shaders/descriptors and two new authoring templates changed in application source.

- Main focused GPU review: **48 checks passed** on candidate02. It covers finite surface output, exact pause, isolated bass/mid/treble causality before the common colour finish, Gallery/full-glass source regions, actual Iris source-aperture opening, visible Orbit source-plane movement under silent input and two targeted live dissolves. Orbit's source centroid moved about 35 pixels at 480px width between the sampled silent endpoints. Iris's exposed source area grew from 9,262 to 31,572 pixels under the isolated bass stimulus; neither result counts common colour animation as geometry movement.
- A final oblique-view refinement touched only the glass shaders. **19 focused glass checks passed** on candidate03, including complete four-region image transmission for both scenes, local band causality, pause and a glass-pair dissolve. The three stage scene files are exact between candidate02/03, so their earlier focused evidence carries forward without rerunning it.
- All five three-time motion sheets and the thirty current look images were inspected. Thirty references are installed under `visuals/original/previews`, with `stage-glass-20261103-manifest.json`. Eighteen stage references were reused by exact source identity after the final glass-only refinement; twelve glass references were rendered again.
- The comparison movie is `.local/stage-glass-20261103/media-final/Alloy-Stage-and-Glass-Preview.mp4`: 30 seconds, 640×360/30, 900 frames, stereo. Six seconds per scene use a continuous recorded music excerpt. Complete decode and actual encoded sample inspection passed. This is labelled offscreen rendering, not native screen capture or listening acceptance.

The first draft's broad checks passed but visual review exposed a half-black Gallery image: a negative-base GLSL power was undefined. This was corrected, a full-source Gallery guard and a visible mid-chase check were added, and the final 48-check pass is the applicable evidence. A corresponding scale-profile square was made explicit. The earlier frames/logs are retained; they are not artistic acceptance. The first glass layout also needed a stronger viewing-angle distinction, addressed by the final oblique projection.

One single-worker, below-normal-priority build and **50 focused native visual checks** passed. The executable remains SHA-256 `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`; this is an asset-only update. Native window delivery was not repeated: renderer, bridge and application interactions are unchanged, and current build/frozen runtime asset identities are checked. No full regression, stress, capacity, benchmark, physical input or listening qualification is claimed. Native checks used `verification.run_checks` with artifact retention enabled and stress disabled.

## Delivery

Jay approved the five revised scenes with “Ok, passed.” Source and build assets retain that approved state. Production app/session, user music, analysis/caches, launcher and immutable release bundle remain untouched. No commit/push or public binary/package release. All accepted scenes remain protected. Liquid/signal overlaps and the other audit groups remain deferred.

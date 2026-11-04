# Dunes / Escarpment refinement â€” 3 November 2026

Jay authorised implementing the two refinements after accepting the
[landscape comparison](LANDSCAPE-COMPARISON-20261103.md). Lucent Ranges
(`terrain-prism`) and every other approved scene remain exact.

| Scene | Result and musical response |
| --- | --- |
| Opaline Dunes | A continuous curved surface replaces triangular terrain and bright mesh edges. Long lateral ridges carry travelling wind ripples while the camera drifts along them. Bass breathes the crests, mids accumulate wind displacement, and treble adds fine ripples and shimmer. |
| Lucid Escarpment | A steep image-relief wall, top silhouette and low ground replace the shallow ramp. The oblique camera travels along the face. Bass extrudes ledges and lifts the rim, mids shift strata, and treble lights fine fissures. |

Only the two main shaders and descriptors change: **four existing scene files**.
`shared/dune-instrument.glsl` and `shared/escarpment-instrument.glsl` are the
authoring recipes and match those main shaders after newline normalisation. Both existing source-height
shaders, pass graphs/resolutions, catalogue keys, runtime/controller modules and
native source remain unchanged. Defaults and Signature response controls are
updated; other named looks retain their explicit values. There remain 203 scenes
and 1,218 looks. These are authored procedural image landscapes, not physical
erosion/geology simulations. Older landscape templates must not overwrite the
revised instances.

## Evidence and limits

Evidence root: `.local/landscape-pair-20261103`; frozen runtime `candidate-01`.
The earlier frozen `.local/landscape-comparison-20261103/runtime` remains the
before-render baseline. Independent before/final source ZIPs, target-reference
backup, identities and reports are retained. Exact before-hash comparisons cover
all other scene directories/references, including Lucent Ranges and every approved
city, feedback, stage/glass, liquid, signal and pigment scene. All source-height
shaders, runtime Python, native source, CMake and launcher remain byte-for-byte
unchanged.

The focused GPU review passes **23 checks**: recorded-input motion, finite targets,
exact paused pixels/targets, independent bass/mid/treble causality in the main pass
before common finishing, two targeted dissolves and white-source form comparison.
In the white-source probe the bright part of the upper half is about 16.5% for
Dunes versus 53.7% for Escarpment, supporting the visually inspected low-surface/
steep-wall separation. This is a bounded form check, not a general landscape
classifier or physical validation.

The existing protocol's separate scene and motion clocks allow isolated movement
checks without changing the renderer or inserting test controls. Holding scene
time fixed while advancing motion time proves both cameras travel with a fixed
image-height target. Holding the camera fixed while advancing scene time proves
Dunes' wind moves the surface; the quiet cliff remains exactly fixed in that
control probe. Camera-only, fixed-camera and white-source pictures were inspected.
No failed draft or optional additional styling cycle was needed.

The longer preview passes **26 render/target/pause/movie checks** across six
24-second jobs: approved Lucent Ranges and the revised pair, with logo above and
opaque chart below. All columns use identical image/PCM/musical times and a
temporary common Electric neon palette, with their own current response defaults.
The anchor's eight sampled JPEG frames match the earlier comparison **8/8 exactly**,
in addition to source preservation. The four revised motion sheets and all
12 current named-look references were inspected; the anchor samples had already
been inspected in the baseline comparison and were checked for exact equality.
Actual encoded movie frames at 4, 12 and 22 seconds were inspected as well.

Movie: `media/Alloy-Landscape-Pair-Preview.mp4` under the evidence root; 24 seconds,
1,152Ã—520/30, 720 frames, stereo. Complete decode and stream/frame counts passed.
It is labelled offscreen rendering and identifies unchanged approved Ranges.
Only the pair's 12 references and their new manifest are installed. The movie
demonstrates two sources and one passage, not all-image/all-show qualification.

One serial, below-normal build and **50 focused native visual checks passed**
through `verification.run_checks`; generated fixtures are cleaned while reports,
hashes and source evidence remain. Stress stays disabled. Executable SHA-256 is
still `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
asset identities distinguish this candidate. No full gate, stress, performance,
native-window rerun, device or musical listening acceptance. No production app,
session, music, analysis/cache, launcher or immutable release bundle changes;
no commit/push or public release. Preview ready for Jay's judgement; not activated.

The existing serial renderer/same-input workflow, already selected after the
skills.sh method check, remains suitable. Impeccable's scoped refinement and
bounded visual review principles informed the work; web markup detection does
not apply to these GLSL scene assets.

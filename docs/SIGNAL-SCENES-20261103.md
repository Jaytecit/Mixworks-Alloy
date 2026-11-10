> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Signal scene refinement — 3 November 2026

Jay authorised the next signal batch while preserving every approved scene.
Five Signal Collage siblings now have different geometry, motion and image
acquisition rules. Broken Spectrum (`signal-fracture`) remains the exact approved
anchor, as do all other scene directories, looks and runtime scripts.

| Scene | Identity and musical response |
| --- | --- |
| Glass Interlace | Broad alternating image columns travel vertically in opposite directions. Their capture intervals differ. Bass offsets columns and refreshes a sparse subset; mids accumulate opposing travel; treble lights vertical joins. |
| Raster Roulette | One horizontal acquisition bar rolls through the image, replacing retained rows with a slowly moving capture plane. Bass lights/displaces its wake, mids shear horizontally and treble leaves a short luminous wake. |
| Afterimage Static | A fixed mosaic holds actual older image captures on independent staggered schedules. A moving capture plane makes those differences visible with still artwork. Bass refreshes a sparse subset; mids tint newly captured blocks; treble lights small capture markers. |
| Circuit Confetti | Sparse detached image patches move and rotate independently across empty space. Bass expands islands, mids alter travel/rotation and treble lights their rims and releases memory. |
| Chromatic Splice | Broad diagonal strips slide in opposite directions along dark cut lines. Bass separates cuts, mids accumulate slip and treble lights chromatic fringes/releases captures. |

`shared/signal-instruments.glsl` and `shared/signal-memory.glsl` are authoring
recipes expanded into only these five main/memory shader pairs using
`SIGNAL_KIND` 0–4. Only their three-file sets (main, memory, descriptor) change.
Existing region solvers, pass graphs, state contracts, catalogue keys,
controllers and shared runtime remain exact. Defaults and Signature response
controls are updated; other named looks retain their explicit controls. The
catalogue remains 203 scenes and 1,218 looks.

## Evidence and limits

Evidence root: `.local/signal-scenes-20261103`; final frozen runtime
`candidate-02`, previous `candidate-01`. Before/final source archives, identities,
receipts and the prior target references are retained independently of Git.
Every other scene directory and runtime script is compared against before hashes;
every non-target reference image is also checked for exact preservation.

The focused GPU review passes **46 checks**: recorded-input rendering, finite
retained states, exact pause, independent bass/mid/treble causality before common
colour finishing, opposite column travel, retention of old/new captures, row-wise
progressive acquisition, exact mosaic holds followed by refresh, sparse continuous
island travel, diagonal gaps and five targeted dissolves. Diagnostic source
switches prove retained image content rather than inferring memory from movement.
Confetti covers about 10.5% of the diagnostic image. These are bounded behavioural
checks, not long-passage, display performance or musical listening qualification.

Five motion sheets were inspected. The second candidate adds slow acquisition
plane drift to Raster/Mosaic so their memory remains visible with a static source.
The first review invocation stopped on an unsupported fixture control; the next
reported three diagnostic failures. Those assertions incorrectly included the
fitted image's black border, used an overly large aggregate change threshold and
used the opposite pixel Y origin for diagonal seams. Corrected diagnostics and
the final report are retained alongside the failures; no failures were discarded.

The comparison movie is a 12-second, 1,440×632/30 stereo six-panel movie with
360 frames: `media/Alloy-Signal-Scenes-Preview.mp4` under the evidence root.
All panels use the same image and exact recorded PCM at the same musical times;
the sixth is approved, unchanged Broken Spectrum. It is labelled recorded-input
offscreen rendering. The 30 current look references and encoded samples are
visually inspected; complete decode, stream counts and source hashes are retained.

One serial, below-normal build and **50 focused native visual checks passed**
through `verification.run_checks`, retaining artifacts with stress disabled.
Executable SHA-256 remains
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed assets identify this candidate. No full gate, stress, native-window rerun
or listening acceptance. No production app, session, music, analysis/cache,
launcher or immutable release bundle changes; no commit/push or public release.
Jay approved all five with “all good” on 3 November 2026. Preserve these accepted
scenes in subsequent work. Approval and updated continuity notes are retained
separately under the evidence root; the original verification receipt/source
archive is unchanged. Not activated.

The existing repository GPU renderer and same-input preview workflow remain the
appropriate method. The requested skills.sh check did not justify another tool
or installation; Impeccable's scoped refinement and bounded visual review
principles were used. Its web-specific mechanical detector does not apply to
these GLSL scene assets.

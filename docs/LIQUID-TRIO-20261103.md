> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Liquid trio — 3 November 2026

Jay requested three distinct liquid scenes, with the same image/audio comparison,
preserving Mercurial Echo and every approved scene. Signal scenes follow after
this preview has been judged; they have not been changed.

| Scene | Shape and musical response |
| --- | --- |
| Satin Vortex | A logarithmic spiral recedes into one clear dark eye. Bass changes wall depth and eye size, mids accumulate image circulation/twist, treble highlights the satin folds. |
| Pressure Garden | Four fixed, separated impact sites disturb one continuous reflecting image. Bass injects retained expanding pressure waves, mids vary alternating source pressure/ring accents, treble adds small waves and glints. Slow ambient rings keep the outward direction visible between musical events. |
| Oilprint | Broad flat thin-film interference over a gently shearing image. Bass shifts film thickness, mids shear image/colour channels, treble shimmers along fine interference fringes. |

These are authored image/relief effects, not a new physical fluid engine. The three
main shaders expand `visuals/original/shared/liquid-instruments.glsl`, with
`LIQUID_KIND` 0/1/2. Pressure Garden also changes its own retained height/velocity
solver: excitation is local to its four displayed sites and the wave release is
less damped. Its source image is not repeated in four tiles. The existing pass
graphs, state contracts, shader runtime, controllers and catalogue keys stay
unchanged. Defaults and Signature response controls are updated; the other named
looks keep their explicit controls. There remain 203 scenes and 1,218 looks.

## Evidence and limits

Evidence root: `.local/liquid-trio-20261103`; frozen candidate `candidate-03`,
previous `candidate-02`. Before/final source archives and exact identities are
retained independently of Git. Only seven existing scene files changed: the three
main shaders/descriptors and Pressure Garden's `liquid.glsl`. Every other scene
directory and runtime module is compared against the before hashes, including
Mercurial Echo (`liquid-memory`), all five original approved image instruments,
and the later approved city, feedback, stage and glass refinements.

The final focused GPU review passes **36 checks**: recorded-input rendering,
finite retained state, exact pause, independent bass/mid/treble causality in the
scene output before the common colour finish, the vortex eye, four local pressure
injections, propagation after a single burst ends, and three targeted dissolves
(including Oilprint/Mercurial Echo). The mean pressure distance from the nearest
source grows from about .041 to .186 scene units between the sampled early/late
states; ambient forcing is subtracted. These are bounded behavioural checks,
not physical fluid validation or musical listening acceptance.

All three motion sheets are visually inspected. The first draft exposed a weak
Pressure Garden silhouette and too much Oilprint image bending; both were refined.
An initial eye contrast check failed (.0065 centre luminance versus .0401 wall);
the final eye is .0026 against the same wall. Earlier attempts and failed reports
are retained. The first invocation used the system Python without OpenGL; the
repository's bundled visual Python was used for actual rendering.

The final 18 look references and encoded movie samples are reviewed before
handoff. The comparison is a 12-second, 1,440×316/30 stereo movie with 360 frames:
`.local/liquid-trio-20261103/media/Alloy-Liquid-Trio-Preview.mp4`. All three columns
use the same image and exact same recorded PCM at the same musical times. It is
labelled offscreen rendering; no native-window capture or listening acceptance is
implied. Complete decode, stream/frame counts and source/asset hashes are retained.

One serial, below-normal build and **50 focused native visual checks passed**.
The executable SHA-256 remains
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
the asset identities distinguish this candidate. Native checks used
`verification.run_checks` with artifact retention enabled and stress disabled.
Full regression, stress, performance and native-window
delivery are not repeated for this asset-only change. No production application,
session, music, analysis/cache, launcher or immutable release bundle changes;
no commit/push or public release. Jay approved all three with “All good.” on
3 November 2026. Preserve these accepted scenes in subsequent work. Approval and
updated continuity documents are retained separately under the evidence root;
the original verification receipt/source archive is unchanged. Not activated.

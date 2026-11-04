# Landscape comparison — 3 November 2026

Jay requested comparing Opaline Dunes and Lucid Escarpment against approved
Lucent Ranges, preserving all approved scenes. **Every scene, shader, default,
preset, reference image, runtime/native source and launcher remains unchanged.**
The comparison supports refining the two siblings while keeping Lucent Ranges
(`terrain-prism`) exact. Jay accepted the comparison with “All good. next?” on
3 November 2026. No refinement has started; that two-scene implementation remains
the next recommended batch. Feedback is retained separately under the evidence
root without changing the original comparison receipt/source archive.

## Findings

| Scene | Observed with the same image, palette and music | Disposition |
| --- | --- | --- |
| Lucent Ranges | Clear faceted image terrain with bright triangular edges, an open horizon and forward travel. | Preserve the approved anchor exactly. |
| Opaline Dunes | The additional rolling height wave is real, but the same triangular mesh and camera path dominate. With the logo it remains particularly close to Ranges; the opaque chart exposes somewhat larger rolling ridges but they still read as hard faceted terrain. | Highest priority of the two: smooth long lateral dune ridges, lighter mesh-edge emphasis and legible wind travel. |
| Lucid Escarpment | Its offset position and oblique heading produce visibly different parallax. However, both inputs still show a low triangulated landscape rather than a dominant cliff face. | Refine the landform into a steep wall with a clear silhouette and a camera travelling visibly along it; retain useful oblique parallax. |

**Recommendation: a two-scene refinement**, Dunes and Escarpment, with different
terrain silhouettes and travel. More palette differences would leave the main
overlap in place. These are bounded artistic judgements from two images and one
passage, not universal duplication scores.

The source supports the observed overlap: all three use the same piecewise
triangular interpolation, mesh scale, bright edge material and image-height
projection. Dunes adds a broad sinusoidal height wave with a mid-driven phase;
its camera formulas match Ranges. Escarpment adds a clamped world-X ramp, fixes
the camera's world-X position at -2 and looks obliquely across it while advancing
along world Z. That oblique view does give lateral screen parallax; a fixed
world-X position does not imply stationary imagery. The added landform still
does not read as a strong vertical wall in these samples.

## Evidence and limits

Evidence root: `.local/landscape-comparison-20261103`; frozen `runtime` includes
all approved work through the pigment trio. Independent source ZIP, before/final
identities, input hashes, source-derived camera formulas/quiet poses and the
comparison report are retained outside Git.

Six serial 384×216/30 jobs each render **24 seconds**: three scenes with the
existing logo, then the opaque chart. All columns use the same source and exact
recorded PCM/musical times. Shared controls have matching current defaults except
palette: a temporary **Electric neon (1/7)** request override matches the approved
Ranges palette across all three. Extra Ranges controls retain their defaults.
Nothing is saved or applied to an app/session or scene descriptor.

The movie is 24 seconds, 1,152×520/30, 720 frames, stereo:
`media/Alloy-Landscape-Comparison.mp4` under the evidence root. Logo is above,
opaque chart below. Complete decode and stream/frame counts passed. The six
motion sheets sample 2.00, 8.00, 16.00 and 23.97 seconds; all were inspected,
along with encoded frames at 4, 12 and 22 seconds.

Three additional **8-second silent camera probes** use the opaque image, fixed
terrain and main-pass pixels before common colour finishing. Image-height targets
remain exactly equal, and the inspected 0/4/8-second strips show genuine camera
travel in all three. This isolates movement from audio deformation and colour
cycling. Source-derived poses show about 12.7 world units of Z travel over the
24-second interval at the common motion setting; this is not a measured optical
flow or a perceptual similarity score.

**41 bounded checks passed** for render completion, finite image-height/main
targets, exact pause, fixed-height silent camera movement and movie structure/
stereo audio. The six music jobs render 4,320 frames; three silent jobs add 291.
Raw main-pass pair differences are supporting measurements only and did not
determine the recommendation. No independent musical-band, all-look, all-image,
longer-show, native-window, device, listening or performance qualification is
implied. No build, native suite, full gate or stress run was needed for this
read-only comparison. Rendering is below-normal priority with one scene job at
a time and single-threaded encoders.

Before/final identity checks confirm all visual assets/references, runtime Python,
native source, CMake and launcher remain byte-for-byte exact, explicitly including
Lucent Ranges and every approved scene. No production app/session/music/data/cache,
launcher/release bundle, commit/push or public release changes. Only this record,
the audit follow-up and continuity notes are updated. The existing serial renderer
and same-input workflow already selected after the skills.sh method check remain
suitable; no additional plugin, installation or parallel review was needed.

# Original scene refinement plan

**Goal** Give Originals smooth movement between kicks, clear sprite colour and
smooth surfaces where the scene calls for them. Keep each scene's silhouette,
character and useful range of looks.

**Approach** Prove the changes on Chromatic Ravine, Opal Reactor, Aether Nuclei
and Liquid Torus. Use shared rendering and audio controls where the cause is
shared. Roll them out by scene family after judging the four in motion.

## What the current build does

The baseline uses the selected nested-relief bundle `0301291dbb2f`. The
[local captures](../.local/scene-refine-1105/baseline.png),
[measurements](../.local/scene-refine-1105/report.json) and
[probe](../.local/scene-refine-1105/probe.py) isolate four behaviours.

| Symptom | What we observed | Where it comes from |
| --- | --- | --- |
| Large moves on bass | A synthetic 65 Hz bass note with no percussion drove `bassHit` to 1. With a steady bass window and the scene clock held still, changing only the grid phase changed all three sculpture renders. | `Engine.cpp` feeds the programme mix to `SceneLane.cpp`. `original_scene_player.py` measures the 30–220 Hz band. `AudioController` in `original_visual_gl.py` treats positive band changes as hits. Sculpture cameras also synthesise an impact from grid phase and bass level. Liquid Torus steps its orbit every two beats. |
| Persistent purple | All four scenes were purple even without a sprite. Neutral artwork still turned purple. Bypassing the palette removed the purple cast from the test frames while leaving the input to the finishing pass identical. | The four scene defaults apply a full-strength cyan/magenta/violet remap. The shared finish grades the opaque frame, including native material, dark areas and background. Dark input and limited hue variation can keep it in the same palette range. |
| Faint sprite pixels still affect surfaces | An alpha-12 purple matte was pixel-identical to a clean sprite on all four. Alpha 24 changed all four, especially Ravine. A neutral matte at the same coverage also visibly changed Ravine. | `visual_media.py` already rejects alpha codes 0–12 before premultiplied filtering. Above that threshold, coverage and pigment still feed material and image-derived height. Material and geometry need different sensitivity to faint artwork. |
| Coarse or broken surfaces | At 720p, Ravine still showed planar triangles and thick lattice lines. Turning off image influence smoothed some contours on Opal Reactor and Liquid Torus. Some steps remained, especially on Aether Nuclei. | Ravine deliberately interpolates planar triangles and lights their edges. Sculptures also displace the traced shape from image luminance with Relief Depth Off. Spherical wrapping, sampled gradients, coarse hit tolerance, clipped repeat cells and banded shading are separate candidates to isolate. |

The observations are from synthetic PCM and a neutral sprite. They establish the
shared palette and motion faults on the selected package. The contribution of
each remaining sculpture artifact and the effect on Jay's own sprites still
need direct comparison.

## 1. Fix the colour and coverage on the four scenes

- [x] In `scripts/original_visual_gl.py`, make zero palette amount bypass all
  palette processing, including the rotating hue mode. Give the four
  `visuals/original/scenes/<slug>/scene.json` defaults a restrained grade with
  distinct material colours. Keep cyan/magenta and other high-saturation choices
  as deliberate looks. Grade native lighting and fog gently. For image-faithful
  looks, apply any strong hue treatment at the material boundary, weighted by
  sprite coverage, before the opaque frame loses that information.
- [x] Keep premultiplied filtering and separate pigment coverage from a smooth
  relief mask. Preserve soft edges in colour. Fade weak coverage out of geometry
  and displacement tint with a soft knee tuned against feathered artwork. For
  sculptural height use the same weighted field for intersections and normals.
  Preserve the underlying material through empty sprite regions. Keep all input
  images read-only. [Source-over compositing reference](https://www.w3.org/TR/compositing-1/#simplealphacompositing).
- [x] Compare the original and candidate at identical positions with no sprite,
  neutral and coloured sprites, an alpha ramp and a legitimate soft edge. Hold
  opaque artwork constant while replacing the hidden matte. Require identical
  frames for the alpha-12 pair, no visible purple wash from the faint matte,
  no visible relief from that matte, and recognisable colour and edges on the
  visible artwork. Verify the selected vivid look still produces vivid colour.

**5 November preview.** The four Signatures use native material colour and
the five sprite-method presets keep the scene palette off. The vivid colour
menu still grades the frame when selected. The candidate uses separate pigment
and relief curves, and faint matte is excluded from exposure metering. Ravine
preserves native terrain under clear sprite pixels. The shared sculpture shader
uses the same filtered height for intersection and normals.

[Matched captures](../.local/refine-colour-1105/comparison.png) and
[52 GPU checks](../.local/refine-colour-1105/candidate-reviewed/report.json)
compare the selected bundle with the isolated candidate at 480×270. The
alpha-24 purple matte changed Ravine by an average 1.96 RGB code values, down
from 11.80. All four alpha-12 comparisons and both zero-palette modes matched
exactly. The alpha-24 matte by itself left Ravine's height buffer and the three
sculpture silhouettes unchanged. Visible artwork still drove relief and
retained colour. The sprite stayed active through the Reactor-to-Torus join
with exact endpoints. The 23 focused Python tests passed. The captured lattice
and clipped cells remain visible and are next in the surface pass. The preview
used synthetic sprites and the isolated renderer. Desktop and in-motion
full-resolution acceptance remain part of the later checkpoint.

## 2. Rework the surfaces by family

- [x] **Chromatic Ravine.** Keep the valley and drifting strata. Replace the
  default piecewise-planar height interpolation in `main.glsl` with a continuous
  height field and consistent slopes. Soften the centre ridge and sample the
  prefiltered `height.glsl` map without abrupt seams across repeats. Make the
  triangle grid a restrained, antialiased overlay in the lattice look. Compare
  fixed crops of the skyline and foreground during a slow camera pass.
- [x] **Opal Reactor and Liquid Torus.** Keep the fused opal mass and the ring.
  Separate albedo from geometric displacement and use filtered, seam-aware
  relief for the smooth defaults. Inspect the base silhouette, displacement,
  normal field and final highlights in that order. If hit stepping remains
  visible, refine the intersection near the hit and compare normal sample radii
  before raising the overall ray-march cost. Keep the ring opening readable
  during its strongest kick.
- [x] **Aether Nuclei.** Check the repeated-cell and enclosing-sphere boundaries
  with flat pigment and then with the sprite. Compare continuous neighbouring
  fields or a small explicit cluster where the current cell clipping creates
  unintended cuts. Keep the impression of separate nuclei and make the
  highlights flow around each body.
- [x] Review silhouette, specular shimmer and sprite legibility at 720p and
  1080p in motion, including close-ups. Compare the shared sculpture program's
  cold compile and steady render times against the same baseline. Check an
  Opal-to-Torus field morph with the sprite held across the join. Expand
  filtering or tracing cost only when the pixels justify it.

**5 November preview.** The Ravine height map meets the native terrain with
zero slope at the tile edges. The Lattice preset draws a thin antialiased
overlay over the same valley. The shared sculpture program blends the new
surfaces with the existing compositions. Reactor, Nuclei and Torus leave the
base shape and normals intact when applying pigment with Relief Depth Off.
Optional relief uses a broader footprint, fades at the longitude seam and
poles, and shares the field for tracing and normals. Nuclei use 22 explicit
centres. Torus's camera keeps the whole ring visible at six synthetic transients
that saturated the existing bass response. Its enclosed opening stayed above
2,100 pixels in the 720×405 fixture.

[Matched motion](../.local/refine-surfaces-1105/motion-comparison.gif),
[surface report](../.local/refine-surfaces-1105/candidate-accepted/report.json)
and [colour regression](../.local/refine-surfaces-1105/candidate-colour-final/report.json)
retain 20 surface checks and 52 repeated colour and coverage checks. Inspected
matched 720p and 1080p frames and normal close-ups. The pinned Opal-to-Torus
morph matched the outgoing and incoming pixels at the endpoints. A fresh
shared program source signature built in 688 ms versus 279 ms for the
baseline. The warm 1080p render and readback medians were 7.5–12.8 ms on the
candidate and 9.1–15.5 ms on the baseline across the four scenes. These
short samples leave sustained desktop cadence for the show review.

## 3. Reserve large movement for confirmed kicks

- [x] Separate a timestamped `kick` event and its continuous `kickImpact` from
  bass, mid and treble envelopes. On mixed audio, measure the attack and short
  decay of low-end energy, with an adaptive noise floor, corroborating spectral
  changes and a minimum retrigger interval. A synth pluck can still resemble a
  kick. Use the beat grid as weak context and let a strong syncopated hit through.
  Low-confidence events leave the big movement idle.
- [x] Detect from successive audio windows on the audio sample clock, independent
  of renderer cadence. Keep the callback bounded and hand work off to the visual
  worker. Deliver event times so a dropped frame can evaluate the current part
  of the same envelope without replaying the attack. Handle silence, pause,
  seek, loop and revision changes explicitly. On a seek or source-mode switch,
  prime current levels before accepting a new event.
- [x] Route major camera surges, rotations and silhouette changes to one eased
  attack and recovery per confirmed kick. Try a 35–55 ms rise and 180–300 ms
  recovery as starting values. Smooth and cap ordinary audio response so it can
  move light, pigment, ripples and small bends. Replace Torus's stepped orbit
  with continuous travel. Audit each migrated family's `bassHit`, `beatPulse`,
  beat-phase, onset and time-step paths for other large responses. Keep scene
  transitions and long-form travel continuous.
- [x] In `tests/test_original_visual.py` and a focused renderer capture, use
  labelled synthetic kicks, bass plucks and sustained bass, snare/hat-only,
  kick plus bass, missing grid kicks and off-grid kicks. Require one large event
  per labelled fixture kick and zero on bass-only, snare/hat-only and silence.
  Measure camera/shape changes separately from colour. Play the same passages
  at two update cadences and inspect the output during a breakdown and drop.

**5 November preview.** The callback attaches sample-clock and generation
stamps to the preallocated visual feed. The native visual worker measures
stereo energy in three bands in 10 ms windows. It checks the low attack, the
downward spectral shift and the low tail against an adaptive floor before
confirming an event. Confirmation takes about 50 ms. Grid proximity changes
strength by up to six percent. The event clock uses a 45 ms rise and a
225 ms recovery. The renderer evaluates the elapsed time when it receives
a frame request. Untimed monitor audio, gaps, loopbacks and new generations
clear the detector and prime the next programme window. A paused frame
holds its pose.

The four preview scenes use the same impact. Ordinary bass still produces
small bends and the mid and treble paths support modest drift and shading.
Torus uses continuous orbit time. Nuclei use wider framing during peaks.
The other five shared sculpture compositions retain their prior response
until the family rollout.

[Moving preview with fixture audio](../.local/refine-kick-1105/renderer-reviewed/breakdown-and-drop.mp4),
[native checks](../.local/refine-kick-1105/native-final/checks.txt) and
[renderer checks](../.local/refine-kick-1105/renderer-reviewed/report.json)
record three confirmed kicks and zero events during the bass and percussion
breakdown. The muted native path delivered the syncopated Reactor impact
74 ms after the labelled onset. At matching 30/60 Hz timestamps the four
scenes produced identical geometry. The 17 native, 17 Python and 18 GPU
checks are scoped to synthetic fixtures. The first mixed fixture exposed
a bass-pluck false event and remains in the local evidence. The tighter
spectral fall requirement clears that fixture. Real music can still contain
kick-shaped synth transients or kicks the mix detector misses.

## 4. Compare the existing stems before committing to a stem-driven mode

- [ ] Reuse [Mixworks' offline stem workflow](STEM-SEPARATION.md) on a small
  set of tracks with strong bass, soft kicks, syncopation and kick-free breaks.
  Annotate the audible kicks and compare timestamped predictions from the full
  mix and the aligned drums stem. Report missed hits, false hits and timing
  error for both. HTDemucs provides drums, bass, vocals and other. The drum stem
  still contains snares and hats, so run the kick detector on it too.
  [Demucs model documentation](https://github.com/facebookresearch/demucs/blob/main/docs/training.md#hybrid-transformer-demucs).
- [ ] If stems improve visible sync, cache only compact source-time kick events
  and envelopes keyed to validated stem identity and feature version. Use the
  drums for kick impacts and finer percussion detail, bass for slow flex and
  optional vocal or instrument envelopes for modest material motion. Use the
  existing clip time mapping and audible contribution through fades and mute or
  solo. For the first integration use the programme detector when reversed,
  outside supported time maps or when stems are missing or stale. Prefer one
  combined event when two audible kicks coincide in a crossfade. Compare timing
  on trims, slips, tempo ramps, loops, blends and seeks against the audio heard.
  Keep playback and preparation independent of separation jobs. Report residual
  leakage and the limits of projecting source features through parent effects.

## 5. Roll out and judge an actual show

- [ ] Make a matched short preview of the four scenes through a breakdown and
  drop, including a sculpture morph and a join across families. Judge the moving
  output with the soundtrack. The large moves should read as individual kicks,
  with calm travel in the gaps, distinct scene colours and stable close-ups.
- [ ] Migrate the rest of the sculptures and landscapes first, then solid
  instruments, liquid surfaces and layered reliefs. Review particles, ribbons
  and signal scenes for large non-kick motions without flattening their
  characteristic detail. Keep intentional facets and grids where a scene's
  identity depends on them. Inventory the shared shader, affected scene/look
  and verification status for each batch. MilkDrop is outside this Originals
  pass.
- [ ] Batch focused Python and GPU checks at the four-scene checkpoint and
  subsequent family checkpoints. Use `scripts/verify_scene_looks.py`,
  `scripts/verify_nested_sprites.py` and the relevant behaviour probes, then
  check the actual desktop with a fresh bridge session under
  [project testing policy](TESTING.md). Check exact pause, seek, pinned and
  rotating sprites, morph endpoints and fullscreen cadence. Retain bundle
  identity, moving comparisons and measured frame times. Fold the broader
  show review into [W15](../FINISHING-PLAN.md).

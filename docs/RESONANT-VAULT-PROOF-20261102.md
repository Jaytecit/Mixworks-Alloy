# Resonant Vault — first visual direction preview

2 November 2026. Follows [the visual direction](VISUAL-DIRECTION.md).
Jay selected **“Yes—refine this direction”** after the early moving preview.
This is approval to develop the direction, not acceptance of the final scene,
100-scene repeatability, transitions or distribution readiness.

**Follow-up:** Jay said the only change he wanted was to start the pulse at the
far opening and ripple forward offscreen. This is implemented in
`.local/vdir-forward/candidate`; review `.local/vdir-forward/preview/preview.mp4`.
The pressure field now moves from its far boundary toward zero, with outflow
rather than reflection; its geometry mapping spans the full 18-unit visible
depth. Other scene properties are unchanged. Thirteen bounded probes pass,
including far-to-near propagation and no retained pulse after exit. The actual
12-second shader capture at 720p/60 encoded fps passes five pause/finite/bound
checks; sampled pixels inspected. Renderer median6.66 ms, p958.35 ms, excluding
encoder/backpressure. No native rerun or installation for this narrow correction;
all native delivery/save/reopen figures below describe the previous candidate.

## Visible outcome

One continuous folded vault carries the supplied artwork across substantial
lit surfaces. Its material moves independently inside the geometry. Bass attacks
open local folds from the far opening toward the viewer with a decaying release. Midrange
changes material transport and the curved axis; treble replenishes pigment and
adds local ridge highlights. Continuous movement remains without attacks.
No authored section cues, global beat-sine drive or new detector are involved.

After Jay's response, candidate-06 reduced material smearing, sharpened source
contours, increased directional lighting contrast and strengthened the travelling
bass deformation. The earlier candidates and the rejected Ribbon Loom experiment
remain preserved. This scene is not installed into the production catalogue.

The **open canopy** control changes the geometry from a five-lobed vault into
opposing pleated surfaces with a broad opening. It reuses the same material,
pressure, lighting, finishing and lifecycle implementation. The red/gold alternate
Martooni artwork also renders successfully. Both supplied images contain alpha;
the alternate must not be described as opaque.

## Review files

All paths below are repository-relative local evidence, excluded from Git.

| Item | Path |
| --- | --- |
| Refined 36-second recording with soundtrack | `.local/vdir/refined-musical/preview.mp4` |
| Structural variation, eight seconds | `.local/vdir/refined-canopy/preview.mp4` |
| Alternate artwork, eight seconds | `.local/vdir/refined-alternate/preview.mp4` |
| Existing Vein Current, matched first twelve seconds | `.local/vdir/benchmark-vein-verified/preview.mp4` |
| Side-by-side, Vault left / Vein Current right | `.local/vdir/comparison.mp4` |
| Early preview shown before Jay's response | `.local/vdir/early-03/preview.mp4` |
| Final frozen shader/runtime candidate | `.local/vdir/candidate-06/` |
| Native refined workspace and evidence | `.local/vd3/` |
| Focused mechanism probes | `.local/vdir/focused/checks.json` |

Music is the previously authorised June Montana *I Need Your Love* copy,
source seconds **112–148**, identity
`9e9ac5390a030224ccf48bd15916071d2d81779efd074ae2b6a748649e7b7e73`.
Primary artwork identity is
`52be27bc0ac94ab4778812e7d0398659a5b6c92e4532b06391edf9da5e92227e`;
alternate is `3f716e10ad280e518995a33dac11a446133ad8e186ba9c48137d3e1bfeb5a593`.
The excerpt covers the previously measured fuller passage near 112 seconds and
reduced energy near 128–136. The inherited 121.98 BPM pulse estimate is not
reviewed downbeat truth; this scene's consequences are driven by PCM bands.

Direct pixel inspection compared the new scene with the supplied Sinusoidal
Warp / Transparent Isoslices screenshots and the matched-input Vein Current
capture. The vault has more continuous spatial depth; Vein Current retains
sharper fine contour breakup. Material softness and long-term compositional
development remain useful judgement points. These comparisons are not a parity
claim. Wave Lens did not consume either transparent image and is retained as an
unsuitable comparison attempt, not used to lower the visual benchmark.

## Recipe and resource contract

Source: `visuals/original/scenes/proof-resonant-vault/`.

- `material.glsl`: transported RGB pigment at 75% output dimensions, two RGBA16F
  buffers; reflected boundaries, nested resampling and bounded source refresh.
- `impulse.glsl`: 128×1 two-buffer pressure transport; bass injection, downstream
  movement, decay and smoothing. This is a one-dimensional transport field,
  not a fluid engine or physical acoustic model.
- `main.glsl`: bounded 128-step ray traversal of an original folded wall;
  interchangeable vault/canopy distance functions consume the same material and
  pressure. Near-grazing rays use their converged surface estimate to avoid holes.
- Existing owned bloom/finish passes provide restrained highlights and tone mapping.

At 720p the targets occupy **23,963,648 bytes**, with 60 Hz state stepping.
The clip owns state; seek/revision/generation resets it. Past feedback history
is not reconstructed after seeking. This demonstrates reuse within one scene,
not a general nested-sprite editor or proof of 100 distinct quality scenes.

## Verification and limitations

Eleven bounded GPU mechanism/lifecycle probes passed: pressure propagation and
release, no false pressure from a beat clock alone, separate bass/treble response,
continuing silent motion, exact pause, reset on backward seek, alternate material,
structural variation and released scene instances. These establish mechanisms,
not convincing musical response by themselves.

Final recordings compiled every shader and passed finite-state, bounded-state
and pause checks. The 36-second capture is 1280×720, 60 encoded fps; renderer
median **7.94 ms**, p95 **12.39 ms**, excluding encoding/backpressure. These are
offline-renderer measurements. Native delivery is recorded separately in the
workspace's `evidence/summary.json`: 2,154 window swaps over 35.92 seconds,
**59.97/s**, median gap17 ms, p9519 ms, maximum23 ms, at 1280×720 internal
resolution. The preview was partly occluded; these are not physical scanout
measurements. Saved session `direction-final-verified.mixworks` was produced
after successful reopen and pixel inspection, then the owned app closed normally.
The native check uses the trusted executable
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02` with the
frozen original runtime; no native source or executable rebuild was needed.

Native playback is monitor-muted. The review movies contain the soundtrack;
physical audio latency, listening quality and final moving-with-sound artistic
acceptance remain for Jay. No full regression, stress, endurance, hardware test,
production activation, app-data reset or library scan was performed.

Retained failures: the first capture omitted the cutout material; two native
fixture attempts lacked required automation fields/generated-point consistency;
the initial scene filename failed native catalogue discovery (renamed to
`main.glsl`, rendering unchanged); a capture helper wrongly applied this scene's
0–1 state bound to Vein Current's signed velocity state. The helper now restricts
that bound to this scene; the corrected benchmark run passes. Original failures
remain available. No shared runtime or native validation was weakened.

The encoded excerpt in the owned native workspace is deliberately retained for
saved playback, as are requested review videos. Original music, production
analysis, manual references, running sessions and launcher remain untouched.

Next: judge the refined recording's attack/release, material clarity and quieter
passage; continue this scene if needed. Establish final acceptance before a
second distinct scene or cross-scene joins.

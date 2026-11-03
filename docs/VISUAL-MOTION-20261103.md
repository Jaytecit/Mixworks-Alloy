# Motion, complete imagery and colour — 3 November 2026

Jay's viewing feedback supersedes earlier visual acceptance assumptions. Most
scenes still appeared insufficiently driven by musical movement; brightness and
colour alone are inadequate. Particle paths must respond, not only particle size.
Prefer more solid/geometric scenes in the direction of Beats in 4D and fewer
particle scenes. Preserve the retained shortlist; this is a change in emphasis,
not permission to retire more entries.

Long full-track transitions should directly blend the live scenes, unless a
demonstrated technique brings their appearances together. Whole-image compositions
should hold a complete, readable image, pump it to the beat and apply different
scene motion effects. Mosaic Kiln should repeat the whole image on each tile.
Fragmented images remain appropriate for some scenes, with occasional coherent
particle reassembly. Quieter scenes benefit particularly from kaleidoscope effects.
Much stronger image/scene recolouring is wanted, both beat-driven and cycling.

## Implementation checkpoint

- Dissolve and zoom now use the real live compositor rather than the generative
  fragment handoff. Exact overlap seconds travel with native render requests;
  radial/sweep handoffs longer than four seconds become direct dissolves. Missing
  duration is conservative. Cuts remain explicit cuts. Automatic overlaps longer
  than four seconds or eight beats select dissolve, including filtered music joins.
- Automatic shows allow particle entries in at most one slot in five when other
  scene families are available. All retained entries remain manually available.
- All 56 relief compositions have stronger band-driven geometry: depth expansion,
  shearing and travelling bends, plus larger accumulated band phases. The 28 quieter
  woven/porcelain/contour/ribbon/branch/curtain/paper compositions default to six-way
  reflection in Signature and Silk; other looks and view choices remain selectable.
- All 28 active particle compositions use integrated musical phases for their
  trajectory clock and band-driven vector fields. A 16-beat cycle eases fragments
  into a complete source chart, holds it over beats 12–14 and releases it. Position,
  rotation, size and fragment borders cooperate in reassembly. Optional display
  reflection is relaxed during the recall; explicitly selected shared kaleidoscope
  modes still apply to the resulting complete image.
- Mosaic family's native material repeats a contained full image per tile rather
  than nested image pieces. Optional image-method effects remain selectable.
- Three new solid image stages: Pulse Gallery (depth frames), Orbit Theatre
  (elliptical instruments) and Resonant Iris (six-lobed rings). Each carries one
  complete fitted image with beat scale and a different motion treatment. These
  are three topology compositions sharing a renderer, not independent engines.
- Shared finishing applies continuous and band-driven hue rotation and tinting to
  actual scene/image pixels. Pause remains deterministic. This complements motion;
  it is not evidence that movement alone is now artistically sufficient.

Evidence and independent source checkpoints: `.local/motion-20261103`.
Production sessions, music, caches, retained/retired registries and launcher were
not modified during this isolated preview.

## Verified preview

Frozen runtime `candidate-04`; native executable SHA-256
`e8384e406ba4083452d237003bfde7289a202d91d64c893571c01c12829b5cbf`.
`verified-source.zip`, runtime identities, `completion.json` and failed-candidate
provenance are retained independently of Git. Initial shader/local-variable and
test-fixture problems are preserved, with separate successful corrections.

- 14 runtime unit tests passed using the existing bundled interpreter.
- 141 GPU checks include one compile/render per active scene, exact live blend
  comparisons for long dissolve/radial/sweep/reverse-sweep requests, a short
  generative reveal, particle path displacement with/without music and pause.
  The three new shaders subsequently received a control-wiring correction;
  their final six-look/method/finite/pause matrix passes 12/0. Mosaic's final
  correction passes 4/0; the other eight representative matrix entries passed.
- `motion-preview-final.mp4`: 56 seconds, seven compositions, actual soundtrack,
  640x360/30 fps. `long-transition-preview/tour.mp4`: 24 seconds with two six-second
  overlaps. The latter checks actual output against both live scene textures.
  Sampled moving pixels and native screenshots were inspected. These are offline
  soundtrack renders, not recordings of native delivery or listening acceptance.
- 45/0 native checks through `verification.run_checks`, stress off and no retention
  errors. Includes 12-track automatic scheduling, long filtered-music joins,
  particle selection frequency, presets/methods/Undo/save and actual mixed-PCM
  rendering of Beats in 4D, particle imagery, Mosaic Kiln and Pulse Gallery.
- 42 reference images refreshed for seven examples, including all three new
  scenes' six looks. The other static references remain from the prior checkpoint.
- Bridge workspace `.local/mot1`: protocol/root/PID/capabilities checked; the
  56-second owned review fixture opened, saved, captured and closed gracefully.
  Transport remained stopped; monitor mute was off. The owned excerpt and images
  are intentionally retained for reopening. The review's authored sweep choices
  exercise the duration fallback; the lane retains the requested effect label,
  while rendering uses dissolve. Newly automatic long joins store dissolve.

No full regression, stress, sustained performance, hardware, listening or public
release clearance is implied. The new artistic direction still needs Jay's judgement.

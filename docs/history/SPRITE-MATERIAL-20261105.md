# Scene-local sprite material implementation plan

Goal: transparent artwork is data for a scene's surface/geometry, never an
independent screen overlay or a transition image source. Relief, domain warping
and displacement colour are independent methods, usable alone or in combination.
Keep existing colour effects. Original artwork and the running user session stay
untouched. This supersedes the broad recurrent sprite treatment recorded earlier.

Architecture: explicit scene-local sprite capability; no automatic MilkDrop
sprite stage. Existing particle reconstruction keeps direct source-atlas input.
Molten Contours provides a lit bevelled relief plate, warped image coordinates,
and optional displacement-driven neon. Transitions borrow only rendered scene
output and typed simulation state; they never choose/upload fresh sprites.

Implementation and verification (same task, no delegation):

- [x] Catalogue/runtime: mark actual geometry/material scenes as sprite consumers;
  remove shared SpriteFeedback injection; decode premultiplied source once and
  unpremultiply only at straight-colour sampling boundaries.
  Files: scripts/visual_catalogue.py, original_scene_player.py,
  original_visual_gl.py, milkdrop_scene.py. Test unsupported scenes/MilkDrop have
  no sprite selection, scene-local support receives a source, transition routes
  do not add one, pause and reset retain their defined semantics.
- [x] Molten Contours: implement a bevelled heightfield with smoothed luminance,
  column-varying downward warp and displacement colour. Add separate normalized
  reliefDepth, meltWarp and displacementColour controls plus individual presets;
  zero remains off during Auto Arrange. Files: scene.json/main.glsl,
  scripts/visual_catalogue.py, Source/VisualScenes.cpp/SceneLane.cpp as required.
- [x] Focused proof: each method alone changes only its intended treatment,
  combinations work, clear image regions contribute no fringe/height, artwork
  hashes remain exact, and no sprite is added by Original/MilkDrop joins. Inspect
  live GPU captures and a short owned animation before calling the preview ready.
  Tests use local disposable fixtures, serial low-priority runs and no audio device.
- [x] Build once after coherent native edits; run only affected native/Python/GPU
  checks. Stage an immutable next-launch bundle, preserve failures/source hashes,
  update HANDOVER. No running user-app restart, cache reset or public release.

Initial investigation: candidate31 is running. The pictured va ultramix2-72 to
Prism Datastream red wash was reproduced both with and without imagery; some red
is legitimate scene colour processing. Jay clarified: allow colour effects but
remove fringes; sprites must be nested into a scene, not appear in every join.
The shared recurrent sprite layer is therefore being replaced, not tuned again
with a higher alpha cutoff. Evidence: .local/red-transition-20261105.


Completed on candidate33 (`b608d839707a`). Evidence:
`.local/red-transition-20261105`; native source checkpoint
`.local/fw04/candidate-33-nested-sprites-source.zip`. 206 native,34 Python and117
GPU checks passed;491 references refreshed. See HANDOVER for scope and limits.
A six-second silent runtime preview demonstrates the methods separately. Originals
and the current app/session are preserved; the launcher changes only the next run.


## Expansion requested 5 November

Expose relief, domain warping and displacement colour independently in the32
existing surface/geometry scenes, in addition to Molten Contours. The shared
sampler handles premultiplied warp and optional colour; each family routes relief
into its own height/distance field. New controls default off in existing looks.
Add separate presets and combination choices; preserve scene identities and no
MilkDrop/transition sprite injection. Verify independent output, absent-source
behaviour, pause and source-noise invariance; inspect representative family
captures and refresh the added preset references. One focused native checkpoint,
then immutable next-launch bundle and handover. Evidence:
`.local/sprite-methods-20261105`.


Expansion completed on candidate34 (`507d7d4ac273`):32 additional scenes/33 total,
160 added method thumbnails. 273 native,35 Python and453 GPU checks pass, with
individual family captures and an eight-second runtime example. Source and local
activation evidence are retained in `.local/sprite-methods-20261105`. Current app
and original images stay untouched. See HANDOVER for limits and retained failures.

# Owned Alloy visual library

The active catalogue is `original-scenes.json`: 203 scenes and six named looks per
scene. It preserves the retained shortlist and does not restore retired entries.
`retained-scenes.json` and `retired-scenes.json` preserve review choices; retired
shader source remains available for reference but is not staged in the active bundle.

See [current visual direction](../../docs/VISUAL-DIRECTION.md),
[scene workflow](../../docs/VISUAL-SCENES.md) and
[current candidate](../../HANDOVER.md). Original compositions are covered by the
root Personal Beta Licence. Dimensional Pulse (formerly Beats in 4D) retains its CC0 notice and author attribution;
third-party material is not relicensed by the application's terms.

The five [image-generator examples](../../docs/VISUAL-FIVE-PREVIEW-20261103.md)
add cellular growth, a source-neighbour flock, image-height terrain, an evolving
liquid surface and persistent region glitches. Palette selection is independent
of scene/view/image method. Active particle assembly is a continuous passage;
transport pause remains exact. The full [eighty-scene collection](../../docs/IMAGE-INSTRUMENTS.md) now adds ten
image-driven instruments; [verification and limits](../../docs/VISUAL-EIGHTY-20261103.md).

## Authoring and staging

The [landscape pair refinement](../../docs/LANDSCAPE-PAIR-20261103.md) gives Dunes
a continuous wind-driven surface and Escarpment a steep wall with along-face
travel. Their main shaders match `shared/dune-instrument.glsl` and
`shared/escarpment-instrument.glsl`. Source-height passes, approved Lucent Ranges
and all other scenes/references remain exact; older landscape recipes must not
overwrite the revised pair.

The [pigment trio refinement](../../docs/PIGMENT-TRIO-20261103.md) gives Wake one
local jet inlet, Press two separated complementary-image fronts and Fault two
sliding banks around an open jagged gap. Four `shared/pigment-trio-*.glsl`
fragments expand into these three scenes' main/vector/pigment shader sets.
Monsoon, Undertow and every other scene/reference remain exact; the older shared
image-flow recipes must not overwrite the revised trio.

The [signal refinement](../../docs/SIGNAL-SCENES-20261103.md) separates opposed
temporal columns, rolling acquisition, held mosaic captures, drifting islands
and diagonal splices. Its five main/memory shader pairs expand
`shared/signal-instruments.glsl` and `shared/signal-memory.glsl`. Broken Spectrum
and all other approved scene directories and references remain exact.

The [liquid trio refinement](../../docs/LIQUID-TRIO-20261103.md) separates a spiral
funnel, four impact sources and a flat thin-film surface. Its three main shaders
expand `shared/liquid-instruments.glsl`; Pressure Garden has its own four-source
retained pressure field. Mercurial Echo and other approved scenes stay exact.

The [stage/glass refinement](../../docs/STAGE-GLASS-20261103.md) gives five scenes
distinct image carriers, shutters and transmitted-image layouts. Their current
recipes are `shared/stage-instruments.glsl` and `shared/glass-instruments.glsl`,
expanded into the five checked-in main shaders; older shared recipes must not
overwrite those revised instances.

The [scene identity refinement](../../docs/SCENE-IDENTITIES-20261103.md) restores
plain defaults to 28 relief scenes and separates the two city and three feedback
instruments. Their `shared/city-instruments.glsl` and `shared/ribbon-instruments.glsl`
recipes expand into their five checked-in `main.glsl` files. The five approved
image instruments and the shared runtime are unchanged.

Scene descriptors declare bounded executable controls, named looks and pass graphs.
Shaders and their checked-in instances are the runtime source. Shared relief and
image-stage templates live in `shared/`; particle authoring templates are in `kernels/`.
When editing a shared template, update only the intended active scene instances and
verify those scenes. Historical generators that restored old catalogues or replaced
curated presets were removed in the [maintenance audit](../../docs/MAINTENANCE-20261103.md).
Do not regenerate the library from old local scripts.

`python scripts/stage_visual_runtime.py --out .local/<fresh-candidate>` freezes only
active scenes and current renderer modules with hashes. It refuses an existing
output. Renderers do not load external scene scripts, extracted helpers or videos.
State textures have declared semantics and belong to a scene instance; colour is
not implicitly reinterpreted as velocity or chemistry. Native runtime delivery uses
bounded mixed PCM and the project clock, not a microphone/capture device.

## Current review tools

Use `scripts/review_image_collection.py` for bounded all-eighty motion/palette
review, `scripts/verify_image_collection.py` for mechanism causality/live blends
and `scripts/render_image_collection_tour.py` for the labelled tour.
Use `scripts/verify_visual_behaviour.py` for authored-fixture live blends, particle
paths and glass imagery, `scripts/verify_scene_looks.py` for selected looks/methods,
`scripts/verify_image_generators.py` for the five generators/palettes/continuous
passage, and runtime/media unit tests. `render_image_generator_preview.py` makes
their bounded soundtrack review. `render_scene_previews.py`,
`render_scene_preset_previews.py` and `render_scene_library_tour.py` create reference
stills or soundtrack tours. Follow [testing policy](../../docs/TESTING.md): serial,
below-normal-priority checks, no automatic stress/full gate. Native workflow checks
use the shared retention harness. Screenshots are not musical/artistic acceptance.

## User media

[Backgrounds](Backgrounds/README.md) and [Sprites](Sprites/README.md) document
image folder roles. Optional videos are local inputs. User artwork and music are excluded
from source commits; the selected application media paths stay in local settings.
Reference thumbnails are generated examples, separate from original user artwork.
Never overwrite original media, retired choices, protected review evidence or a
running production session while preparing a new local bundle.

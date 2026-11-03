# Owned Alloy visual library

The active catalogue is `original-scenes.json`: 123 scenes and six named looks per
scene. It preserves the retained shortlist and does not restore retired entries.
`retained-scenes.json` and `retired-scenes.json` preserve review choices; retired
shader source remains available for reference but is not staged in the active bundle.

See [current visual direction](../../docs/VISUAL-DIRECTION.md),
[scene workflow](../../docs/VISUAL-SCENES.md) and
[current candidate](../../HANDOVER.md). Original compositions are covered by the
root Personal Beta Licence. Beats in 4D retains its CC0 notice and author attribution;
third-party material is not relicensed by the application's terms.

## Authoring and staging

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

Use `scripts/verify_visual_behaviour.py` for authored-fixture live blends, particle
paths and glass imagery, `scripts/verify_scene_looks.py` for selected looks/methods,
and runtime/media unit tests. `render_scene_previews.py`,
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

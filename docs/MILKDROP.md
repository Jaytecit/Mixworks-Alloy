# MilkDrop preset import — first integration

Alloy can import `.milk` presets into a separate local library and render them
using optional libprojectM 4.1.8. The automatic arranger offers Originals, MilkDrop or Both; the default remains
Originals. Both alternates engines before choosing individual scenes, so a small
imported collection is represented even alongside the 203 original scenes. No NestDrop executable, plugin DLL, preset
collection or texture collection is copied into the application package.

## Linked collections and previews

In **Visuals / scenes**, choose **MilkDrop** in the library collection selector,
then **Link MilkDrop folder...**. Choose the collection root (for example a
Milkdrop2 folder containing Presets and Textures, or Presets itself). Alloy indexes
`.milk` files recursively in the background. It writes only an index under
`%LOCALAPPDATA%/Mixworks/MilkDrop-v1/linked`; source presets, JPGs and textures stay
in place. There is no 128-preset limit for linked collections. Identical preset
content is deduplicated; invalid files and presets over 1 MB are skipped.

Existing JPGs beside a preset with the same filename stem are used directly as
library previews. No JPG is copied. Missing or unreadable images fall back to
cached 320x180 previews generated serially using demo audio, while transport is
stopped. Only the current page of 24 presets is queued, and image memory is bounded
to that page. Previews pause during playback, Live mix, clean output or a hidden
browser. A generated still is illustrative, not a compatibility certification.

Originals and MilkDrop have separate searchable library views; the independent
**Scene sources** setting still controls Originals / MilkDrop / Both for automatic
shows. Adding, dragging and saving presets use the existing visual timeline.
Choose the same folder again to rescan after changing collection contents.
An unavailable or changed linked preset reports reconnect/rescan guidance when
loaded. Keep the linked collection and textures available; sessions store content
identities, not portable copies of external assets. MilkDrop 3 formats remain
unsupported. Not every imported shader is compatible with projectM.

## Single-preset import

Build the optional renderer once with `scripts/fetch-projectm.ps1`, then run
`scripts/build.ps1`. Both builds use one worker and below-normal priority.
The projectM/GLEW shared libraries and their notices are staged beside the visual
worker under `original-visualiser-runtime/milkdrop`.

In **Visuals / scenes**, click **Import MilkDrop...**, select a `.milk` file,
select the imported entry, **Add at playhead**, then choose **Live mix** and
play the music. Imported entries have a **MilkDrop:** prefix. Ordinary placement,
trim, removal, Undo and saving the visual timeline use the existing workflow.
Native Alloy looks, image/view methods and palettes do not apply to these presets.

For automatic shows, choose **Scene sources: Originals / MilkDrop / Both** in
**Auto Arrange**, or the matching selector above **Match mix** in the visual
browser. The two controls share the choice in the current application window.
**Keep visual lane** keeps every existing block and disables source selection.
MilkDrop and Both require an imported preset and the optional runtime; missing
imports leave the current music and visuals untouched. Both uses both engines
when there are at least two visual blocks; a single-block show starts with an
original. Your selected background image is used by original scenes. MilkDrop
presets also receive transparent artwork from the configured Sprites folders as
evolving feedback material. Automatically generated imported controls remain
limited to their supported schema. All scene joins use generative feedback.

## Transparent sprites and generative handoffs

The project image picker can pin a transparent sprite across the show. Compatible
Originals map that source into particle pigment, relief, sculpture or instrument
surfaces, landscape height or liquid normals. Automatic sprites rotate every 64
bars in 4/4 and share the same selection across compatible Originals and MilkDrop.
The final bar of each phrase blends the sprite inputs. A pinned sprite overrides
the automatic choice. Changing it while paused refreshes particle pigment at the
current positions.

The patched projectM renderer receives the premultiplied sprite in its feedback
buffer before its own warp and composite passes. Each preset therefore determines
how the material moves, transforms and decays. Presets that suppress previous-frame
colour can also suppress the sprite. Preset files are unchanged. Flat Original
effects without a material input ignore the sprite.
NestDrop's deck-level sprite activation and nested render input are the workflow
reference. [NestDrop user manual](https://nestimmersion.ca/assets/nestdrop_user_manual_V2.pdf)

Sprite uploads discard the lowest twelve PNG alpha codes and filter premultiplied
colour before converting to straight colour at
sampling boundaries. Image detail remains available for colour; sculpture height
and normals use a broader filtered footprint. This prevents almost invisible RGB
noise and sharp texels becoming coloured fringes or jagged surface slopes.
Original artwork is read only. Intentional scene colour effects remain enabled.

The 32 sculpture, solid-instrument, landscape and liquid-surface scenes, plus
Molten Contours, expose **Relief depth**, **Domain warping** and **Displacement
colour** independently in their scene context menus. Each has Off/Gentle/Strong
choices, and individual named presets plus combinations. Relief uses smoothed
image luminance as height; warping drags source coordinates in animated columns;
displacement colour maps the image-derived field into a moving palette. Turning
one method off leaves the other settings intact. Auto Arrange keeps disabled
methods off. The additional scenes keep their native geometry; relief modifies
its height/distance field or optical surface normals. Colour is applied only at
shading, leaving underlying simulation/height data unchanged. Existing looks keep
the three new controls off. Particle scenes retain their own reconstruction
controls. Molten Contours lifts its actual image-derived heightfield on kick
attacks and disperses a volume coloured and shaped by the same source. Set Relief
depth to Off to remove the extrusion and its vapour.

Delayed forward frames retain the live preset and its feedback. Actual seeks,
stops and document changes use the engine's explicit reset generation; a slow
frame alone no longer repeatedly reloads a preset and traps playback in startup.

Joins within the eight implicit sculptures or within the eight solid instruments
trace one changing distance field. The 52 compatible layered reliefs morph across
their 13 profiles by blending height, coverage and material coordinates before
lighting. Both scene control sets contribute through the overlap. A direct change
uses a two-second morph. Sprite material and musical motion stay live throughout.

Other joins still use the feedback renderer. Overlaps seed from the current
outgoing frame, native adapters transfer compatible typed state, and the recovery
after overlap lasts 1.25 seconds. projectM's sprite input does not provide a shared
parameter model for Original/MilkDrop or arbitrary MilkDrop pairs. The worker
delivers the current picture before preparing the upcoming scene. Preparation
still occupies the worker briefly and sustained display cadence needs desktop
verification. Music timing and midpoints come from the arrangement.

Pause holds the state and pixels exactly. Seeking restarts feedback, including
scene-local geometry and handoff histories. A seek directly into a join therefore
does not reconstruct the uninterrupted show. MilkDrop library JPGs/previews
depict the preset itself.

The importer retains the exact file in
`%LOCALAPPDATA%/Mixworks/MilkDrop-v1/<sha256>/preset.milk` with an `import.json`
descriptor. Duplicate content has the same identity. The descriptor records
the source location and read-only texture search paths (the preset folder and
its sibling `Textures` folder, when present). Keep those folders available for
presets that need external textures. The development bridge and native tests
redirect this library under their isolated data roots. Single-file imports are limited to
1 MB per preset and verified against their SHA-256 when discovered.
The library persists independently of sessions; a saved mix refers to its imported
content identity. Moving a session to another machine also requires the imported
library and any external textures. Imports do not change the original files.

## First-version limits

- The input is Alloy's existing bounded mixed PCM feed: mono from the louder
  stereo channel. There is no microphone, desktop loopback or NestDrop capture.
- projectM runs in the existing visual child process, away from the audio
  callback. Missing DLLs or rejected shaders report an error; original scenes
  do not load projectM unless an imported preset is selected.
- Pausing holds the rendered frame. A seek/transport discontinuity restarts
  preset feedback; the upstream 4.1.8 clock is wall-clock based. This is live
  playback support, not deterministic timeline reconstruction or offline export.
- Alloy owns the scene and transition targets. MilkDrop handoffs use
  rendered colour feedback, rather than access to projectM's private simulation.
- MilkDrop 3 extended formats are not supported. Some `.milk` shader features
  differ from the original DirectX renderer; importing a file does not certify
  that its entire visual behaviour is compatible.
- Host `targetBytes` diagnostics count Alloy-owned targets. projectM's internal
  feedback/blur/noise storage is additional and is explicitly identified in the
  imported scene's diagnostics; it is not covered by Alloy's owned-scene budget.

## Dependencies and rights

Upstream: [projectM 4.1.8](https://github.com/projectM-visualizer/projectm/releases/tag/v4.1.8)
and [GLEW 2.3.1](https://github.com/nigels-com/glew/releases/tag/glew-2.3.1).
Archive SHA-256 pins, serial build commands and notice collection are in
`scripts/fetch-projectm.ps1`. The pinned release archive and
[`projectm-nested-input.patch`](../scripts/projectm-nested-input.patch) reconstruct
the modified projectM source. The DLL is dynamically loaded. The packaged
`milkdrop/licenses/dependency.json` records DLL identities and the patch name. The
projectM LGPL and GLEW/vendor notices retain their own terms. Presets and textures
retain their authors' terms and are user imports, not original Alloy material.
See [LICENSING](LICENSING.md) before any public package; this local integration
does not establish redistribution clearance.

## Focused verification

Use the packaged `visual-python/python.exe` for Python checks (the system Python
need not include PyOpenGL). Run `tests/test_milkdrop.py` and
`tests/test_original_visual.py` serially. The native focused selector is
`--milkdrop-only --milkdrop-preset <absolute-path-to-local-preset>` and must run
through `verification.run_checks`. It checks native import/deduplication,
placement, save/parse, actual engine mixing into projectM, pause, seek, switching
back to an original scene, the real Both / Make mix callback, visual Match mix
source choices and Undo/Redo. `--visual-arrangement-only` covers all three source
filters, image handling, control schemas, supported handovers and missing imports. Existing `--original-visual-only` covers the unchanged
original workflow. These are scoped checks, not a complete regression receipt.

`scripts/verify_visual_feedback.py --runtime <packaged-runtime> --scripts
<packaged-runtime> --assets <packaged-runtime> --out <fresh-owned-evidence>`
checks scene-local routing, sprite-free MilkDrop/transitions, exact pause,
all four engine directions, long-overlap feedback, direct-join continuity,
outgoing teardown and seek reset using authored disposable fixtures.

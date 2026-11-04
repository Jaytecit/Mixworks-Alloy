# MilkDrop preset import — first integration

Alloy can import `.milk` presets into a separate local library and render them
using optional libprojectM 4.1.8. The automatic arranger offers Originals, MilkDrop or Both; the default remains
Originals. Both alternates engines before choosing individual scenes, so a small
imported collection is represented even alongside the 203 original scenes. No NestDrop executable, plugin DLL, preset
collection or texture collection is copied into the application package.

## Use

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
original. Your selected image is used by original scenes, while imported presets
retain their own imagery. Automatically generated imported controls are limited
to their supported schema, and inter-engine transitions use dissolve, zoom or cut.

The importer retains the exact file in
`%LOCALAPPDATA%/Mixworks/MilkDrop-v1/<sha256>/preset.milk` with an `import.json`
descriptor. Duplicate content has the same identity. The descriptor records
the source location and read-only texture search paths (the preset folder and
its sibling `Textures` folder, when present). Keep those folders available for
presets that need external textures. The development bridge and native tests
redirect this library under their isolated data roots. Imports are limited to
128 presets, 1 MB per preset, and verified against their SHA-256 when discovered.
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
- Alloy composites both engines' output textures. Dissolve, zoom and cut work
  directly. Generative simulation handoffs involving MilkDrop use dissolve.
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
`scripts/fetch-projectm.ps1`. Upstream projectM source is unmodified and remains
available under `.local/deps/libprojectM-4.1.8`; binaries are dynamically loaded.
The packaged `milkdrop/licenses/dependency.json` records DLL identities. The
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

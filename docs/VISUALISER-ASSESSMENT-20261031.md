> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Alloy visualiser assessment — 31 October 2026

Alloy has a working scene timeline and clean output. Making it a useful DJ
visualisation tool now depends on editable looks, musical automation, smoother
delivery and dependable operation. Completing a standalone Synesthesia clone is
not a prerequisite. This is an assessment and proposed sequence, not an
implementation commitment or permission to resume deferred media work.

## What we actually have

The installed Synesthesia executable still matches the recorded analysis target:
version 1.25.4.189, SHA-256
`304433c168d81eb2667032c00d83a566be74f7723572c6859679a6f807f19766`.
Existing reports, source and retained captures were inspected; no new native
analysis, rendering, build, test run or live-app interaction was performed.

| Area | Recovered or demonstrated | Boundary |
| --- | --- | --- |
| Scene content | Original JSON definitions, GLSL shaders, optional JavaScript and images are accessible in the installation. Fresh inventory: 89 scene definitions, 48 with `script.js`, 63 with named passes. | 87 release scenes plus New and Welcome; this is not 89 independently authored Alloy scenes. |
| Scene host | Format/defaults, shader declarations, helper library, control types, multipass interface and script bindings were recovered. The Python/OpenGL/Duktape host runs existing scenes. | Some feedback, reset, coercion and smoothing behaviour remains an explicit prototype policy. |
| Audio | Spectrum/history layouts, several timing equations and raw-level normalization have static native evidence and implementation. | Processed spectrum, frequency-band normalization, envelopes, onsets and automatic song-change behaviour are not fully matched. |
| Controls and variation | Standalone player exposes scene controls, defaults/randomization and manual variation; original scene definitions contain sliders, toggles, XY, colours, triggers and dropdowns. | Alloy's worker currently uses scene defaults; its saved clip model contains identity/title, timing and offset, without saved control values or automation. |
| Media | Standalone player decodes images and silent videos, supplies raw/processed textures and supports picture adjustments and seeking. Selected original-app comparisons are retained. | Alloy has no media timeline/source integration. Cameras, video audio and general timing equivalence remain absent/unverified. Images/video remain on hold. |
| Alloy workflow | Scene placement/editing, Undo/Redo, saved arrangements, Auto Arrange/Match mix, two-scene crossfades and clean second-monitor output exist. Actual mixed PCM and project beat/BPM drive visuals. | Automatic scene choice is varied, not mood-aware. Visual blocks do not follow later music edits automatically. Seeking resets history instead of replaying it. |

The latest saved native visual-output run passed **87 focused checks**. The
preceding compositor report passed **23 checks**. The latest Alloy scene sweep
rendered **89 scenes for three frames each at 160×90**, without reported failures.
These establish bounded capability, not a full-HD performance guarantee or exact
Synesthesia equivalence. Retained fullscreen and library captures were visually
inspected during this assessment.

## What still needs to be reconstructed or compiled

The existing scene shaders are source text and already compile on the GPU.
JavaScript scripts already execute in the replacement host. We do not have the
original complete C++ project; decompiler output is evidence for reconstructing
behaviour, not a project that can simply be recompiled into Synesthesia.

Target further native investigation at visible defects:

1. **Audio response:** trace the retained spectrum-provider constructors/update
   callbacks, FFT mapping, processed curves, band normalization and trail
   smoothing. Compare controlled audio against the original with reliable,
   foreground captures. Existing native timing/audio captures include invalid
   occluded images and failed timing comparisons; do not treat them as matches.
2. **Scene behaviour:** compare feedback initialization/reset, explicit double
   buffering, script initialization/coercion and trigger/traveler smoothing where
   an unchanged scene visibly differs. Some scripts still need NaN fallbacks.
3. **Media timing, later:** resolve capture/scheduling uncertainty and broader
   seeking/VFR behaviour only when integrating media requires it. Alloy should
   have deliberate playback policies; reproducing an original failed rewind is
   not a product requirement.

Presets, MIDI/OSC, output sharing and export can be designed around Alloy's own
contracts. They do not require recovering Synesthesia's entire UI or project
format. A complete C++ port is also not a prerequisite; retain the separate
renderer if it meets measured delivery and packaging needs.

## Upgrades and recommended order

| Stage | User-visible outcome | Work and acceptance |
| --- | --- | --- |
| 1 — Editable looks | Select a scene block, change its available controls, save a named look and reuse it. | Expose parsed control metadata through the worker; persist per-block values; respect ranges/types; provide reset and intentional randomization. Demonstrate two different saved looks from one scene, Undo/Redo and save/reopen. |
| 2 — Timeline and musical control | Draw changes in available parameters and trigger variations at chosen beats/bars. | Add continuous curves and discrete events with explicit seek/pause behaviour. Use Alloy's project grid for beat/bar events and its audio for spectrum/onsets. Improve and expose sensitivity/smoothing. Demonstrate a repeatable build-up, breakdown and drop without relying on an approximate song detector. |
| 3 — Show-quality delivery | Smooth clean output, predictable resource use and clear recovery. | The current `startTimerHz(20)` requests at most 20 frames/s; GPU pixels are read back, CPU-composited when blending, JPEG-encoded, transferred and decoded. Measure this chain and remove unnecessary copies/compression; evaluate shared textures or another bounded transport. Establish 30 fps first, then 60 fps where hardware/scenes permit. Test both scene blends and representative expensive scenes before promising a resolution/rate. Add frame-time/drop diagnostics and deliberate fallback behaviour that leaves audio running. |
| 4 — Live performance | Favourite looks, beat/bar-quantized launches, MIDI/OSC mappings and master brightness/blackout. | Share one control/event model between manual actions, automation and hardware mappings. Define manual override versus recorded timeline behaviour. Reuse relevant Alloy MIDI infrastructure after checking its routing/ownership. Show an actual controller-triggered quantized change; simulated input alone is insufficient. |
| 5 — Content and delivery expansion | Images/video/logos, stream/VJ output and finished audiovisual exports. | After the media hold is lifted, adapt existing decoders/adjustments to timed sources; add layering/opacity and separate branding overlays. Add Spout for same-PC sharing, NDI later for networks. Export using the offline mix and a fixed-step visual clock, not a recording of the 20 Hz preview. Preserve seeds, events and feedback progression; validate audio/video synchronization. |

Stages 1 and 2 offer the clearest next preview. Stage 3 is a requirement before
calling the result ready for sustained shows. Master blackout should accompany
any real live-performance milestone. Full-resolution endurance, scene-switch
hitches, audio protection and recovery remain unverified; this assessment did
not run a stress test.

Useful later Alloy-specific additions include curated scene/energy tags,
phrase-based choices, eight/sixteen/thirty-two-bar variations and visuals linked
to arrangement transitions. Existing arrangement timing supports that direction;
reliable automatic drop/mood detection is a separate capability to establish.

## Runtime, content and distribution

Current builds copy renderer source/helpers into `visualiser-runtime` but still
launch Python from the prototype's local virtual environment and read installed
Synesthesia scenes. They are not self-contained visualiser packages. Package a
versioned runtime with explicit dependencies and resource limits; a native port
is one option, not an automatic requirement.

Installed scene assets and recovered shader helpers are not established as
redistributable Alloy content. A release needs appropriate rights or independently
authored replacements, including the recovered helper/media stages. Useful scene
compatibility and distribution clearance are different facts. Existing Alloy
dependency obligations also remain as documented in `LICENSING.md`.

## Sources and verification scope

Local sources:

- [Current Alloy behaviour](VISUAL-SCENES.md), `Source/VisualScenes.h`,
  `Source/SceneLane.cpp`, `scripts/visual_scene_worker.py`, and `CMakeLists.txt`.
- [Latest output receipt](../.local/viz5/verification-summary.json),
  [native result](../.local/viz5/output-native-02-result.json),
  [scene sweep](../.local/viz4/revision-sweep/report.json) and
  [compositor checks](../.local/viz4/revision-blend/report.json).
- [Prototype handover](../../prototypes/synesthesia-compat/HANDOVER.md),
  [implementation scope](../../prototypes/synesthesia-compat/README.md) and
  [audio/media investigation](../../prototypes/synesthesia-compat/REBUILD-2026-10-30.md).
- [Initial recovered contract](../../.cache/local-setup/synesthesia-scene-analysis/compatibility-contract.json).
  Later reports supersede its original unknowns. Recovered shader-template
  Evidence: `ev_736e74bd32aeb43fc7fe02272c9768409685a4fdbcb05db85270c818bf98f1ec`;
  audio packing: `ev_2afd67b46ed30516b12b480612a8ec6528170436b5cc1afa10614b9200907452`;
  raw normalization: `ev_e1ea1a7343c627bfaa36371d9bf0ea779b2c19c2f3e28853fe4060ce52339331`.

Official references checked 31 October 2026 describe product capabilities and
integration options; they do not prove compatibility with the installed target:

- [Synesthesia controls](https://app.synesthesia.live/docs/manual/controls.html)
  and [presets/show organisation](https://www.syn.live/docs/manual/presets.html)
  support controls, saved looks and triggering as relevant performance workflows.
- [Spout](https://spout.zeal.co/) documents GPU-based sharing between Windows apps.
- [NDI SDK](https://docs.ndi.video/all/developing-with-ndi/sdk) documents network
  audio/video integration.
- [Skills directory](https://skills.sh/) was consulted for method discovery.
  The existing REA/source-inspection workflow remained sufficient; no additional
  skill, dependency or external service was installed.

No application code or user session was changed. Existing media deferral remains
in force. Proposed stages need implementation and their stated demonstrations;
there is no supported overall completion percentage or delivery date.

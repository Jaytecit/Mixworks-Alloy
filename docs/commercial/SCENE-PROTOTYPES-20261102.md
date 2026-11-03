# Two scene-quality prototypes — 2 November 2026

Jay requested substantial reconstruction of every scene, starting with two existing
scenes as prototypes. Later in this task he chose **the main Alloy build with scene
library updates**, and offered to close/reopen it. No separate user preview was
launched and no existing session was changed. Artistic acceptance remains Jay's.

## Delivered compositions

- **Waterglass:** a purpose-built, full-frame multiscale liquid field. Opaque artwork
  uses aspect-correct screen-space cover with bounded refraction and reflected image
  boundaries. Cutout alpha and visible luminance contribute optical relief, surface
  normals, pigment and edge lighting. One submerged object replaces repeated quads.
- **Cathedral Descent:** a navigable vaulted interior with repeated architectural
  bays, nested ribs, stained windows, ceiling tracery, distance haze, a rose window
  and reflections traced against the same architecture. Backgrounds are mapped in
  physical wall coordinates; a single distant cutout belongs to the rose window.
  This is a stylised renderer, not a physically complete light-transport model.
- Both have a deliberate linear-light colour path: image sRGB decoding, lighting,
  restrained bloom, a hue-preserving highlight shoulder and one output sRGB encode.
  The shared old media sampler and all other scene colour paths are unchanged.
- New mid/treble transient envelopes and integrated band clocks supplement the
  existing bass signals. Pause and seek reset behaviour is covered. They are
  frequency bands, not instrument separation. Per-scene destinations and control
  meanings are recorded in each scene descriptor. Some inherited generic channels
  are unused; this is not a new manual-controls UI.
- `generate_visual_library.py` preserves these hand-authored scenes rather than
  regenerating them from shared family kernels. Catalogue size remains 100.

## Verification and evidence

`.local/scene-prototypes-07/` retains the independent before/final source snapshots,
original installed-library backup, build log, installation hashes, authored GPU
checks, before/after frames and four-second motion captures. No reference shaders,
assets or host helpers were added. Method discovery consulted the existing
[shader-dev listing](https://www.skills.sh/minimax-ai/skills/shader-dev); the owned
renderer and prior [reference study](SCENE-DEPTH-STUDY-20261102.md) informed this work.

One below-normal, single-worker incremental build succeeded. Controller/descriptor
unit checks **7/0**; focused GPU checks **20/0**. These cover both scenes, independent
band input, pause/seek, alpha-edge filtering, continuous evolution, transition target
bounds and an unchanged CC0 scene sentinel. The neutral midtone ramp's maximum error
was 0.58 byte. Waterglass circle aspect ratios at maximum refraction were 1.009 wide,
1.025 square and 1.027 portrait. These are correctness measurements, not art scores.

Inspected actual rendered PNGs, temporal strips and artwork integration at 720p.
Four-second synthetic captures at 960x540 averaged 12.5/15.1 ms including readback;
these are bounded diagnostics, not 1080p/60 Hz or sustained performance claims.
No full regression, stress, new native suite, real-mix audition, musical listening
acceptance or matched Synesthesia parity claim. Main application UI was inspected
read-only; the captured occluded window pixels did not establish new-scene playback.

## Installed main-library update

Active root: `out/releases/0.55.67-4258046694c2/`. The main executable remains SHA256
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
Eight runtime files were updated: each prototype's main/finish/descriptor, the
catalogue, and the Python band controller. Scene bloom filters were unchanged.
`scene-update-07.json` records old/new hashes and the rollback archive. Historical
activation/verification receipts remain evidence of the earlier bundle, not of this
new artistic update. The package manifest now records the updated file hashes.

The running main PID420 was not restarted, stopped or controlled. Its Python worker
may still have the prior library loaded. Jay was told installation is complete and
to save, close/reopen Alloy, then choose Waterglass or Cathedral Descent. The same
scene keys mean existing visual blocks will use the upgraded files after reload.
No AppData reset, music/session change, image movement or launcher change.

## How this scales to the remaining scenes

Do not expand the catalogue. After Jay judges these examples, rebuild a small group
at a time using the technique appropriate to each scene: screen optics, spatial
materials, particles, or actual simulation state. Each needs a complete composition,
explicit image-coordinate/alpha/colour contracts, separate musical destinations,
and inspected moving output with and without user art. A generic family variation
or passing non-black-frame test does not establish a completed scene upgrade.

Remaining acceptance: subjective vibrancy/depth, motion with Jay's music, far-edge
shimmer in the architecture, varied user media and sustained performance. A useful
next step is Jay's comparison in his usual main application, not another catalogue
expansion or automatic full regression.

## Follow-up: music must drive motion (update 08)

Jay confirmed both scenes look improved, but their music response felt limited to
pumping/brightening. Reconstructed the mappings within the same two prototypes:
Waterglass now advects travelling waves, rotating/shearing currents and fine ripples;
its submerged object follows a bounded rigid path. Cathedral's accumulated bass
controls forward travel and travelling vault deformation; mids curve its nave and
camera path; highs move window leading, the rose window and bounded camera bank.
Direct brightness modulation was reduced. No new audio-controller/native changes.

The six geometry checks use temporary shader probes excluding lighting/pigments:
height plus slope for Waterglass, world-space ray depth for Cathedral. They show
all three isolated frequency bands change geometry at identical elapsed time.
The original height-only high-frequency check failed; its report is retained.
Slope measurement was added because fine ripples change normals far more than
absolute height. This prevents a brightening-only implementation passing the check.
Final geometry6/0 and focusedGPU20/0; actual six-second motion frames inspected.
No new native build/suite, stress or real-music listening acceptance. GPU shaders
compiled through the actual scene player. Evidence: `.local/scene-motion-08/`.

Main library update08 contains only four files (both main shaders and descriptors),
with a rollback archive and current package hashes. Main executable unchanged;
Jay needs to save/reopen to load the revised shaders. User judgement of musical
movement is still needed before carrying these patterns across the catalogue.

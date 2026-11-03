# Voronoi Geode construction study — 2 November 2026

Jay requested REA and a close reconstruction, allowing the agent to choose the
reference. Selected **Voronoi Geode**, credited to **Shane** in the installed
scene. This authorises the local investigation and preview, not publication.
It supersedes the earlier instruction not to read reference-scene code for this
specific study. The original commercial replacement objectives remain open.

## Evidence and observed construction

REA inspected only `C:/Program Files (x86)/Synesthesia/release/voronoi_geode.synScene`,
without executing it. Parent Evidence:
`ev_500b58eea115a9955d13785677740aafdfb18086524b6345e574f77caaef9e20`.
Directory identity: `ee44a0334fa6ed577839016d9bca1f019f3cfff4a22c472beb89f99c9f4c6494`.
Export and individual source/image hashes are retained privately under
`.local/visual-variety-03/`. REA parsed one JavaScript file without parse errors;
its semantic coverage remains unknown with46 unresolved static relationships.
Its JavaScript analysis does **not** analyse GLSL or prove runtime execution.

Directly inspected the shipped scene.json, script.js, main.glsl and thumbnail.
The following are source observations, not guesses from the thumbnail:

| Element | Installed implementation | Consequence |
| --- | --- | --- |
| Scene package | One main shader, a small controller script, two agate images; no declared intermediate passes | Most complexity is in the material and relief, not a large pass graph |
| Cellular field | Nearest and second-nearest site distances in a2D neighbourhood; main.glsl:103 | Polygon regions split into planar-looking facets |
| Geometry | A perturbed back-plane height field; low field values become a separately identified raised border; main.glsl:140,156 | A shallow relief creates the apparent crystal depth |
| Camera | Front-facing rays, camera elevation and slow image-plane rotation; main.glsl:269 | Macro material view rather than a freestanding sculpture |
| Material | Triplanar agate-image sampling, optional media, dark border and red crystal treatment | Mineral texture and material separation are central to the look |
| Lighting | Surface-gradient normals, curvature/edge darkening, diffuse/specular light and procedural reflected environment; main.glsl:181,246 | Moving glints reveal the facets and frame |
| Musical motion | Script integrates squared high-hit activity multiplied by squared motion control; script.js:39 | Crystal motion responds to transients rather than only absolute time |
| Additional reactivity | Presence alters seed movement, highs affect shimmer, bass hits alter reflection colour, overall energy scales environment lighting | Distinct audio signals act on distinct visual properties |

The exact native implementations of the host audio envelopes, smoothing,
colour pipeline and helpers were not reconstructed. No live reference playback,
matched-input A/B recording, or pixel/motion equivalence measurement was made.

## Alloy implementation and remaining differences

**Ember Mosaic** is the resulting close visual study, alongside the independent
Neon Run and Velvet Current scenes. It reproduces the observable design of red
mineral facets, a raised dark cellular frame, a close camera and moving cool/warm
reflections. Its shader was authored here with a different seed hash, bounded
relief intersection, height profile, lighting and procedural mineral texture.
No installed shader/script, agate image or extracted host helper is packaged or
loaded by the original renderer. This is informed reconstruction, not a claim of
clean-room independence or commercial clearance.

The owned controller now preserves loud-signal dynamics with soft-knee energy
mapping, faster attack, band envelopes, transient novelty and accumulated
audio-driven motion. Its audio behaviour is an approximation of the observed
approach; it does not reproduce the native Synesthesia detectors exactly.
Alloy uses its own four-pass HDR/bloom/tone-map path and pause/seek reset rules.

Compared the reference thumbnail with captured Alloy pixels. The cellular/facet
composition and red/dark/cool-reflection treatment are recognisable; mineral
texture, seed distribution, colour balance and motion are not identical. Thin
facet highlights may shimmer at preview resolution. Judge closeness in the
running preview; technical checks do not establish artistic equivalence.

Verification: native26/0, existing GPU23/0, pack GPU31/0 and controller/descriptor
5/0. The original native attempt retained three save-fixture failures caused by
a memory-only audio source; fixed the visual-save fixture and reran that focused
native journey. No renderer change was needed for that retry. Moving captures
use authored PCM; the interactive preview uses a private copy of a user track.

Method discovery also consulted the
[skills.sh shader directory](https://www.skills.sh/minimax-ai/skills/shader-dev).
No external skill shader library was installed or copied.

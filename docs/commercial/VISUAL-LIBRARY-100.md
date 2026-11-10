> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# 100-scene library and automated media — 2 November 2026

**Historical catalogue implementation.** The [current visual direction](../VISUAL-DIRECTION.md) owns
the goal and next milestone: prove one scene, with reusable foundations for 100
distinct scenes. Earlier scope, preview/session state and implementation priorities
below are dated records; do not execute them as current instructions.


Jay requests 100 scenes, coverage of the installed Synesthesia references,
additional original designs, still-image integration, conditional video and
whole-mix control automation. He supplied `visuals/original/Sprites` and wants
users to populate it. Manual controls come later. Existing pv03 is preserved.

## Implementation and boundaries

The owned catalogue has 100 recipes: four existing hand-authored scenes, 87
reference-associated new recipes and nine additional designs. The 96 new recipes
share 21 rendering algorithms, with explicit composition variants and control
defaults. **This is not 100 independently engineered effects or 87 close replicas.**
The catalogue's `study` and `family` fields expose this distinction. Three reference
utilities (audio indicators, MIDI mapping and test card) become creative looks;
they do not implement MIDI diagnostics. Cloud Atlas is layered procedural dust,
not Cloudten's volumetric cloud marcher. Glass Estuary uses an owned bounded
reaction-like field, not Glassier's exact simulation. Such associations describe
inspiration, never reconstructed equivalence. Voronoi's earlier direct study
remains separately documented.

Source: `scripts/generate_visual_library.py` assembles owned GLSL kernels and
explicit recipe data; it never reads the reference installation. Generated
per-scene shaders are committed runtime inputs, so runtime generation is not
required. No supplied reference shaders, scripts, helper implementation, textures
or thumbnails enter the runtime. Direct reference inspection means this is not
a clean-room claim, nor a legal clearance opinion.

The reference audit covers all 87 installed release descriptors, shader function
inventories, control declarations, audio signal names, pass declarations and file
hashes. The independent local record is
`.local/visual-library-04/reference-inventory.json`; REA static package evidence
is exported beside it. REA summary evidence:
`ev_d09a60228d3eeb1a30b6a02d7f2c5c786315ba31c1c4de7aefb71f73d56ba9ad`.
Static metadata/function inventory does not establish rendered parity or native
audio-helper behaviour. No matched live reference A/B has been performed.

## Controls, timing and media

New recipes expose bounded continuous, discrete-choice, toggle, XY, RGB colour,
speed, beat-pulse amplitude and image-counter controls. XY/RGB groups are scalar
channels in the protocol. Beat pulses use project beat and mixed-PCM envelopes;
they are not arbitrary executable controller scripts. `Match mix` generates
persisted curves using section energy, saves actual values (not a random seed
that changes after reload), eases continuous changes and aligns discrete changes
to 16-beat phrases. The renderer evaluates actual transport time. Motion uses the
analytical integral of its saved speed curve, avoiding phase jumps during ramps.
Session format is 47, preventing earlier executables silently dropping automation.

Backgrounds and Sprites are scanned when the visual worker starts, capped at
512 unique images from 1024 candidate files. Actual alpha coverage distinguishes
cutouts from opaque images in the mixed Sprites folder. Backgrounds is an explicit
background role. SHA-256 deduplication prefers Backgrounds and never removes a
copy. Opaque PNGs stay backgrounds; fully transparent/invalid images are reported.
Cutout-capable recipes use alpha-preserving, proportion-correct sprite grids,
particle objects, foreground objects or surface decals. Backgrounds continue to
feed scene materials and image effects. Choosing a transparent image explicitly
overrides the cutout pool in a cutout-capable scene.

Default selection advances every 32 project beats with a four-beat crossfade;
each scene has a saved image offset. A selected project image overrides the pool.
Up to eight pool textures cover both roles and two overlapping actors; images are capped at 16MP
and reduced to at most 2048 square. Malformed images are reported, not repeatedly
retried every frame. Scenes can sample images as materials, masks, edge patterns,
warps or tiled graphic content; zoom/rotation and aspect correction are shared.
Original user files are never edited or deleted. Folder additions appear after
closing and reopening the visual browser. Folder membership is external media,
so changing its files changes later playback; it is not embedded in the mix.

The build stages code and scene assets, not user artwork. Local preview media can
refer to Jay's supplied folder through `media-library.json`. Images remain excluded
from Git and their rights are separate from application code.

## Transition inspiration and video decision

Synesthesia documents crossfade scene switching, playlist changes triggered by
breakdowns/drops, hard switches between preset values and smooth choices within
control families. Its media helpers support aspect-corrected sampling, masks,
distortion and edge detection. Sources inspected 2 November 2026:

- https://synesthesia.live/docs/faq/
- https://synesthesia.live/docs/manual/presets.html
- https://app.synesthesia.live/docs/ssf/json.html
- https://app.synesthesia.live/docs/ssf/functions.html

Alloy retains its existing dissolve, zoom, radial, directional sweeps and cut,
and now adds saved within-scene automation. This is functional inspiration, not
reuse of Synesthesia's transition implementation.

Video remains **disabled**. FFmpeg's official licence documentation confirms
that optional GPL parts and libraries such as x264/x265 affect the resulting
binary's licence. Our existing decoder bundle's exact build/provenance remains
unqualified; see `NO-PAID-LICENCES.md`. No assumption of clearance is made from a
library's reported licence string. There is no active Video folder or decoder in
this runtime. Add the folder when a qualified implementation actually uses it.
Primary sources: https://ffmpeg.org/legal.html and
https://ffmpeg.org/doxygen/trunk/md_LICENSE.html.

## Verification record

Evidence is retained under `.local/visual-library-04`. Initial bounded GPU pass:
248 checks, zero automated failures, 100 low-resolution renders. Pixel inspection
found camera penetration in two lattice compositions and excessive similarity
within families, despite that numerical pass; fixes followed. The initial result
must not be treated as artistic acceptance. Final GPU270/0, role/alpha/dedup7/0,
media unit3/0 and real-track native save/reopen are complete. Native31/0 passed on
the initial automation candidate, before the later media-role/caption changes.
Final preview uses12 unique backgrounds and3 cutouts, ignoring14 duplicate files;
ten automatically chosen scenes and200 control curves cover the real audition
track. Candidate identities and limitations are recorded in HANDOVER.
No full regression, stress, sustained-performance test, matched reference
motion or full-library artistic acceptance is claimed.
# Local activation update — V. 0.55.67

Jay selected Beats in 4D as the sole retained third-party shader. Its attributed
CC0 adaptation replaces the earlier Hyperspace Seeds approximation in the
100-entry catalogue: 99 owned recipes plus one CC0-derived scene. The unknown-
licence tanh approximation is excluded; GLSL's built-in function replaces it.
The descriptor, controller and fitted media integration are owned implementations.
See `visuals/original/scenes/beats-in-4d/PROVENANCE.md` and `licenses/CC0-1.0.txt`.
No installed scene script, thumbnail, image or extracted host helper is packaged.
The runtime is activated locally with a dedicated Python dependency bundle;
this does not establish the outstanding non-visual public-release prerequisites.

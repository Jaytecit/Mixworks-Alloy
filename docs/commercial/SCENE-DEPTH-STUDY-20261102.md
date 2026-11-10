> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Scene depth and media study — 2 November 2026

Jay's review rejects the current quality of background mapping, sprite integration,
colour and musical response. The 100 recipes establish a catalogue and automation
framework, not reference-level artistic quality. Further breadth is not the next
milestone. Study and improve one complete experience, starting with opaque images.

## Evidence and limits

Reused the REA inventory from `VISUAL-LIBRARY-100.md`, evidence
`ev_d09a60228d3eeb1a30b6a02d7f2c5c786315ba31c1c4de7aefb71f73d56ba9ad`.
Read the installed `main.glsl`, `scene.json` and available `script.js` for Waterly,
Voronoi Geode and Turbo Encabulator Fluid. Installed files were not changed.

Also rendered the unchanged reference scenes through the existing private
compatibility harness. Nine four-second, 30-step/s controlled-input sequences,
plus three fluid/media sequences; output 480×270 except Geode's configured
aspect/size. Inputs were authored uniforms, not decoded music or native detector
output. Hashes, script outputs, PNGs, short WebPs and the drivers are retained in
`.local/background-mapping-05/reference-study-02/` and
`reference-media-study-01/`. Contact sheets were inspected. All twelve runs had
no script errors/fallback logs and their source hashes remained unchanged.
These results establish selected dependencies in the compatibility harness;
they do not establish native-app parity, musical listening acceptance, or fidelity
of Alloy's implementation.

The first `reference-study-01` left the fluid's reset control at its declared
initial value. That could keep reinitialising its field, so it is retained as an
inadequate simulation experiment. The second run explicitly releases reset and
supersedes its fluid conclusions. No benchmark or broad native suite was run.

Method discovery consulted [skills.sh shader-dev](https://www.skills.sh/minimax-ai/skills/shader-dev).
The existing harness and source tracing were more useful here than installing a
general shader library. Audio semantics were checked against
[Synesthesia's audio uniforms](https://app.synesthesia.live/docs/ssf/audio_uniforms.html):
band levels, transient hits, accumulated clocks and longer-term presence are
different signals. Beat/BPM signals are additional inputs, not substitutes.

## What each scene actually does

| Reference | Image and geometry | Audio and control behaviour |
| --- | --- | --- |
| Waterly | One image pass. A moving multiscale noise field becomes a bounded displacement of screen-relative image coordinates. Pan/zoom and effect position are separate. Mirrored boundaries avoid a stretched border texel. This scene samples media directly, so do not assume it inherits the host's aspect-fit helper. | Its script integrates an effect clock from rate, overall level/presence/hits and bass presence/hits. Music changes the pace of the field. Rate, scale and intensity have different jobs; they are not interchangeable generic intensity controls. |
| Voronoi Geode | Raymarched second-order Voronoi relief with distinct mineral and dark lattice materials. Surface normals weight three texture projections. Image content is a material on geometry, not a picture over the finished frame. Diffuse/specular lighting, reflection, edge darkening and material separation create depth. | High hits drive an accumulated cell-motion clock and shimmer. Presence affects cell layout; level affects reflected light. Alternate cell metric, elevation, spin, growth and material choices alter structure. These controls have interactions and exclusions. |
| Turbo Encabulator Fluid | Two floating-point state buffers plus display. State represents a vector field and divergence, not final RGB colour. Neighbour sampling, curl, divergence and advection evolve it. Image brightness can influence the field; separate controls add refracted media colour at display time. | Bass presence affects diffusion, bass hits affect update smoothing, and high hits affect advection. Optional tracers use separate band clocks/presence. Reset, simulation substeps, growth, curl, media influence and colour mix change different stages. Media influence defaults off: loading an image alone does not activate all integrations. |

Source anchors in the installed release folders: Waterly `main.glsl` image lookup
near line 88 and its complete short script; Geode `main.glsl` lines 41, 76, 96,
322 and 362 plus its script; Fluid `main.glsl` lines 195–205, 228–285, 330–396 and
`scene.json` pass declarations near line 213. These are observations of shipped
source, not copied runtime implementations.

## Controlled observations

All cases retain the same elapsed time, image, overall level and other inputs.
Only the selected band's level, presence, hits and clock change after the common
initialisation period. This isolates a band group, not each detector individually.

| Scene | Bass-group final-frame difference from quiet | High-group difference |
| --- | ---: | ---: |
| Waterly | 34.633 | 0.000 |
| Voronoi Geode | 0.000 | 9.473 |
| Turbo Encabulator Fluid | 27.389 | 12.787 |

Numbers are mean absolute RGB-byte differences, not quality scores. Waterly's
integrated clock increased from 1.504 to 2.220 with bass; Geode's from zero to
2.339 with high input. The selective response agrees with the inspected code.
It does not mean those scenes never respond to other musical information.

In a separate fluid experiment, image influence changed the simulation while
media colour mix stayed zero (87.151 mean RGB-byte difference). Enabling colour
mix then revealed the refracted image through the changed field. This is a
concrete example of an image participating in the effect rather than sitting
on top. The supplied Copper Circuit Lab background was used read-only.

## Gaps in Alloy and decisions

1. **Backgrounds:** the common sampler currently combines arbitrary scene UVs,
   large rotation/zoom ranges and edge clamping. A photograph can be stretched
   into a material domain or dragged into border streaks. A universal screen-space
   replacement would hide those defects but also destroy legitimate surface
   mapping. Separate fitted image/refraction, material projection, and simulation
   input contracts; declare which a scene uses. Fit/fill must preserve aspect;
   intentional texture repetition must have its own boundary policy.
2. **Sprites:** classification by alpha is useful but does not create integration.
   Existing repeated cutout quads/decals are insufficient. A selected scene needs
   explicit alpha-aware influence on shape, material, lighting or simulation.
   Hidden RGB outside the cutout must not become an unintended force/mask.
3. **Colour:** the shared palette, media mix, bloom, compression and gamma paths
   need a separate audit with neutral image and colour-ramp fixtures. Geode has
   a deliberate input/output colour treatment and material contrast. Merely
   raising saturation across the catalogue would not establish the cause of
   Alloy's washed-out appearance. This cause remains unverified.
4. **Audio:** Alloy currently supplies three broad bands, overall energy, one
   overall novelty envelope and one combined motion accumulator. A global beat
   pulse and shared domain transform dominate many recipes. Separate per-band
   transient/presence/motion signals and scene-specific destinations are missing.
   Spectrum/history textures are useful for scenes that actually need them, not
   mandatory overhead for every effect. Do not claim this separates instruments
   or vocals; frequency bands are not stems.
5. **Controls:** twenty-one generic curves per recipe do not recreate the
   reference controls. Define meaningful scene-specific ranges, discrete states,
   envelopes and compatible combinations before automating them over the mix.
   Native manual UI remains deferred.

The provisional universal background patch was archived and removed from active
source. Its GPU report has 20 passing and 3 failing checks (maximum-warp circle
proportions); it is not an accepted fix. Evidence and code remain in
`.local/background-mapping-05/provisional-background-source.zip` and `gpu-01/`.
The live pv04 runtime was never replaced.

## Next implementation milestone

Use **Waterglass** as the first image-led reconstruction, replacing its generic
liquid-relief recipe with a purpose-built refraction scene. Start with faithful
image proportions, coherent bounded distortion, separate image/effect positioning,
and intentional boundary behaviour. Do not apply one texture policy to every scene.
Compare square, wide and portrait fixtures plus Jay's artwork, including corners
and lettered subjects, before calling background handling fixed. Then judge it
against a copy of his mix. Sprite integration, colour and audio response remain
separate review steps; the study above defines the architecture they need.

Future acceptance must include moving captures and band-isolation comparisons,
not just non-black frames, shader compilation or counts of controls. One strong
example per relevant technique should precede library-wide application. No new
parity claim or preview is being delivered by this investigation alone.

## Preserved user mix

Live development instance pv04/PID19536 was saved under the new name
`jay-visualiser-reference-20261102.mixworks` after Jay confirmed readiness.
Five clips span 883.097 seconds. Session SHA-256:
`b92698d5acb7affde5a64951d46a457aa74cd4364711df9ef8f22a9747c6d646`.
Independent exact save and an audition copy are in `.local/background-mapping-05/`.
The audition copy includes five hash-verified audio copies (144.7 MiB); it omits
unused library entries only. Clips, tracks and visual automation compare exactly
to the saved original. See `mix-copy/manifest.json`. No source media was moved,
deleted or rewritten, and the live app was not restarted or reconfigured.
At final inspection PID19536 was absent and a read-only bridge status request
timed out. Its exit cause is unknown; this task did not close or terminate it.
The verified save and audio copies were already complete.

Direct reference inspection is not clean-room work. The reference code, helper
assets and textures remain private research dependencies; none was added to the
owned runtime or distribution. Existing licensing/release prerequisites remain.

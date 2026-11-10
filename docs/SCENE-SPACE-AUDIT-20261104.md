> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Original scene space and motion audit - 4 November 2026

Review and recommendations only, as requested. No shaders, scene controls, previews, application bundle or running session were changed. MilkDrop presets and the deferred NestDrop collection scan are outside this audit.

## Criterion

Black space is acceptable when substantial motion regularly occupies it: large pumps, expansions, rotations, sweeps or travelling elements. A small object rotating in the middle does not satisfy that. Dark colour alone is not a defect; a featureless coloured wash also does not establish useful activity. Prefer scene-specific movement or depth rather than one common wallpaper.

## Direction update: stronger rhythmic impact

After comparing the original and continuous-travel tunnel examples, Jay said he
prefers the jumping version because it gives the scene much more energy, and
suggested that this may be missing from many current scenes. Treat this as the
preferred direction for the next comparisons, not approval to alter the library.

Prioritise visibly large pumps, snap rotations, sudden forward/back surges and
rhythmic camera excursions in suitable scenes. Do not smooth away impact simply
because a motion discontinuity exists. An eventual implementation should make
jump amount/timing controllable, respect pause, and retain a scene's own character;
it must not silently replace the preferred energetic look with gentle drift.
Screen coverage over time remains the criterion. Add backgrounds where stronger
movement still leaves persistent dead regions. Existing measurements below remain
unchanged; earlier background recommendations are provisional under this update.

## Main recommendation

Start with punchier motion comparisons for Liquid Torus, Prism Bloom and Orbit Theatre: large rhythmic expansion, camera surges and snap rotations. For Iridescent Wake, Graphite Wake, Ember Advection and Nebula Press, compare broader sweeps/bursts with matching moving depth; use backgrounds if movement alone cannot cover the field without losing the main form. Investigate Plasma Orchard separately before disguising its near-empty output. Keep Neon Run, Soma Lanterns and Circuit Confetti for now: motion substantially changes the still-image judgement.

## Evidence and limits

- Inspected every one of the 203 main previews in labelled contact sheets and screened all 1,218 named-look previews. The six-look pass measured dark coverage; it was not a visual inspection of every individual look.
- Screened a 48-region grid for large, low-detail dark areas: 65 candidates from stills, 45 with at least a quarter of the grid featureless. Thresholds are triage aids, not visual pass/fail rules.
- Rendered and inspected 20 representative Signature scenes for 16 seconds at 320x180/10 fps with a common opaque background, transparent wordmark and authored 120-BPM low/mid/high input. Warm-up first 2 seconds excluded from metrics. This is offscreen controlled evidence, not a real mix or listening test.
- Motion sheets use 2, 6, 10, 14 and 15.9 seconds; per-scene clips retain all 160 frames. A four-second rolling reach is a diagnostic, not a user-imposed timing requirement.
- Plasma Orchard was also measured for 16 seconds at 30 fps and remained almost empty under this fixture. The cause is not yet diagnosed, and this does not establish failure with every image/song.
- Most shortlisted scenes have still/source evidence only. Do not describe them as individually motion-verified or automatically change all of them together. No native build, regression, benchmark, stress or production-app interaction was needed for this review.

Local evidence: `.local/scene-backgrounds-20261104` (contact sheets, complete 203-scene CSV, metrics, motion clips, scripts and initial failed/limited attempts). The first motion attempts used a missing bundled-image path, then a cutout-only fixture with an incorrect final scene key; those were retained and superseded by the validated common-input 20-scene run.

## Shortlist totals

- Priority candidate: **38**.
- Keep: motion already supports the brief: **3**.
- Motion/growth first; background conditional: **5**.
- Secondary candidate: **19**.
- No obvious persistent empty field in still review: **137**.
- Investigate near-empty behaviour: **1**.

## Motion-reviewed scenes

| Scene | Largest active region coverage | Reach over the sampled 14 seconds | Recommendation |
|---|---:|---:|---|
| Iridescent Wake | 31% | 38% | Background first: a matching image-fed mist/current across the field. A narrow jet cannot cover the height through its present small movement. |
| Ember Advection | 19% | 73% | Add drifting smoke/heat depth and broader ember gusts. Existing sparks move, but remain tiny and sparse. |
| Graphite Wake | 38% | 54% | Add a moving ink wash and secondary calligraphic strokes above/below the ribbon; keep its main wake. |
| Cellular Lace | 90% | 90% | Motion/growth first: it already expands substantially. Increase distributed seeding and shorten the sparse opening before considering a background. |
| Nebula Press | 50% | 50% | Widen/pump the two compression fronts and carry a subtle moving nebula through the outer field. |
| Ion Ballet | 77% | 98% | Keep the travelling flock character; increase burst extent and frequency of edge visits before adding a background. |
| Prism Bloom | 52% | 67% | Motion first: a much closer camera swell/flyby. Its current rotating sculpture stays central; existing dust/background is too subdued. |
| Liquid Torus | 35% | 35% | Motion first: a pronounced beat-driven scale/dolly swell with off-axis travel; add volumetric depth only if needed. |
| Plasma Orchard | 0% | 0% | Investigate population survival/visibility before adding a background. Almost empty with this strong rhythmic fixture at both 10 and 30 fps; do not hide that with decoration. |
| Neon Run | 100% | 100% | Keep for now: regular expansion and rotation already reach the edges in the motion probe. |
| Soma Lanterns | 100% | 100% | Keep for now: grows across the field after the sparse opening. A later representative preview would describe it better. |
| Orbit Theatre | 50% | 73% | Motion first: large image-carrier approaches and sweeping orbits, with a slower background/parallax layer as support. |
| Polar Canopy | 67% | 67% | Add visible motion to the empty lower band: reflected light, ground fog or a counter-moving floor structure. |
| Prismatic Waltz | 96% | 98% | Keep its large transient bursts; strengthen how often they reach the edges, not a permanent background by default. |
| Spectral Migration | 69% | 96% | Travels broadly already. Increase sweep width/frequency if required; current movement is a stronger remedy than a generic backdrop. |
| Chroma Escalier | 67% | 67% | Enlarge and stagger image stair flights so they pass through the black gaps; consider a counter-moving secondary flight. |
| Prism Datastream | 77% | 94% | Bands sweep through much more of the frame than the still suggests. Increase vertical excursion/regularity first. |
| Folded Scaffold | 71% | 75% | Motion first: a close tumbling pass/large rhythmic dolly. Current rotation does not carry the scaffold into the corners. |
| Lucent Ranges | 67% | 67% | Keep approved Lucent Ranges geometry/camera. Add restrained, moving sky structure that remains readable above the ridge. |
| Circuit Confetti | 71% | 100% | Keep for now: independently drifting islands visit the whole frame regularly, despite a sparse still. |

Activity coverage combines visible intensity with local spatial structure in a coarse 8x6 grid. It describes the sampled input and is not the fraction of foreground-object pixels.

## Scene-by-scene proposed changes

| Priority | Scene | Evidence | Recommended first change |
|---|---|---|---|
| Behaviour | Plasma Orchard | Motion sampled | Investigate population survival/visibility before adding a background. Almost empty with this strong rhythmic fixture at both 10 and 30 fps; do not hide that with decoration. |
| First | Iridescent Wake | Motion sampled | Background first: a matching image-fed mist/current across the field. A narrow jet cannot cover the height through its present small movement. |
| First | Chromatic Torso | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Liquid Torus | Motion sampled | Motion first: a pronounced beat-driven scale/dolly swell with off-axis travel; add volumetric depth only if needed. |
| First | Opal Reactor | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Ember Advection | Motion sampled | Add drifting smoke/heat depth and broader ember gusts. Existing sparks move, but remain tiny and sparse. |
| First | Nebula Press | Motion sampled | Widen/pump the two compression fronts and carry a subtle moving nebula through the outer field. |
| First | Pulse Cocoon | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Graphite Wake | Motion sampled | Add a moving ink wash and secondary calligraphic strokes above/below the ribbon; keep its main wake. |
| First | Facet Voyager | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |
| First | Luminous Atoll | Stills/source only | Seed activity across more of the domain and shorten the quiet start; use larger growth/pulse excursions. |
| First | Velvet Spores | Stills/source only | Seed activity across more of the domain and shorten the quiet start; use larger growth/pulse excursions. |
| First | Gelatine Satellites | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Velvet Geode | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Afterglow Atlas | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| First | Fluorescent Nursery | Stills/source only | Seed activity across more of the domain and shorten the quiet start; use larger growth/pulse excursions. |
| First | Orbit Theatre | Motion sampled | Motion first: large image-carrier approaches and sweeping orbits, with a slower background/parallax layer as support. |
| First | Tidal Lichen | Stills/source only | Extend growth into the outer field and add distributed reseeding or a moving microstructure layer. |
| First | Prism Bloom | Motion sampled | Motion first: a much closer camera swell/flyby. Its current rotating sculpture stays central; existing dust/background is too subdued. |
| First | Orbital Etching - Precessing ellipses | Stills/source only | Widen flock/attractor travel and outward bursts; add a second moving depth layer if edge visits stay rare. |
| First | Mercury Cathedral | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Raster Roulette | Stills/source only | Increase full-height/full-width raster or fragment excursions; preserve the existing acquisition/release character. |
| First | Aether Nuclei | Stills/source only | Use pronounced rhythmic scale/dolly excursions and off-axis travel; rotation in place is insufficient. |
| First | Lenticular Dreams | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| First | Living Mosaic | Stills/source only | Seed activity across more of the domain and shorten the quiet start; use larger growth/pulse excursions. |
| First | Crystal Pendulum | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |
| First | Glass Delta | Stills/source only | Animate the unused sky/ground with scene-specific clouds, haze, light or distant structures; retain terrain identity. |
| First | Broken Spectrum | Stills/source only | Increase full-height/full-width raster or fragment excursions; preserve the existing acquisition/release character. |
| First | Folded Scaffold | Motion sampled | Motion first: a close tumbling pass/large rhythmic dolly. Current rotation does not carry the scaffold into the corners. |
| First | Velvet Mycelium | Stills/source only | Extend growth into the outer field and add distributed reseeding or a moving microstructure layer. |
| First | Lucent Ranges | Motion sampled | Keep approved Lucent Ranges geometry/camera. Add restrained, moving sky structure that remains readable above the ridge. |
| First | Prism Switchboard | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |
| First | Resonant Iris | Stills/source only | Bring image carriers close enough to sweep the edges; use a low-contrast moving parallax field behind them. |
| First | Spectral Carousel | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| First | Chromatic Fault | Stills/source only | Extend moving turbulence beyond the main image banks; add broad beat-driven surges. |
| First | Enamel Scales | Stills/source only | Extend the repeating relief into the outer field or add a slower counter-moving relief layer. |
| First | Spectrum Shards | Stills/source only | Increase full-height/full-width raster or fragment excursions; preserve the existing acquisition/release character. |
| First | Chromatic Salon | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| First | Prism Rosarium | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| Conditional | Cellular Lace | Motion sampled | Motion/growth first: it already expands substantially. Increase distributed seeding and shorten the sparse opening before considering a background. |
| Conditional | Ion Ballet | Motion sampled | Keep the travelling flock character; increase burst extent and frequency of edge visits before adding a background. |
| Conditional | Prismatic Waltz | Motion sampled | Keep its large transient bursts; strengthen how often they reach the edges, not a permanent background by default. |
| Conditional | Spectral Migration | Motion sampled | Travels broadly already. Increase sweep width/frequency if required; current movement is a stronger remedy than a generic backdrop. |
| Conditional | Prism Datastream | Motion sampled | Bands sweep through much more of the frame than the still suggests. Increase vertical excursion/regularity first. |
| Later | Opal Smoke | Stills/source only | Add matching full-field ink wash, smoke or ember depth; keep the main stroke/jet clear. |
| Later | Pigment Carnival | Stills/source only | Extend moving turbulence beyond the main image banks; add broad beat-driven surges. |
| Later | Polar Canopy | Motion sampled | Add visible motion to the empty lower band: reflected light, ground fog or a counter-moving floor structure. |
| Later | Chromaflora | Stills/source only | Extend growth into the outer field and add distributed reseeding or a moving microstructure layer. |
| Later | Compound Aperture | Stills/source only | Expand folded geometry/rays farther through the frame; retain its radial identity. |
| Later | Orbital Etching - Binary satellites | Stills/source only | Widen flock/attractor travel and outward bursts; add a second moving depth layer if edge visits stay rare. |
| Later | Petal Circuit | Stills/source only | Extend growth into the outer field and add distributed reseeding or a moving microstructure layer. |
| Later | Saffron Undertow | Stills/source only | Extend moving turbulence beyond the main image banks; add broad beat-driven surges. |
| Later | Traced Arcade | Stills/source only | Push passing frame planes beyond the edges and add counter-moving distant frame detail. |
| Later | Scarlet Relay | Stills/source only | Widen flock/attractor travel and outward bursts; add a second moving depth layer if edge visits stay rare. |
| Later | Catoptric Tide | Stills/source only | Let projection/echo layers overshoot the frame; use larger phased zooms or travelling reflections. |
| Later | Chroma Monsoon | Stills/source only | Extend moving turbulence beyond the main image banks; add broad beat-driven surges. |
| Later | Chromatic Ravine | Stills/source only | Animate the unused sky/ground with scene-specific clouds, haze, light or distant structures; retain terrain identity. |
| Later | Hexa Resonator | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |
| Later | Honeylight Causeway | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |
| Later | Opaline Dunes | Stills/source only | Animate the unused sky/ground with scene-specific clouds, haze, light or distant structures; retain terrain identity. |
| Later | Prism Spores | Stills/source only | Extend growth into the outer field and add distributed reseeding or a moving microstructure layer. |
| Later | Chroma Escalier | Motion sampled | Enlarge and stagger image stair flights so they pass through the black gaps; consider a counter-moving secondary flight. |
| Later | Kinetic Vault | Stills/source only | Add close camera passes or much larger expansion; give unused areas moving architectural depth. |

## Implementation direction for a later authorised pass

Use small coherent groups. First match ink/jet/spark backgrounds to their own flow; then address central sculptures with strong, bounded scale/camera movement. Preserve current main forms and use independent depth/speed in background layers. Keep landscapes separate so added skies/ground activity do not replace their accepted geography. Check quiet and strong passages, opaque art and cutouts, and exact pause. Update previews only after the actual motion is accepted.

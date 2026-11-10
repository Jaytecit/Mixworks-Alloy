> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Synesthesia technique and Alloy capability audit

**Historical mechanism research.** The [current visual direction](../VISUAL-DIRECTION.md) owns
the goal and next milestone: prove one scene, with reusable foundations for 100
distinct scenes. Earlier scope, preview/session state and implementation priorities
below are dated records; do not execute them as current instructions.


2 November 2026. This audit superseded update09 at that checkpoint; its execution
direction is now historical.
Jay rejects the current movement and image integration despite successful technical
checks. The target is distinct, musically responsive scenes for 120–160 BPM EDM,
images that participate in the effect, and transitions that change generation.

## Finding

The main shortfall is **missing mechanisms**, not insufficient animation speed.
The reference library contains different evolving systems: vector-field flow,
reaction–diffusion, cellular growth, stored particle dynamics, nonlinear colour
feedback, image-selective feedback, raymarched materials and volumetric integration.
Alloy's 100 recipes mostly share 21 simpler families. A family called
`reaction-field` does not currently implement the reference reaction–diffusion
equations; a particle-network appearance does not establish persistent particles.
Adding beat oscillation to these substitutes cannot recover their characteristic
motion. Update09's compile/pixel-difference checks did not establish that fidelity.

Jay subsequently identified the dominant defect precisely: nearly every scene
moves the image one way and back in time with the beat. Source confirms the common
cause: `common.glsl:9–11` defines bass/mid/treble swings as fixed sine functions of
the same project beat (with variant phase offsets); all seven generated kernel
files use them. Different axes/amplitudes do not create independent musical
behaviours. The previous instruction against slow one-way travel was incorrectly
generalised into ubiquitous beat reversal. Remove that as a shared motion policy
when rebuilding each group; do not merely randomise its phase, axis or speed.

Voronoi Geode was the useful precedent: study the actual mechanism, material,
lighting and selective audio destinations, then build a coherent original scene.
Do not replace that process with a generic recipe selected by visual category.

## Scope and evidence

Reference: installed Synesthesia **1.25.4.189**, executable SHA-256
`304433c168d81eb2667032c00d83a566be74f7723572c6859679a6f807f19766`.
Inventory covers **87 release scenes, Welcome Scene and New Scene: 89 packages**,
with descriptors, shader/script identifiers, line references and all package-file
hashes. All descriptors parsed. 47 packages have scripts; 63 declare intermediate
targets; 8 declare smooth transition families and 18 hard transition families.
Fluid Body has 10 intermediate targets plus final output. Counts describe declared
resources, not every possible runtime branch. The inventory distinguishes zero
intermediate targets from one; an earlier scratch summary conflated them.

The [scene-by-scene index](SYNESTHESIA-SCENE-INDEX-20261102.md) accounts for every
package and maps release references to Alloy's named recipes. The capability table
below groups mechanisms rather than counting cosmetic variants as new techniques.
Selected dataflow was traced directly, especially all eight user examples. Other
index entries remain structural source classifications, not individual native
visual acceptance. This is a catalogue/technique audit, **not exhaustive recovery
of all native implementation details or a claim of parity**.

Private evidence root: `.local/synesthesia-audit-10/`. It contains reproducible
inventory output, REA evidence/snapshot, controlled probes, and exact copies/hashes
of Jay's nine supplied images. Sources remain in the installed application.
No reference shader, helper library or texture was added to Alloy's runtime.
Direct source inspection is not clean-room work. Previous private research was
reused rather than rerunning native audio/media investigations.

Primary documentation cross-checks: [audio signals](https://app.synesthesia.live/docs/ssf/audio_uniforms.html),
[shader inputs](https://app.synesthesia.live/docs/ssf/standard_uniforms.html),
[helpers](https://app.synesthesia.live/docs/ssf/functions.html),
[scene/pass/control descriptors](https://app.synesthesia.live/docs/ssf/json.html),
[persistent scripts](https://app.synesthesia.live/docs/ssf/script.html),
[presets](https://app.synesthesia.live/docs/manual/presets.html),
and [scene switching FAQ](https://synesthesia.live/docs/faq/).
Online documentation may describe a different revision; shipped source/native
evidence takes precedence for this installation. Method discovery also consulted
[skills.sh shader-dev](https://www.skills.sh/minimax-ai/skills/shader-dev);
the existing REA evidence and controlled renderer were more useful here.

## The eight supplied examples: one image, different jobs

Jay supplied `Martooni.png` and eight native screenshots in the following order.
These establish the desired appearance. They do not establish time, audio,
parameter values, transition behaviour or exact motion from a single frame.
Line anchors below are in each installed release package's `main.glsl`.

| Reference → current Alloy recipe | What the image actually does | Motion and evidence | Required Alloy change |
| --- | --- | --- | --- |
| Vein Melter → Vein Current | With `grab_frame`, luminance sets vector direction and a signed state component (131–133). Independently, the evolving field displaces image sampling and modulates its colour (155–230). The screenshot can use the latter without enabling the former. | Two state targets; neighbour curl/divergence/Laplacian, multi-step advection, rotation and smoothing (34–107). Bass/high input can push/pull painted forces (109–128); script supplies heartbeat timing. | True signed state field, image capture into that field, separate refraction/colour controls, and evolving fine veins. Current shared cellular surface is insufficient. |
| Transparent Isoslices → Isosurface Lantern | The image is sampled using the ray's resulting spatial coordinates (129–130), so the sliced depth field rearranges its placement. This is not evidence that the image creates the distance field. | Organic/sliced distance field, repeated ray advancement and accumulated surface lighting (27 onward,92–118); bass morph, beat slice, rate and high-hit accents are distinct. | Layered ray accumulation and spatial media projection with coherent depth; preserve broad bands of the logo across changing slices. |
| Thresholder → Posterised Mirage | A colour/alpha test selects which fresh image regions survive; other pixels come from transformed previous output (148–164,190–235). Palette conversion can happen before or after feedback. | Previous colour itself steers subsequent coordinates; presence, band levels and capture events affect movement and replacement. Two intermediates can swap routing in glitch mode (238–257). | Selective image replenishment, colour-driven feedback, capture/hold and meaningful palette controls. A posterisation filter alone misses the evolving regions inside the logo. |
| Terrain Lattice → Topographic Grid | Three spatial image projections are weighted by surface normals, then used on both triangular terrain and lattice rods (330–343,489–523). The logo is the lit material. | Procedural triangulated height surface, separate rods, normals, occlusion/shadow and specular/bump treatment; audio changes terrain/rod presentation. | Proper flat triangular planes, separately shaded lattice, coherent triplanar artwork and camera. Do not claim the logo itself supplies terrain height. |
| Swarm → Firefly Assembly | Luminance changes the palette coordinate of the particle/trail field (221–225). The logo and moving swarm therefore share the colour-generation function. | First target stores particle position/velocity; forces, resistance and limits evolve it (13–100). Second target draws directed shapes/halos and neighbourhood trails (140–177). | Persistent interacting particles and field-driven palette integration. In this source, media is not a particle-force input; do not invent that as a reference claim. |
| Sun Set → Solar Wash | A media mask modulates a channel entering a recursive colour system (50–58). Previous colour controls future sample coordinates (37–44); another buffer blends delayed/shifted state. | Bass-triggered resets, high-hit mask modulation, frame-stepped updates and presence-dependent nonlinear contrast (63–109,119–122). | Nonlinear recursive state and image-mask injection. Echoing logo contours must emerge through repeated evolution rather than several static copies. |
| Sprinkles → Microbial Mosaic | Media seeds initial state (188–194) and continues feeding two reacting concentrations, gated by hits/pulses (111–124). Output is shaded from state gradients (159–185), not a direct image overlay. | Laplacian diffusion plus reaction/feed/kill terms (78–106), two update/copy pairs per frame, and separate gradient-based colouring. | Genuine reaction–diffusion with image feeding, growth/erasure/pulses and derived normals. Letter edges should grow into the ridged patterns seen in the example. |
| Sinusoidal Warp → Wave Lens | The same iterated coordinate field both displaces the image and supplies its brightness/shading (27–68), with mirrored sampling and independent image position. | Three coupled cosine-warp iterations; script integrates a clock from rate and audio energy/presence/hits. | Coupled deformation and shading with purposeful optical depth. It need not become a fluid simulation to match this distinct role. |

These are all legitimate image integration, even where implemented with texture
sampling. The distinction is **where image information participates and how the
scene evolves**, not whether a GPU texture exists. Some examples keep the logo
readable; others consume it as simulation input. A universal image blend amount,
universal fit policy or universal saturation adjustment would erase that variety.

## Individual musical behaviour, not one shared movement

For each scene, define the input feature, affected mechanism, response envelope,
state persistence and behaviour between hits. Scene differences must survive a
comparison at the same BPM. These are observed reference destinations,
not a requirement to enable every optional effect by default.

| Scene | Distinct musical action observed in source | Temporal character and Alloy acceptance |
| --- | --- | --- |
| Vein Melter | High hits and squared bass level apply opposing local forces when painting is enabled; an integrated quarter-speed BPM clock drives heartbeat modulation of the field. | An impulse changes subsequent flow. The field continues evolving between events; image-wide left/right translation is not the principal response. |
| Sprinkles | High hits feed concentrations in image-defined regions; pulse/growth/erase controls change diffusion/reaction evolution. | Cells grow, split or erode locally after a hit. Inspect changes in state and contours with colour held fixed. |
| Swarm | High hits/presence alter inter-particle forces; optional centre/frantic behaviours add other force terms. | Particle velocity and relationships evolve with inertia/resistance. Compare distributions and paths, not just brightness or shared position offsets. |
| Thresholder | Bass/high-mid levels affect the keep threshold, mid level affects feedback zoom, presence drives colour-dependent displacement, and hits can capture a new frame. | Some regions refresh while others retain evolving history. Equal-BPM inputs with different transients must yield different replacement/evolution patterns. |
| Sun Set | Bass hits can reset; high hits change image-mask injection; mid presence changes feedback coordinates; stutter regulates update cadence. | Abrupt reseeding and delayed nonlinear evolution coexist. The result must not reset or reverse uniformly on every beat. |
| Terrain Lattice | Beat envelope changes lattice thickness, presence can affect height perturbation, beat time can animate floor lighting, bass level can control colour splitting. | Structural articulation and optional optics are distinct from camera travel. Camera reversal is not required to prove reactivity. |
| Transparent Isoslices | Squared bass accumulates morph/travel clocks, beat envelope can change slicing, mids affect colour and high hits cause local accents. | Structural evolution has memory through its clocks; slicing and accents have different envelopes. Test morph geometry separately from colour. |
| Sinusoidal Warp | Script integrates rate with overall level×hits plus bass presence into the distortion clock. | Music changes how a coupled field evolves; it does not simply reverse the whole image every beat. Preserve sustained motion through attacks and releases. |

Source anchors: the eight detailed rows above; additionally Swarm59–75,
Terrain141/153/508/601, Transparent Isoslices33/40/55/123/133 and its script,
Vein Melter's BPMCounter script, and Sinusoidal Warp's Timer script.
Some reference integrators advance per frame rather than by dt; new Alloy
implementations should retain the intended dynamics with explicit time handling.

Acceptance must compare steady energy, isolated hits, changing presence and
different band patterns at **the same tempo**. A constant-level input with a beat
clock should not reveal one identical reversal policy across the catalogue.
Not every scene needs beat locking, every band, violent motion or visible image
destruction. Sustained flow, transient impulses, event capture, structural changes,
population evolution and optical modulation are different useful behaviours.

## Capability coverage and gaps

Status describes the main Alloy runtime at update09: **partial** means a related
mechanism exists but not the full demonstrated behaviour; **missing** means no
equivalent contract/implementation was found; **present** is scoped, not parity.
Items with an unknown native detail are not assigned an invented equation.

| Capability | Reference evidence/examples | Alloy status and required work |
| --- | --- | --- |
| Named multipass dataflow | Fluid Body, Compound Iris, Sprinkles | Partial: maximum 4 total stages and earlier-stage inputs only. Add explicit resource graphs, enough stages and per-target sizes. |
| Cross-buffer previous-frame state | Fluid Body, Sprinkles, Swarm | Missing general bindings. Current history is only a pass's own previous texture. Declare current/previous edges explicitly; reject current-frame cycles. |
| Signed/high-precision state | Vector fields, particle positions, reaction concentrations | Partial: RGBA16F/8; add RGBA32F/data targets where justified, avoiding colour conversion/clamping on state. |
| Sampler policy and resolution | Named target width/height, nearest/linear/mipmap, repeat/clamp/mirror | Partial. Specify per-resource filtering, wrapping, mip generation and fixed versus output-relative dimensions. |
| Previous final output | Myoplasm, Boreal and feedback scenes | Partial local history; distinguish visual history from simulation state and define reset/seek behaviour. |
| Shared helper/include libraries | Shipped SSF, hg_sdf and lygia includes | Partial owned GLSL helpers. Build only needed owned/licensed algorithms; do not ship recovered SSF. |
| Noise, domains and repetition | Value/simplex-style noise, fBm, Voronoi, polar/kaleidoscopic folds, repeated grids | Present broad primitives. Their parameters, derivatives and role inside state/geometry matter more than using the same noise name. |
| Analytic patterns and hierarchy | Circles5, PLUS, Meta Experiment, Pop, Replik8 | Partial. Preserve each circle/grid/quadtree/shape construction and its discrete versus continuous transitions. |
| Projection and camera systems | Equirectangular, curved tunnels, raymarch paths | Partial panorama and procedural cameras. Separate camera travel, object deformation and image sampling; reversal is a design choice, not a universal fidelity fix. |
| Vector advection/curl/divergence | Turbo Fluid, Vein Melter, Dynamical Flame | Missing equivalent state dynamics; RGB-history warping is not sufficient. |
| Reaction–diffusion | Sprinkles; related feedback systems in Biopsy | Missing actual two-species reaction system. Keep separate from colour feedback. |
| Continuous cellular growth | Gaussian Smoothlife: inner/outer Gaussian neighbourhoods and birth/survival | Missing. Noise/Voronoi patterns do not substitute for populations evolving from local state. |
| Multiscale flow aggregation | Fluid Body's eight flow scales and circular interpolation | Missing; current Mercury Skin is a relief recipe. Needs its own original mechanism after a smaller flow system is proven. |
| Persistent particle dynamics | Swarm, Neuron Proximitors | Missing equivalent positions/velocities/neighbour relationships; current particles are mainly analytic trajectories. |
| Neighbour sorting/morphology | Pixel Sort, Hills Eels, Quicksand, Min Max Hex | Partial appearance; implement sorting/selection rules and recurrent neighbourhood state where applicable. |
| Colour-derived velocity | Milk Honey Smoke Bile, Thresholder | Missing dedicated feedback mechanics; preserve colour-to-motion causality. |
| Packed/multiscale blur fields | Biopsy, Molten, Compound Iris | Partial simple post blur; separate simulation neighbourhood estimates from final bloom. |
| Antialiasing and temporal sampling | Bouncy Hexagons, Fluid Body, temporal/post stages | Partial. Match the needs of each geometry/data representation; sample history deliberately and inspect edge shimmer in motion. |
| Scene-specific colour and optical processing | HSV/RGB, YUV/YCoCg in Compound Iris, blackbody-style palettes, dispersion/dither | Partial generic palette/bloom/compression. Preserve working space, gradient-derived shading, contrast and purposeful colour remapping; do not apply one saturation cure. |
| Procedural SDF solids/CSG | Glob, Alien Cavern, Deeper, hex scenes | Present as a broad category; partial shape, navigation, material and control fidelity. Preserve individual constructions. |
| Fractal folding/escape fields | KIFS Flythrough, Scaffold, Mandelbrot Decoration | Present broad categories; different orbit/fold rules and their temporal behaviour still need individual study. |
| Volume/density integration | Cloudten, Fire Tunnel, Transparent Isoslices | Partial/absent equivalent integration in several assigned recipes. Dust layers or solid surfaces do not demonstrate volumes. |
| Terrain triangulation/lattice | Terrain Lattice | Partial; need distinct terrain and structural material/normal treatment. |
| Multi-sample reflection/lighting | Neon City, Raymarched Reflections, Geode | Partial. Inspect sampling and accumulation per scene; do not label every raymarcher a path tracer. |
| Material projection and bump | Geode, Terrain, Circuit Bending | Partial. Controlled triplanar/surface projection must coexist with screen optics, not be replaced by a universal screen fit. |
| Image as simulation seed/feed | Sprinkles, Smoothlife, Quicksand | Missing equivalent behaviour in their current recipes. Separate seed, ongoing feed and visible colour amount. |
| Image as force/flow features | Fluid Body derivatives/chromaticity; Vein grab-frame | Missing. Prove state changes with image colour visibility disabled. |
| Image-selective replacement | Thresholder, Canvas/masks | Partial masks; missing threshold-controlled recurrent replacement/capture. |
| Image palette/field modulation | Swarm, Sun Set, Lost in the Sauce | Partial generic mixing. Preserve luminance/colour-to-field mappings and selective roles. |
| Image refraction/optical shading | Waterly, Sinusoidal Warp, Vein Melter | Partial; Waterglass improved, but this is not a substitute for all fluid-like scenes. |
| Raw versus processed media | Native media stage, SSF helper calls | Partial. Keep distinct media resources, adjustments, masks and sampling contracts. |
| Alpha and masks | Existing native media/Canvas probes | Partial cutout support. Alpha, luminance masks and hidden RGB are different; state injection needs an explicit policy. |
| Video/live media transport | Documented media and external integrations | Still-image workflow is not equivalent. Native timing, video/live sources and input transports remain separately scoped. |
| Scene images/cubemaps | Descriptor contract and native parser | Missing general owned asset binding beyond current user-image/cutout resources. A declared capability is not proof every scene uses it. |
| Four bands, each with level/hits/time/presence | Shipped shader/script references and audio docs | Partial: three band levels/hits/integrals. Add separate mid/high-mid and long-term presence semantics; do not claim instrument separation. |
| Spectrum/waveform/history textures | Audio Indicators; spectrum consumers | Missing. Provide opt-in textures and documented frequency/time mapping. Exact native FFT/DSP remains unknown. |
| Beat clocks and confidence | Native phase/sine/ramp evidence; scene scripts | Partial project beat clock is useful. Add explicit beat events/subdivisions and confidence policies; do not invent kick=downbeat. |
| Macro intensity/fade/song-change events | syn_Intensity/FadeInOut/PrimeBang consumers | Missing equivalent macro features. PrimeBang is not first-frame initialization; automatic selection semantics remain unresolved. |
| Stateful per-scene control logic | 47 scripts; Waterly/Geode/Thresholder | Partial generic audio controller. Add bounded scene-specific accumulators, triggers and state machines; arbitrary JS is not required to represent the capability. |
| Meaningful typed controls | 20 declared control-type strings, including null/spacers | Partial uniform 0–1 schema. Expose actual ranges, discrete choices, vectors, smoothing, impulses, counters and integrated controls as needed. |
| Smooth variation inside generation | 8 packages; Myoplasm structural use | Missing general variation contract. Interpolate compatible parameters while retaining state; distinguish colour-only families. |
| Hard variation/event choices | 18 packages; Pop, Quicksand | Partial discrete generic controls; requires explicit trigger policy and meaningful combinations. |
| Cross-scene picture blending | Native sceneMixer and FAQ | Present in Alloy. Existing dissolve/zoom/sweep/radial effects operate on completed pictures. |
| Cross-scene generation blending | User's explicit required experience | Missing in Alloy; arbitrary native scene-to-scene state transfer not established. Design compatible state/field morphs and incoming image/field seeding as distinct operations. |
| Reset, warm-up, pause and seek | Stateful sources and harness lifecycle | Partial; must explicitly define deterministic reset, warm-up and discontinuity handling. Do not silently erase simulation history on ordinary control edits. |
| MIDI/OSC/Spout/NDI and output routing | Documented host features; prior compatibility gaps | Not verified as equivalent in Alloy. Track independently from shader fidelity; do not promise them from a scene audit. |

Alloy source anchors: `scripts/original_visual_gl.py` Target allocation at94,
pass validation157–183, AudioController195–236 and pass bindings271–319;
`scripts/original_scene_player.py` PCM analysis48 onward;
`scripts/visual_compositor.py` completed-image blend37;
`scripts/generate_visual_library.py` reference-to-recipe table and family list;
`visuals/original/kernels/feedback.glsl` and `feedback-finish.glsl` for existing
colour-history feedback. These constraints are architectural, not naming issues.

## Transitions: evidence versus the requested experience

Three operations must remain distinct:

1. **Mixing finished outputs.** Installed native shader at `0xf1bbc0` samples
   scene A and B and linearly mixes them. REA xrefs include `0x748369`; the containing
   construction routine `0x747ad0` constructs sceneA, sceneB and sceneMixer stages.
   Evidence `ev_603b8fd59e4e0e8b64ab07724c0c7b46fed9c0a95496f95572871f6f22ae07b4`.
   This corroborates a crossfade path, not proof every native transition uses it.
2. **Changing generation parameters.** Descriptor smooth families supply shader
   values; some change colour only (Glob's c/b families), while Myoplasm's c1/c3
   affect its Voronoi-derived field at line63. Feedback may continue evolving
   during such changes. Native selection, interruption and exact easing policies
   are not fully recovered.
3. **Transferring or combining generation across different scenes.** No verified
   native universal state-transfer mechanism was found. Jay's requested experience
   is nevertheless an Alloy requirement. Related simulations can share a declared
   state interpretation; unrelated scenes can hand off an outgoing field/image
   as input to the incoming generator. Raw buffers with incompatible meanings
   must never be blindly interpolated. A distorted dissolve is still an output
   transition and must not be presented as generation blending.

The eight supplied screenshots establish image roles, not scene-switch motion.
Keep the optional transition example request open without blocking this audit.

## Controlled mechanism observations

Reused the earlier 12 Waterly/Geode/Turbo Fluid sequences and native media studies;
see [scene depth study](SCENE-DEPTH-STUDY-20261102.md). The earlier fluid probe
already showed image influence changing state with visible media colour disabled.

New bounded, serial GPU probes: two 90-frame Smoothlife sequences and three
single-frame Myoplasm cases, 240×135, below-normal process priority, no audio
device or running application. Unchanged installed sources were rendered through
the private compatibility harness. All five cases had no script errors/logs and
source hashes remained unchanged. Captured pixels were inspected.

* Smoothlife: both runs start with a black input; one receives a white ring only
  during frames10–29. At frame9 they match exactly. After returning to identical
  black inputs, differences persist: mean RGB-byte differences at44/59/89 are
  9.984/10.559/11.780. This demonstrates image-driven state with memory. It is not
  a quality or native-parity score; low resolution alters neighbourhood scale.
* Myoplasm: the c0→c1 generation midpoint differs from the arithmetic midpoint
  of the endpoint pictures by54.531 mean RGB bytes. The capture visibly changes
  cell extent instead of superimposing both endpoint patterns. Identical time,
  fixed audio uniforms and feedback=1 isolate this shader-parameter distinction.
  This does not establish a native transition trajectory or cross-scene morph.

Driver, exact source identities and report:
`.local/synesthesia-audit-10/probe_mechanisms.py`, `probes/report.json`,
`probes/comparison.png`. New tests were not a native regression suite, listening
session, performance benchmark or a replay of Jay's exact screenshot settings.

## Implementation order and observable acceptance

Do not perform another blanket motion/palette edit across all100 recipes.
Retain the existing distinct catalogue while replacing mechanisms in coherent
groups. Keep Jay's original logo and all reference captures unchanged.

1. **Image-driven flow and reaction group.** Build explicit current/previous
   resource bindings and signed state targets, then Vein Current and Microbial
   Mosaic as two distinct original systems. One should stretch and curl image
   pigment; the other should grow/erode cellular ridges from it. Separately prove
   image-to-state, display refraction and colour roles. Use the same logo as Jay.
2. **Feedback and particle group.** Posterised Mirage, Solar Wash and Firefly
   Assembly need selective replenishment, nonlinear colour evolution and stored
   particle dynamics respectively. Demonstrate retained image identity and
   evolving detail without making all three look like liquid refraction.
3. **Spatial material and optics group.** Topographic Grid, Isosurface Lantern
   and Wave Lens need different projection/shading contracts. Compare the same
   image as lit terrain/rods, sliced spatial projection and coupled optical warp.
4. **Generative transitions.** First demonstrate within-family parameter/state
   continuity, then outgoing-field/image injection into an incoming generator.
   Inspect the transition midpoint and subsequent evolution, not merely endpoints.
   State whether each pairing is a field morph, seeded handoff or output blend.
5. **Remaining mechanism groups.** Extend proven primitives to cellular growth,
   multiscale flow, sorting, volumes, reflection and fractals using the full index.
   Upgrade each distinct scene's controls/audio destinations, not only its label.

Each group needs moving captures with the same artwork and controlled120/140/160
BPM signals, then an authorised copy of real EDM. Check silence, sustained bass,
transients, breakdown/re-entry and independent band ablations. Band selectivity
is desirable: the reference Geode's high-hit response is evidence against forcing
every scene to visibly react to every band. Measure state effects separately from
brightness and assess motion visually/listening where possible. Preserve
frame-rate-independent timing and bounded numerical stability in new algorithms;
do not copy reference frame-dependent quirks merely for superficial similarity.

After each coherent group passes relevant focused checks and visual inspection,
update the external runtime in the main Alloy build with a hash receipt/rollback.
Do not restart Jay's app/session. Native rebuilds are needed only for actual native
contract changes; no automatic full gate, stress run or benchmark during this work.

## Remaining uncertainties and delivery status

Exact native FFT normalization/band boundaries, macro detectors, traveller/counter
envelopes, all callback coercions, pass/reset lifecycle, automatic variation policy,
video timing and direct native texture dimensions remain unresolved. Prior custom
diagnostic loading was blocked by the native licence restriction; no bypass was
attempted. Cubemap/transport declarations alone do not establish their full runtime
semantics. Arbitrary user-written scenes and future versions are outside this
finite installed-library inventory.

This audit delivers evidence, the full catalogue map and an implementation contract.
It does **not** deliver the missing capabilities. Main Alloy remains on update09;
its visual quality is not accepted by Jay. No production data, running session,
installed reference files, main runtime files or executable changed in this audit.

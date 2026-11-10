> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Cross-scene generative handoffs — update16

2 November 2026. Jay requested particles and images evolving into the next scene,
rather than only crossfading finished pictures.

## Behaviour and scope

Overlapping scene blocks now start a generative handoff automatically for any
non-Cut transition. Existing overlap length and pivot control its timing. Cut
remains an explicit cut. The old dissolve/zoom/radial/sweep labels no longer
select those finished-picture effects on this route; there is no new native UI
or saved-format change.

The 36 particle scenes, Vein Current, Microbial Mosaic, Posterised Mirage and
Solar Wash have explicit state conversions and generation/shading transitions:

- Particle to particle: copy captured source atlas, positions and velocities.
  Gradually introduce the incoming forces and interpolate fragment geometry,
  orientation and display colour. The outgoing finished picture does not enter
  their overlap display. In the focused fixture, initial incoming geometry and
  colour match the outgoing pixels exactly.
- Image/field to particles: capture outgoing material into the source atlas,
  initialise source cells and convert available motion to UV velocity. Tiles
  initially cover the image; gaps, rotation and incoming dynamics develop with
  the overlap. Original image fragments subsequently refresh under native rules.
- Into flow: explicitly convert image luminance into pigment/relief and available
  motion into bounded pixel velocity. Incoming flow evolves the material and its
  refraction/shading develops over the overlap.
- Into growth: convert luminance into valid nutrient/organism concentrations.
  Chemistry develops from those seeds; shading changes toward the growth surface.
- Into pigment feedback: captured colour becomes retained pigment/reservoir state.
  Incoming advection, pigment lifetime and display treatment evolve that state.

Compatible semantic buffers copy exactly. Unknown/incompatible channels are never
blindly mixed: RGBA colour is not interpreted as chemical concentrations or signed
velocity. Particle velocities are indexed by source material cells; their mapping
to a flow field is an artistic conversion, not an Eulerian reconstruction or
physical fluid simulation claim. Source state is not mutated.

Other scene families receive a persistent, transported image-material input through
their existing texture/material sampling. Their geometry is not transferred.
Those routes use a displaced fragment replacement presentation while the incoming
scene generates from inherited material. They are less integrated than the 40
explicit state destinations and still need artistic refinement; they are not
claimed to morph arbitrary geometry or incompatible simulation equations.

There is no Spout/video integration in this change. Recognition of the logo is
optional. The priority remains evolving structure and response to music.

## Ownership and lifecycle

`visual_handoff.py` owns GPU snapshots independently of the outgoing scene.
The outgoing instance can close at the end of the overlap while the incoming
simulation keeps its transferred state. A bounded image-material tail continues
on a 60-Hz clock, fades and releases its snapshot buffers after 12 seconds.
Transferred simulation state remains owned by the destination. Snapshot bytes
are included in the existing allocation facts.

Pause freezes simulation and inherited material. Seeks, generation changes and
revision edits invalidate stale handoffs. A seek into an overlap reconstructs
from the outgoing scene's freshly reset state; it does not replay the full prior
history or promise the same state as uninterrupted playback. Chained A→B→C
handoffs inherit B's evolved state. Empty timeline gaps free scene resources.

The native protocol is unchanged. Facts add `generativeHandoffs`, including each
destination, the actual conversion routes and current material strength. CMake
includes the new runtime module in future packaging. Scene contract metadata and
the particle generator now document explicit transfers.

## Evidence and limitations

Independent evidence/checkpoints: `.local/scene-handoff-16/`.

- `gpu-02`: 229/0. Eight representative six-second routes cover compatible
  particles, particles→flow, flow→growth, growth→pigment, pigment→wash,
  wash→particles and both directions between a geometry scene and particles.
  Checks include exact particle copying, finite state, pause, post-overlap
  differences against an unseeded control, outgoing release, reset/revision/gap
  handling and shader compilation by catalogue family.
- `bundled-lifecycle`: 18/0 against an isolated runtime assembled from the
  installed bundle and candidate files. Checks source immutability, exact state
  copying, a zero-mean-error particle start seam, snapshot survival, tail cleanup,
  chained handoffs, Cut and boundary/nondefault pivots. Three six-second real-PCM
  sequences are retained. End/drop frame differences remain within each captured
  sequence's normal motion range; this is not a frame-rate or smoothness guarantee.
- `bundled-materials`: 44/0 after repairing the separate LOD material-sampling
  path used by older scene families. Twenty-two family representatives show
  inherited material affecting generation after the overlap, with no GL errors.
  The prior lifecycle routes do not use this changed LOD path.
- Unit checks: 11/0, including conversion selection and pivot/endpoints.
- Activation fixture: active-app guard, predecessor enforcement, corrupted-payload
  rejection, exact installation and idempotence. The real guard left active
  runtime files untouched. Nine protected reference hashes verified.

`gpu-01` also passed technical checks but its universal pixel-replacement
presentation was rejected on visual inspection. `gpu-02` replaces that overlay
for the 40 state destinations with their own generation/shading transitions.
Subsequent bundled checks verify particle seam colour/geometry and pivot refinements.
Sampled temporal frames from synthetic and real-PCM sequences were inspected.
The legacy geometry/material route remains more visibly a fragment replacement
and is not equivalent to the particle-to-particle morph.

The real PCM is the existing authorised copy, source seconds60–66.1, decoded in
memory and not retained as new audio. The clock was140 BPM; actual track-grid
alignment and listening acceptance were not established. State persistence and
technical passes do not establish Jay's musical/artistic acceptance or exhaustive
coverage of all127×127 scene pairings. Longer musical overlaps remain unreviewed.

All checks were serial and below normal priority. No native build/restart,
native session or physical gestures, full regression, stress or benchmark run.

## Local activation

Updates14/15 were confirmed installed at the start of this task. **Update16 is
staged, not active**:218 hashed runtime/descriptor files under
`out/releases/0.55.67-4258046694c2/scene-update-16-payload`, including40 metadata-only
transfer-contract updates. The normal launcher applies16 after Alloy/worker close
and requires installed15. Direct EXE launch bypasses this chain. No watcher or
automatic restart. Read `scene-update-16.json` before asserting activation.

Rollback/source archives, before/after hashes, installer, receipts and moving
captures are retained independently of Git. Executable SHA256 remains
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
No user music, sessions, analysis, corrections, caches or protected references
were changed. This is a local review candidate, not a public release.

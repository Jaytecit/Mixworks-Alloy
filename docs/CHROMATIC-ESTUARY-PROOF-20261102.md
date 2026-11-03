# Chromatic Estuary — second visual direction proof

## Correction after Jay's playback review

Jay reported two concrete shortcomings: the outgoing scene stopped reacting
during the join, and the response still looked like a back-and-forth twitch.
The previous evidence below did not establish acceptable musical movement.

Current candidate: `.local/estuary-live/candidate`. Current soundtrack preview:
`.local/estuary-live/preview/preview.mp4` (same 26 seconds and authored overlap).
The player now refreshes the owned outgoing appearance after the outgoing scene
renders each overlap frame. Simulation seed snapshots remain separate. This
preserves live outgoing deformation/audio response until the reveal completes,
without retaining an unsafe reference to a released scene.

Estuary previously added current velocity directly to surface position, so an
attack pushed it and the release pulled it back. Surface travel now integrates
bass/mid energy, while nested rotation and fine image travel accumulate mid and
treble energy. The old sine-based inlay rocking was removed. Existing advected
flow/pigment remain; this is a targeted response correction, not a new solver.
The reveal front is bounded so its endpoints remain fully outgoing/incoming.

Eight capture checks passed: six sampled outgoing textures match that same
frame's live source exactly and all six differ from each other; join start,
pause, finite state and resource release also pass. Fourteen bounded probes
include non-reversing accumulated travel retained as the bass envelope decays.
Eleven focused unit tests passed in 0.361 seconds. Offline median render6.54ms,
p959.11ms; encoder/backpressure excluded. Sampled pixels inspected. Native
verification is isolated in `.local/est2`, separate from the earlier run below.
No full suite, C++ rebuild, production installation or data reset.

The corrected native preview saved/reopened, saved as `live-correction-final`,
and closed gracefully. In 24.18 measured seconds it delivered 1,452 window swaps
(about 60/s), median gap17ms, p9521ms and maximum40ms. An initial131ms gap is
retained in the full-run evidence rather than hidden by the steady interval.
Monitor muted; no listening or physical scanout acceptance. Inspected native
pixels: `.local/est2/evidence/corrected-playing.png`. Before/completion source and
identities are retained in `.local/estuary-live` independently of Git.

### Earlier proof and its retained evidence

Jay asked to continue with vibrant variation, substantial reactive movement,
many uses of nested imagery and strong transitions across the eventual 100
scenes. Resonant Vault's approved direction and requested far-to-near pressure
wave remain intact. This proof contrasts Vault with a lateral liquid composition.

## Preview and recipe

`.local/estuary/join-03/preview.mp4`: 26 seconds at 1280×720, 60 encoded fps, with
the authorised soundtrack excerpt at source seconds 112–138. The scene overlap
is authored at preview seconds 4–9. Audio responses have no authored section cues.
The late portion includes the quieter passage. This is ready for visual/musical
judgement, not a claim that Jay has accepted Estuary or the transition.

Estuary uses four passes: a 384×216 signed velocity field with broad inertial
currents; shared material-coordinate transport; an advected RGB pigment field;
and lit/refracted enamel with artwork contours and a transformed nested inlay.
Bass adds momentum, mids alter shear and damping, and treble adds smaller forces
and replenishes pigment. Lighting is independent of audio amplitude. This is
stylised advection, not a physically complete fluid solver. The inlay and its
parent chart demonstrate two levels, not an arbitrary scene-graph editor.

The first render was too uniformly crinkled. It remains in `early-01` and
`candidate-01`. A new broad current field replaced the fine shared flow;
`early-02` demonstrates the change. Material-coordinate transport, audio controls,
state contracts and lifecycle remain shared. The candidate adds an optional
full-resolution outgoing appearance sampler to the renderer; existing shaders
do not consume it.

The join captures Vault's appearance, deforms it with Estuary's current field,
and reveals incoming material along an uneven front. Outgoing image gradients
seed incoming velocity through the existing explicit semantic adapter. It is a
single pair's material transition, with an initial captured outgoing appearance,
not continuous outgoing geometry or a general all-pairs transition solution.
Future scene pairs should use different appropriate mechanisms.

## Evidence and limits

Frozen runtime/source identity: `.local/estuary/candidate-03` and its
`candidate-hashes.json`/`source-checkpoint.zip`. Before/completion source and
independent identities live in `.local/estuary`. No production catalogue entries,
launcher installation or user data were changed.

- Six capture checks passed: exact outgoing appearance at join start (maximum
  channel error 0), exact pause, outgoing/handoff resource release, and three
  finite state buffers. Sampled pixels were inspected.
- Twelve bounded GPU probes passed: finite state, continuous silent motion,
  pause, deterministic backward seek, distinct band-driven velocity, alternate
  artwork participation and reset. Mean speed in the synthetic bass case was
  25.04 versus 13.36 pixels/second in silence; mids and treble changed the actual
  velocity field. These measurements support the mechanism, not musical taste.
- Eleven focused runtime unit tests passed in 0.574 seconds. No automatic full
  regression, stress or C++ rebuild was run.
- Offline render median 7.20 ms, p95 11.84 ms on RTX 4060 Ti. This excludes encoder
  backpressure and is not physical playback evidence.
- Native `.local/est1` used trusted executable 0.55.67, SHA-256
  `4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
  Protocol/root/PID/capabilities checked; both scenes and the handoff appeared in
  live frame facts. Inspected native Estuary pixels are retained as
  `evidence/estuary-playing.png`. In 24.13 measured seconds: 1,449 window swaps
  (about 60/s), median gap 18 ms, p95 20 ms, maximum 40 ms. Monitor muted; this
  establishes neither listening acceptance nor physical display scanout.
- Native session saved/reopened successfully, saved as `estuary-final`, then
  closed gracefully. The owned 26-second audio copy is retained deliberately
  for reopening this isolated preview; original music remains untouched.

Remaining judgement: whether the liquid movement, image richness and join have
the desired excitement. Two proofs do not establish catalogue-wide variety.
Use the diversity ledger in [visual direction](VISUAL-DIRECTION.md) to guide
the next spatially distinct scene and avoid expanding through palette presets.

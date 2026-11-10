> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Flow and reaction scene group — 2 November 2026

Implements the first group from the retained Synesthesia technique audit. Only
Vein Current and Microbial Mosaic are rebuilt. The other 98 scenes and their
musical/visual limitations remain; update09 is not retrospectively accepted.

## Implemented mechanisms

Vein Current stores signed velocity, pigment and relief in RGBA32F. Semi-Lagrangian
advection, neighbourhood smoothing, local curl forces, drag and image-edge forces
evolve that field. A second retained target transports material coordinates.
This is an original forced flow system, not a claim of a pressure-projected,
incompressible fluid solver. Bass sustains circulation and adds transient momentum;
mids change image-edge circulation/drag; high attacks add local curl from relief.
The display derives normals from state and separately refracts/colours artwork.
It does not read projectBeat or the common lowSwing/midSwing/highSwing functions.

Microbial Mosaic evolves nutrient and organism concentrations with Gray–Scott
reaction, a nine-sample Laplacian, feed and kill terms. Six numerical updates run
per fixed tick. Image luminance seeds and feeds concentrations. Bass affects
survival and local erosion, mids replenish nutrients, and high attacks feed
image-defined regions. Colour and surface normals derive from concentrations;
the display shader does not sample the artwork. Growth continues between hits.

Existing native control keys are retained: `feedback` controls image feeding;
Vein's `warp` controls optical refraction and `imageMix` its image colour amount.
Microbial's `imageMix` changes the ridge palette, not a direct image overlay.
The descriptor records these semantics; this does not add a new native control UI.

## Renderer contract and ownership

`passSchema: 2` declares each input as `{pass, frame: current|previous}`. Current
edges must point backward in the ordered graph; previous edges require retained
targets and are resolved from a snapshot before any pass in that tick writes.
State passes precede display; state cannot depend on display history. Display
reads completed current state. State targets require a semantic identity and
signed storage. RGBA8, RGBA16F and RGBA32F formats and linear/nearest filtering
are explicit. Fixed targets are bounded to 512 in either dimension, at most 12
passes, four inputs per pass and 128 MiB target allocation per scene.

These two simulations use 384×216 state independent of output size and run at
60 Hz, with at most 18 catch-up ticks. Paused control changes redraw without
advancing simulation. Backward/large seeks, revision and generation changes reset
state deterministically. A seek starts a new simulation, not a reconstruction of
the entire preceding song. Held-input states match exactly at 30/60 Hz; audio
envelopes still depend on the supplied PCM sampling cadence. The finite simulation
grid can look softer at 1080p; no sustained performance guarantee is made.

Each clip owns its Scene and its typed state; a scene-key change destroys that
instance. Velocity/relief, material coordinates and chemical concentrations are
never blended or transferred implicitly. Future generation transitions need
explicit compatible morphs or field-to-seed adapters. The existing compositor
still blends finished outputs. Its tested overlap is **not a generative transition**.

## Visual development and bounded evidence

Evidence: `.local/scene-flow-11/`, independent of Git. Primary input is the
preserved Martooni image, SHA256
`52be27bc0ac94ab4778812e7d0398659a5b6c92e4532b06391edf9da5e92227e`.
The eight reference screenshots/manifest and original music remain unchanged.
No proprietary shader, texture or helper was copied into the runtime. Method
discovery rechecked the skills.sh shader-dev listing; the existing local GL
renderer and retained audit supplied the relevant implementation contract.

The first exploratory Microbial capture lost most population after energy fell;
feed/kill and seed strength were repaired. A stronger Vein transport experiment
consumed too much of the logo and was reduced. Jay said the original Vein capture
did not look too bad; the final direction preserves its readable liquid relief.
`preview-01`/`preview-02` and the stronger experiment's source snapshot are retained.

Final evidence, run serially below normal priority:

- Unit/controller/descriptor checks: 8/0.
- Focused source GPU checks: 136/0. Includes signed-value arithmetic and old/current
  buffer oracle, same-140-BPM sustained/isolated/silent PCM, post-hit state memory,
  image removal with colour disabled, fixed-tick equality, pause/control/reset
  behaviour, two independent instances and two existing scene smoke checks.
- Isolated bundled-runtime candidate: 22/0, including both scene graphs and their
  output-only overlap. Actual GPU shader compilation is the runtime build here;
  no native contract changed and no native rebuild was required.
- Independent visible colour/refraction with unchanged state and short maximum
  input/control probes: 3/0.
- Safe activation tests: 5/0, including an active-session guard, hash rejection,
  injected partial-write rollback, clean activation and idempotence.
- Two eight-second real-PCM visual captures from the previously authorised track
  copy, source seconds 60–68.1. Decoded PCM existed only in memory. The 140-BPM
  clock is authored; track-grid alignment and musical listening were not verified.

The 120/160-BPM captures hold the isolated-hit schedule in physical seconds;
they exercise clock independence, not a tempo-aligned live mix audition. The
140-BPM comparisons use different audio content at the same clock. Captured
sequences show Vein folds/shear retaining momentum, while Microbial expands
connected ridges under sustained input and splits into smaller islands after
release. These observations, not pixel differences alone, support this group as
an improvement. Short 640×360 and 960×540 sequences were inspected; moving WebPs,
temporal sheets, state metrics and real-PCM frames are retained. Final artistic
acceptance remains Jay's. Fine-edge shimmer, broad Vein folds and the limited
chemical palette remain visible limitations.

The first GPU harness attempt failed before imports because isolated bundled
Python lacked the scripts path. `gpu-01/failure.txt` retains the failure; `gpu-02`
is the corrected run. No full regression, stress, benchmark, device test, native
session manipulation or public release occurred.

## Main-build delivery

Main executable remains `out/releases/0.55.67-4258046694c2/Mixworks.exe`, SHA256
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
Nine changed runtime files are hash-verified in `scene-update-11-payload` beside
it. `scene-update-11.json` records the payload and current activation status.

At staging, main PID32944 and its old visual worker PID3476 were running. Replacing
the live descriptors with schema 2 could break that already-imported old renderer.
Therefore **the live runtime remains update09**. `out/Launch.cmd` now runs the
hash-checked installer before the normal launch. It does nothing while the main
app/worker is running; after Jay closes Alloy normally, the next usual shortcut
launch installs all nine files and then starts Alloy. No watcher, termination,
session command or automatic restart was added. Directly launching Mixworks.exe
bypasses this one-time installer; use the usual shortcut/Mixworks.cmd.

Rollback ZIPs are in the release and `.local/scene-flow-11/`, with old runtime
files, package manifest and launcher. New-file identities, before/after hashes,
candidate/source snapshots and evidence are retained. Activation and native reload
are pending until the normal launcher runs with no active app/worker. Inspect
`scene-update-11.json` for the actual status rather than assuming staging is activation.

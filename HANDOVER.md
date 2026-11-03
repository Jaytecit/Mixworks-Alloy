# Mixworks current handover — 3 November 2026

## Current milestone

Jay requested translucent mosaic scenes that refract a complete image behind
moving tiles, followed by an obsolete-document/test audit, a new local app version,
launcher activation and a source commit pushed to Jaytecit/Mixworks-Alloy.
Version **0.55.68** is built and verified. The local launcher selects
`out/releases/0.55.68-110a215dcfdd`; executable SHA-256 is
`110a215dcfdd16fb8cc4ba82dfcbc086c1e3ccedd7fc00fc2e4323052f29cd8d`.
Evidence and the activation receipt are under `.local/glass-release-20261103`.
The previous `0.55.67-60c6494debe5` bundle remains available. Existing production
processes were not restarted; the updated launcher applies on the next launch.

The active library has **123 scenes / 738 named looks**. All 61 retained choices
remain in the active catalogue; retired registries and source remain intact.
Mosaic Kiln and Spinning Stamps offer a **Glass image** look. Rhombus Foundry and
Enamel Scales use continuous-image refraction by default. Their tiles distort
one image that remains visible between tiles. The opaque tiled looks remain.
See [visual direction](docs/VISUAL-DIRECTION.md) and
[maintenance/activation record](docs/MAINTENANCE-20261103.md).

## Accepted visual direction

Movement must be the main musical response, not only brightness or colour.
Particle trajectories must respond to music and periodically reassemble into
one image. Use more solid/geometric compositions in the direction of Beats in 4D;
automatic shows allow at most one particle slot in five when alternatives exist.
Whole images should often remain readable, pump to the beat and receive different
scene effects. Mosaic tiles may each carry a complete image, or refract a complete
image behind them. Fragmentation and nested imagery are optional treatments.
Quieter scenes benefit from kaleidoscopes. Strong cycling and beat-driven
recolouring applies to images and scene geometry. Extended track joins use live
blends; fragment/reveal handoffs are reserved for short transitions. Artistic
acceptance remains Jay's, independent of numerical tests.

## Verification and continuity

The 0.55.68 single-worker build passed. Focused checks passed: 17 visual runtime
unit tests, 45 native renderer checks, 47 native arrangement checks, 41 authored
GPU behaviour checks and 16 selected preset/method checks. Both retired selectors
returned the expected refusal, before suite setup. Current document/CMake links
passed. Native reports used the shared retention harness; no stress/full gate ran.
The 24-second glass preview and native browser pixels were inspected. Isolated
`.local/gla1` opened, saved, reopened and gracefully closed the four-scene fixture
with playback stopped. Its owned excerpt/images are deliberately retained for
review. Production media folder settings were copied to the new local bundle.

The preceding motion checkpoint is `.local/motion-20261103/candidate-04`, executable
`e8384e406ba4083452d237003bfde7289a202d91d64c893571c01c12829b5cbf`.
It passed 14 runtime unit tests, bounded GPU checks and 45 native checks through
`verification.run_checks`. Its soundtrack movies show seven scenes and long live
blends. See [motion record](docs/VISUAL-MOTION-20261103.md). The isolated
`.local/mot1` review was saved and closed without starting playback.

The current glass/audit checkpoint preserves the entire previous publishable
worktree in `.local/glass-release-20261103/before-source.zip`. The removal plan
contains original paths, SHA-256 hashes and reasons. Obsolete duplicate documents,
external-renderer tests and one-off scene proof/generator scripts were removed
only after verifying their archived bytes. Original failed evidence, historical
receipts, user requirements and licensing/provenance records remain available.

Maintain `scripts/stage_visual_runtime.py`, `scripts/verify_visual_behaviour.py`,
`scripts/verify_scene_looks.py`, the three catalogue preview renderers and runtime
unit tests. Native `--original-visual-only` covers the actual owned renderer;
`--visual-arrangement-only` uses current catalogue fixtures and current save
precision. Removed external-renderer selectors must not be reintroduced.

Builds use one worker, `/MP1`, below-normal priority and no numeric RAM guard.
Tests run serially through the retention harness. Use essential focused checks
under Jay's responsiveness override; no automatic stress/capacity/full regression.
A local launcher update is not a public package or complete-regression claim.

## Protected inputs and running work

Use this folder's Git repository, not the parent REA repository. `.local/`, build,
out, music, sessions, caches and original user artwork remain outside commits.
Never stop/restart a running production instance just to activate a future-launch
bundle. Preserve AppData, analysis, corrections, audio caches, library/recovery,
original music, and protected DownbeatReferences-v1 plus independent dated baselines.
This visual/maintenance update requires no detector reset or session migration.

The 64 extracted transparent sprite PNGs remain in `visuals/original/Sprites`;
source/crop/hash evidence is `.local/sprite-extraction-20261103`. Main app image
folder settings must be copied when staging a new release. Never import isolated
review retention checkboxes into production settings.

Use the [development bridge](docs/DEVELOPMENT-BRIDGE.md) with a fresh short owned
`.local` root, verify protocol/root/PID/capabilities, wait for asynchronous outcomes,
save before close, and inspect pixels. Do not retry mutations after timeouts
without inspecting their actual outcome.

## Other work and limitations

Constant-tempo 4/4 BPM/pulse/downbeat alignment remains the music priority.
The source A/B/C correction workflow and source-save semantics are described in
[Source grid alignment](docs/SOURCE-GRID-ALIGNMENT.md); protected manual evidence
is documented in [Downbeat references](docs/DOWNBEAT-REFERENCES.md).
Prior audio/grid/export/readiness results are dated evidence in
`docs/release-audit/`; they do not establish current complete acceptance.
Hardware/ASIO/FLkey/private-output listening, independent detector accuracy,
sustained performance and clean-environment distribution remain unqualified.

Custom libraries/crates were explicitly deferred on 2 November: plan later,
implement later. Preserve the full P01–P56 backlog in [the roadmap](FINISHING-PLAN.md)
and original requirement detail in the retained history files. Manual/help work
comes after workflows settle. Do not resume old bulk-recovery or detector-reset
instructions from historical reports.

The [Personal Beta Licence](LICENSE) covers original work; third-party terms remain.
Public binaries are blocked by [licensing prerequisites](docs/LICENSING.md).
Source commits and local technical checks do not establish distribution clearance.

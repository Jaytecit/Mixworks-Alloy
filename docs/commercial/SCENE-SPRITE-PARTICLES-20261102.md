# Sprite-driven particle scenes — update15

2 November 2026. Jay requested images breaking apart and evolving with music,
with their own behaviour rather than additional wash, ideally 30–50 nested
sprite interactions. Implemented **36 selectable behaviours in nine families**.
Nine existing scenes were rebuilt and 27 named variants added: 127 catalogue
choices total. The four earlier flow/feedback scenes are unchanged.

## Finding the behaviours

Search the Visuals scene browser by family. The original family title selects A;
B/C/D append the behaviour name to the title. These are directly selectable
scenes because the current native browser has no manual mode editor. A bounded
four-step `mode` control also remains available to authored automation. Current
native automatic variation stays inside each named default's quantisation cell.

| Family / scene search | A (base title) | B | C | D |
| --- | --- | --- | --- | --- |
| Firefly Assembly | Radial fracture | Edge peel | Colour separation | Scatter and recall |
| Magnetic Filaments | Dipole capture | Contour runners | Alternating poles | Magnetic comb |
| Plasma Seeds | Vortex seeds | Paired cyclones | Cell orbitals | Colour convection |
| Light Loom | Woven strips | Zipper opening | Accordion pleats | Diagonal braid |
| Synaptic Web | Elastic mesh | Travelling rupture | Neighbour alignment | Cluster exchange |
| Orbital Etching | Kepler shells | Rosette tracks | Binary satellites | Precessing ellipses |
| Crimson Threads | Falling ribbons | Updraft fibres | Pendulum strands | Edge streamers |
| Overdrive Rings | Shock shells | Spiral conveyor | Gear teeth | Ring exchange |
| Star Nursery | Supernova return | Gravity wells | Comet tails | Accretion disc |

These describe artistic force systems, not claims of astrophysical fidelity.
Use new scene blocks to evaluate the new contracts; older particle blocks and
their captured control schemas are not a compatibility target.

## Mechanism

Each scene stores 1,344 fragment positions and velocities in a nearest-filtered
48×28 RGBA32F texture. A separate 512×288 premultiplied source atlas retains image
material. Original colour, alpha and local edges determine forces, grouping and
fragment appearance. Each fragment samples its own part of the atlas: there is
no intact image overlay. Source refresh can close while captured fragments keep
moving. A procedural rosette supplies material when the scene starts without
an image; this is a fallback, not a user asset.

Nine equation groups provide four different force/constraint systems each.
Synaptic modes couple actual neighbouring position/velocity state. Bass attacks
provide impulses, sustained bands change constraints/field strength, and treble
attacks articulate source edges. Momentum and springs continue between hits.
This is not a common beat-clock reversal or a list of recoloured presets.

The existing 60-Hz simulation lifecycle, previous/current edges, reset identity
and allocation accounting are retained. Renderer schema 2 gains a bounded,
scene-local vertex shader and instanced display draw (maximum 4,096 instances).
Display clears each frame and composites premultiplied fragments; it does not
accumulate display trails or bloom. Blend state is disabled after the draw.
Position boundaries reflect fragments; speed is capped. Atlas/particle buffers
belong to each scene instance. Cross-scene overlaps still blend final pictures;
they do not transfer these states or establish generative handoffs.

`generate_particle_scenes.py` owns the nine families and named variants. The
main library generator preserves these authored scenes and recreates the variants.
The Python catalogue bound is now 160; native discovery was already dynamic.
Native catalogue test expectations were updated to 127; that native suite was
not rebuilt or run in this runtime-only checkpoint.

## Verification and visible evidence

Independent evidence: `.local/scene-particles-15/`.

- `gpu-02`: 268 checks, all passing. All 36 modes at the same 140-BPM clock with
  music-shaped PCM versus silence; finite signed state, distinct trajectories,
  exact pause, captured source hold/removal, fixed-step equality, deterministic
  reset, transparent-RGB rejection, opaque-image feed and GL state cleanup.
  Nine animated sequences, mode sheets and a 36-mode overview are retained.
- Visual inspection found Magnetic / Contour runners pushed too many fragments
  against the boundaries. Reduced contour forcing and strengthened its return
  spring. `magnetic-repair`: 36/0 focused checks on the repaired family. The old
  captures remain as evidence; its newer mode sheet is the accepted version.
- `bundled-final-02`: 90/0 using an isolated candidate assembled from the actual
  bundled renderer modules plus the two updated modules and source assets.
  All 36 named defaults matched their tested mode's exact pixels/state; all nine
  families distinguished equal-level bass/mid/treble attacks. Includes 120/160-BPM
  short runs, three six-second real-PCM captures and a particle/Vein overlap.
- `bundled-magnetic-final`: 16/0 after the magnetic repair, including its four
  named defaults and a further real-PCM capture. Other family equations were
  unchanged by that repair. The 90-check run remains prior-candidate evidence.
- Focused Python unit suite: 9/0, including rejection of unbounded particle
  counts, invalid vertex paths, state/history instancing and incomplete fields.
- Activation fixture: 5/0 (active-session guard, predecessor requirement,
  preflight hash rejection, exact installation including new files, idempotence).
  The real guard returned pending and preserved every installed target hash.
  All nine protected reference hashes were verified.

Initial attempts are retained: `gpu-01` did not start because embedded Python
requires an explicit script import path. `bundled-final` failed a band-selectivity
fixture that began with an already-sustained tone, producing no attack; the
corrected fixture explicitly starts silent before the equal-level tone. That
fixture correction did not change the dynamics implementation.

Inspected all mode stills and sampled temporal frames, including real PCM for
Firefly/Orbital and the repaired magnetic family. Fracture, strips, clusters and
orbits remain separated image material rather than accumulating wash. Firefly
and Star Nursery share a radial-break starting character; the orbital variants
also share a family resemblance. Thirty-six distinct equations/choices do not
claim 36 unrelated visual aesthetics. Musical/artistic acceptance remains Jay's.

The real PCM is seconds 60–66.1 of the existing authorised local audio copy,
decoded in memory only. No listening or track-grid alignment was established.
Runs were serial and below normal priority. No native app was launched/restarted,
native build/full regression, stress, benchmark or long-duration guarantee.
The source atlas uses the native visual output's current 16:9 framing convention.

## Local activation

210 hash-identified files are staged under
`out/releases/0.55.67-4258046694c2/scene-update-15-payload`. Update14 was still
pending at staging time. The normal launcher applies update14 then update15
after Alloy and its worker close; update15 explicitly requires installed14.
Direct EXE launch bypasses this chain. No automatic restart or watcher.

Read `scene-update-15.json` before asserting activation. Source checkpoints,
before/after hashes, installer, staged receipt and rollback ZIP are retained
independently of Git. Native executable remains
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
No original music, sessions, analysis, corrections, caches or protected references
were modified. This is a local review candidate, not a public release.

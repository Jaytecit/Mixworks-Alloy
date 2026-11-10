> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Retained scenes: first refinement, 3 November 2026

Jay selected 61 of the 127 installed entries and authorised retiring the others.
His exact shortlist is preserved in `visuals/original/retained-scenes.json` and
`.local/retained-20261103/user-shortlist.json`. The active catalogue contains those
61 plus the three focused direction scenes. The 66 unselected entries are recorded
in `retired-scenes.json`; their source folders remain available for reference.

## Applied behaviour

The shared controller already supplies 3.5× bounded attack envelopes and separate
accumulated bass/mid/treble impulse phases. This runtime brings those into the
retained catalogue. Analytic shaders replace beat-clock reversal with accumulated
phase travel; separate bands move structures, material inlays and fine detail.
Particle simulations receive stronger forces and retain position/velocity;
fragment turns continue after attacks. Comet tails have bounded dispersion and a
stronger restoring force to keep their group in the composition.

Surface materials now contain independently moving child charts and a nested
grandchild, with flowing, shearing or turning image domains, refracted inlays and
local rim accents. They inherit the parent's surface coordinate system. Particle
source capture also includes nested material before fragmentation. Existing Solar
Wash and the three focused scenes retain their specialised stateful image systems.
This establishes nested behaviour; it does not establish that every image/theme
combination is artistically successful.

The 28 retained particle entries have nine animated settings:

| Family | Setting and supporting effects |
| --- | --- |
| Firefly Assembly | Receding mineral terraces and peeling sparks |
| Magnetic Filaments | Electrode towers and travelling curved field lines |
| Plasma Seeds | Paired vortices in flowing vapour |
| Light Loom | Woven ribbon canopy and travelling stitches |
| Synaptic Web | Cellular circuit floor and illuminated junction pulses |
| Orbital Etching | Tilted orbital instruments and planetary horizon |
| Crimson Threads | Layered silk curtains and rising ember trails |
| Overdrive Rings | Toothed concentric rotor deck |
| Star Nursery | Stellar clouds, accretion sheet and moving motes |

Variants alter palette, geometry parameters, motion mode and composition.
Mirror/kaleidoscope settings are selective defaults and editable controls. The
kaleidoscope gathers every angular source sector so an asymmetric moving group
cannot disappear merely because it leaves one sampled wedge. These settings are
procedural scenery, not imported 3D environments.

Colonnade columns now stand beside the route, lattice geometry includes an open
service passage, skyline cameras travel above rooftops, and camera deviations
are bounded separately from the structure's music response. Neon Run and Traced
Arcade advance through open apertures without the old bass-driven reversal.
Cathedral camera banking is restrained while its windows and structure respond.

Fallback transitions select stable scene-pair fragment, ribbon, radial or petal
choreography. Particle scenery is introduced spatially while the live outgoing
appearance continues updating. Typed simulation handoffs remain in place.

## Review artifacts and verification

- Final frozen runtime: `.local/retained-20261103/candidate-04`.
- Independent before source ZIP, migration record, calibration candidates,
  retained-list snapshot and final source/evidence identities remain under
  `.local/retained-20261103`; not dependent on Git status.
- All 64 active scenes compiled and rendered reference images with the same
  authorised background/cutout artwork and music excerpt. Final affected particle
  images were recaptured after correcting sector coverage and comet dispersion.
- A 58-second soundtrack tour at 960×540/30 encoded fps covers 14 scenes and all
  nine particle settings. Authored clip timings, unmodified mixed music112–170s:
  `.local/retained-20261103/tour-final/retained-scenes.mp4`.
- Tour checks compare the live outgoing texture with the handoff appearance
  throughout each overlap and assert exact paused output. These are mechanism
  checks, not artistic or listening acceptance.
- Six actual shader-camera probes run 240 maximum/alternating band samples each:
  colonnade, furnace, lattice, two skylines and cathedral. Minimum sampled signed
  clearance is positive in all cases (smallest .1905 for the lattice). These use
  the actual camera expressions and geometry field, not brightness comparisons.
  Probe time range is 30 seconds at default scene controls; no exhaustive
  arbitrary-control or long-duration guarantee is claimed.
- Native isolated review: `.local/ret1`, with all 61 retained checkmarks copied,
  updated 64-image library and a saved 58-second tour arrangement. Existing UI
  executable is reused unchanged; shader/runtime assets are the updated candidate.
  No audio/DSP, detector, saved-format or native UI source changed this turn.
- Native evidence contains 43 valid snapshots over positions13.77–58s, about
  59.82 GPU window swaps/s at720p (not monitor scanout). Maximum reported gap,
  including startup,116.44ms. Verification was monitor-muted. The library and
  live particle/material frames were inspected; arrangement saved as
  `retained-preview-verified`. The review window is left available to Jay.
- Final soundtrack tour passed52 exact live-outgoing texture comparisons and
  exact pause preservation. Offline median/p95 render time10.91/18.55ms at540p;
  encoded30fps does not itself prove native delivery or musical quality.
- Initial missing Python dependency, four shader/descriptor failures, and camera
  harness corrections are retained as earlier evidence; final successful captures
  supersede them within this scope. No full native regression, stress run,
  hardware audio test or user artistic acceptance is implied.

## Proposed next extension: named looks

Jay asked whether settings or several JSON files could multiply variety. Yes:
implement explicit named presets grouped under the parent scene, sharing shaders.
Useful independent controls include band reaction strength, momentum, material
behaviour, scenery, glow, symmetry and camera travel. Do not silently interpret
prose in `audioMapping` or `motionProfile` as executable behaviour: these currently
document shader mappings. New control names and ranges require validator, UI and
uniform support. Feedback needs explicit previous-frame edges; supersampling
scale2 is not currently accepted. This preset loader is proposed, not implemented
in the present refinement.

> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Original nested scenes — first playable proof

2 November 2026. Implemented and inspected; **rejected by Jay as a quality regression**.

Historical experiment record. [Alloy visual direction](../VISUAL-DIRECTION.md)
owns the current goal and one-scene milestone; this report does not set the plan.

## User verdict — supersedes the initial visual assessment

Jay reports that this is nowhere close to Synesthesia or even most existing Alloy
scenes: "a few floating bits on a black screen that barely move in response to the
music". This is rejection of the overall composition and musical behaviour,
not an isolated request to reduce Bloom's busyness. The intended proof failed.

The technical results below remain factual, but do not justify presenting this
as progress in visual quality. Sparse geometric fragments, excessive empty space
and subdued musical deformation are the central defects. The numerical audio
response check proved only that PCM affected state, not a convincing response.
The first scene should have been held against the quality benchmark before
expanding the collection. Keep this candidate as a rejected experiment; no
installation or fourth-scene work. Reassess the mechanism and establish one
compelling matched-input moving scene before continuing the sequence.

The three-scene minimum now plays through Ribbon Loom → Coil → Orbital Bloom →
Unfurl → Prism Fold. A 70.83-second recording with music is available at
`.local/original-scenes-poc/sequence-01/sequence.mp4`. The native saved preview is
`.local/poc1/sessions/poc-preview-final.mixworks`, with its isolated bundle and
owned media alongside it. Nothing was installed into the normal launcher.

## What was built

Eight persistent parent groups carry 384 identified children. Parent transforms,
velocities and integrated musical phases live separately from child-local
displacement/velocity. Vertex geometry composes the local children through their
parent; the captured Martooni artwork supplies material inside the geometry.
Large and smaller forms share the material without an intact image overlay.

Ribbon Loom bends broad bands with travelling local disturbances. Orbital Bloom
opens material into petals and independently moving satellites. Prism Fold folds
tile-bearing panels in shallow perspective. Coil closes bands into loops before
opening petals; Unfurl aligns existing fragments into hinges. Orientation and
size interpolate separately during joins to avoid collapsing a rotating tile.
The same parent, child and captured-atlas semantics copy exactly into independently
owned incoming state. The initial join pixels match the outgoing scene exactly.

The shared runtime change adds only an optional hierarchy-mode value and its
handoff uniform. Existing descriptors default to no hierarchy mode. New shaders
and descriptors live in `visuals/original/scenes/poc-*`; the 127-scene production
manifest is unchanged. The candidate is staged with `scripts/prepare_scene_poc.py`.
No new runtime module, native source change or executable rebuild was needed.

## Music, timing and authored cues

Inputs match the preserved identities:

- June Montana, *I Need Your Love*, authorised `01.flac` copy:
  `9e9ac5390a030224ccf48bd15916071d2d81779efd074ae2b6a748649e7b7e73`.
- Preserved Martooni artwork:
  `52be27bc0ac94ab4778812e7d0398659a5b6c92e4532b06391edf9da5e92227e`.

The sequence uses source seconds 88.73951–159.570794. Representative moments are
the established rhythm around 92 seconds, the fuller passage around 116 seconds,
and reduced energy around 128–136 seconds. The measured eight-second RMS falls
from about .171 at 112 seconds to .121 at 128 seconds, then recovers.

A bounded bass-onset pulse fit over seconds 40–130 supports 121.98 BPM and onset
phase .200405 seconds. Four 16-second windows have aggregate phase residuals
between −13.45 and +10.91 ms. This is **pulse evidence, not reviewed downbeat truth**.
The scene start is approximately pulse 180; two section cues are explicitly
authored every 48 pulses (23.610428 seconds), with four-beat overlaps (1.967535
seconds). No automatic musical section classifier or downbeat detector was built.
The shader responses use the existing mixed-PCM energy/attack envelopes.

The recording retains the requested encoded soundtrack. Native playback used
the same lossless excerpt, unit playback rate, neutral master processing and
monitor mute; mixed PCM still reached the visual worker. Listening acceptance
and physical audio latency are not established.

## Evidence and limits

| Evidence | Result and scope |
| --- | --- |
| First moving preview | 16 seconds, 720p/60 encoded fps, `.local/original-scenes-poc/loom-01/`; first image was too dark and tiled, retained for comparison. |
| Short joins | 18-second captures in `join-02/` and `join-03/`; intermediate sparse/collapsing geometry prompted the loop/orientation repair. |
| Final renderer recording | `sequence-01/`, 1280×720, 60 encoded fps, 60-Hz simulation; six final-state/pause checks. Offline render median 8.56 ms, p95 12.11 ms, excluding encoder/backpressure. This is not a native recording. |
| Hierarchy and lifecycle probes | `focused-02/checks.json`: 29 pass. Exact atlas/parent/child transfer and initial pixels for both joins; pause, finite state, outgoing release, incoming snapshot ownership and seek cleanup; two actual music passages versus silence; alternate transparent image and reset cleanup. |
| Existing focused unit tests | `tests/test_original_visual.py`: 11 pass, 0.387 seconds. No broad suite. |
| Native playback | `.local/poc1/evidence/`: 70.69 seconds at 1280×720; 4,240 window swaps, mean 59.98/s; median gap 17 ms, p95 20 ms, maximum 53 ms. Both overlap pairs observed in native frame facts; all three scenes inspected in native screenshots. Window swaps do not prove monitor scanout. |
| Native save/reopen | `poc-preview-reviewed.mixworks` successfully reopened with three scenes, one owned audio clip and the artwork; saved as `poc-preview-final.mixworks`, then closed gracefully. |

Native root/protocol/PID/capabilities were checked. Trusted executable SHA-256:
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
Runtime source and candidate hashes are independent of Git. Native state samples,
screenshots, source checkpoints, unsuccessful fixture requests and earlier movies
are retained. The initial shader used a reserved GLSL variable name, corrected
before GPU qualification. The first focused runner lacked the embedded Python
script search path, corrected before its passing run. Several manually authored
native fixture fields failed strict loading; the final fixture was validated by
actual open, playback, save and reopen. These were fixture failures, not accepted
native playback results.

No full regression, stress, endurance, physical-device or listening acceptance.
Pause/seek and ownership were checked; uninterrupted history is not reconstructed
after a seek. Artistic evaluation remains separate from those technical results.

## Initial visual assessment — superseded by user rejection

The first proof has distinct silhouettes and visible parent/child relationships.
The loop at Coil is clearer than the initial direct interpolation. Orbital Bloom
is noticeably busier and more fragmented than Ribbon Loom; Prism Fold exposes a
regular tile structure. These are the specific points to judge in motion, not
reasons to add more presets. No claim of polished or user-approved visual quality.

**Ember Garden remains deferred.** The initial proposal to judge or simplify Bloom
alone was insufficient: Jay has since rejected the overall visual approach above.

To revisit the isolated native preview (the running production app is unrelated):

```powershell
python scripts/alloy_bridge.py start .local/poc1 --exe out/releases/0.55.67-4258046694c2/Mixworks.exe
python scripts/alloy_bridge.py call .local/poc1 open --args-file .local/poc1/open-preview.json
```

Wait for readiness and verify the status says the session opened. Open Visuals /
scenes, keep 720p, and play. Monitor mute was enabled for the inspection. Follow
the bridge's save-before-close workflow. The encoded excerpt and artwork copy
are deliberately retained so this saved preview remains usable; original music,
production analysis, corrections, caches and protected references were untouched.

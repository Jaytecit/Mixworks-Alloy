# Prismatic Assembly — third visual direction proof

**Superseding feedback:** Jay found its beat response weak, sideways travel hid
rotation, and requested light/colour/glow and mirror/kaleidoscope to reduce dead
space. See [the reactivity revision](VISUAL-REACTIVITY-20261102.md) for the current
candidate. The original recipe and measurements below remain historical evidence.

Jay judged the Estuary correction better and asked for a completely different
movement style and use of nested imagery. This scene uses rigid spatial motion
and physical image-bearing hierarchies. It is ready for judgement, not marked as
artistically accepted.

## Visible difference

Sixteen staggered groups travel across and through depth, rotating continuously.
Each group has a parent, attached child and grandchild. Local child transforms
compose with their parent transforms, so the smaller forms travel with the large
form while turning independently. Artwork appears on each face in a distinct
gold/violet/ivory/navy palette. This is three geometric levels, not image feedback
or liquid material distortion. Visible lettering is incidental to the supplied
artwork; alternate artwork is supported.

Bass adds spatial travel, mids add parent angular travel, and treble adds child
and grandchild turns. Integrated phases retain motion as music releases; there
is no global audio-brightness multiplier. The scene has a quiet baseline speed.
These are stylised kinematics, not collision-resolved rigid-body physics.
Forms can interpenetrate; they are compositional elements rather than simulated
solid mechanics. Children are attached, not spawned as unrelated particles.

Two passes: 16×1 RGBA32F persistent kinematics, then full-resolution analytic
ray/box intersections for up to 48 forms with depth occlusion and face lighting.
The exit span accounts for aspect ratio and the hierarchy's conservative bounds
so wraparound happens outside the camera view. Shared audio, media loading,
pause/seek/reset and presentation are reused; no shared runtime code was changed.
Only isolated staging includes the new scene; the production catalogue is intact.

## Evidence

- Current frozen bundle: `.local/assembly/candidate-04`, including source archive
  and candidate hashes. Before/completion checkpoints are independent of Git.
- Soundtrack preview: `.local/assembly/final-preview/preview.mp4`, 26 seconds,
  1280×720, 60 encoded fps, authorised source music seconds112–138. No authored
  musical cues or scene transition. Sampled pixels inspected.
- Two capture checks passed: exact pause and finite state. Offline render median
  6.83ms, p958.27ms on RTX4060Ti, excluding encoder/backpressure. Scene plus output
  targets7,373,312 bytes; this is not a physical playback claim.
- Thirteen bounded GPU probes passed: monotonic travel/turns during attack and
  release; silence movement; independent band response and child turning; pause,
  deterministic seek, alternate-image participation and instance release.
- Early thin-card composition is retained in `candidate-01`/`early-01`; it lacked
  depth. The solid-form refinement is retained in `candidate-02`/`early-02`.
  Candidate03 expanded the group count; candidate04 bounds exits by aspect ratio.
- Native isolated root `.local/asm1` uses the existing trusted0.55.67 executable
  (SHA-256 `4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`).
  Protocol/root/PID/capabilities verified, monitor muted. Inspected native pixels
  retained as `evidence/assembly-playing.png`; delivery and save/reopen evidence
  are retained separately from the offline capture.
- Native measured interval: 1,440 window swaps over24.02 seconds (59.95/s), median
  gap17ms, p9519ms, maximum25ms. Initial startup gap121.26ms is retained in the
  full-run evidence. Saved/reopened successfully, saved as `assembly-final`, then
  closed gracefully. These are window swaps, not monitor scanout measurements.

No C++ build, full regression, stress or production data changes. No new transition
into Assembly was authored for this requested third-scene milestone. The existing
live-outgoing transition correction remains unchanged. Native muted playback does
not establish listening or physical display scanout acceptance.

The authorised excerpt copy remains in the isolated root for reopening the
preview. Original music, protected references and running production sessions
remain untouched. A skills.sh shader-method lookup was attempted but unavailable;
the implementation uses original GLSL and the repository's existing runtime.

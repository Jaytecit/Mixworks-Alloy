> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Stronger musical response and Assembly kaleidoscope

Jay requested more aggressive frequency response across the scenes. Assembly
needed legible beat rotations, additional movement, light/colour/glow and a
mirror/kaleidoscope composition to reduce dead space. Earlier proofs were not
accepted as sufficient evidence of musical strength.

## Current result

Frozen runtime: `.local/reactive/candidate-03`.
Soundtrack previews, both26 seconds at720p/60 encoded fps using music112–138s:

- `.local/reactive/assembly-preview/preview.mp4`: six reflected sectors, closer
  composition, fixed spatial anchors, percussive turns at all three hierarchy
  levels, bass depth/scale kick, mid child separation, band-coloured edge emission
  and bloom. Open mode retains greatly reduced drift; four-sector mirror and
  six-sector kaleidoscope hold their anchors. Mode0/0.5/1 selects the three views.
- `.local/reactive/vault-estuary-preview/preview.mp4`: stronger shared attack
  response in Vault; Estuary advances surface/inlay movement using accumulated
  attack envelopes and adds local treble ridge light. Overlap remains authored
  at4–9s with no authored musical response cues. Outgoing Vault stays live.

The shared AudioController multiplies positive relative band changes by3.5 before
clamping to1. Attacks below level0.015 do not create hits. Sustained band energy
is unchanged; quiet music is not blindly normalised to full strength. New
bassImpulse/midImpulse/trebleImpulse channels integrate attack envelopes, reset
on seek/revision/generation and remain unchanged when paused. These are additive
visual uniforms, not changes to audio/DSP or analysis data.

Assembly's angular state accumulates attacks, retaining turns after release.
Depth/scale/lighting accents intentionally return to rest while the turn remains.
The visible hierarchy still uses three physical levels. Reflection transforms
the view; it does not replace the parent-child transform chain. Glow reuses owned
Vault bloom/finish shaders via staging references. Five passes including a tiny
kinematic buffer use15,667,712 bytes including output at720p.

The soundtrack's half-second sampled mean hit strengths changed from
bass0.234→0.424, mid0.269→0.462 and treble0.352→0.492 under matching inputs.
These are envelope measurements, not proof of artistic acceptance.

## Verification and retained limitations

- Assembly15, Estuary14 and Vault13 bounded GPU checks pass. They cover band
  response, forward accumulated motion, pause, seek, alternate media, release,
  reflected modes and resource reset. Vault's pulse still starts at the far
  opening, travels toward the viewer and exits without reflection.
- Twelve focused unit tests pass, including visible modest attacks on a sustained
  bed, low-level noise rejection, independent bands, pause and impulse reset.
  Initial run had one obsolete exact-reset-dictionary expectation; updated it
  for the three new zeroed channels and reran. Both outcomes retained.
- Assembly capture passes two checks. Vault→Estuary passes eight, including exact
  current outgoing appearance at all six overlap samples and resource release.
  Sampled pixels from both recordings were inspected.
- Offline medians/p95: Assembly8.16/11.34ms; Vault→Estuary10.19/16.00ms on RTX4060Ti.
  These exclude encoding/backpressure and are not native delivery measurements.
- Native isolated `.local/rct1` uses the unchanged trusted0.55.67 executable.
  Protocol/root/PID/capabilities checked. UI input interference delayed opening
  the preview, so only delivered positions16.52–25.36 were measured:531 swaps in
  8.84s, about60/s, median17ms, p9521ms, max23ms. Startup maximum176.15ms retained.
  Paused pixels inspected and saved as `evidence/kaleidoscope-paused.png`.
  Saved/reopened successfully, saved as `reactive-final`, closed gracefully.
- Native monitor muted. No listening, physical scanout, full-catalogue or complete
  regression claim. No C++ rebuild or stress run. Vault/Estuary were checked with
  the real shader/player capture here, not rerun natively in this revision.

Original music, production catalogue and the separately running production Alloy
instance remain untouched. The owned excerpt is retained for reopening the native
preview. Before/candidate/completion source identities and failed early layouts
remain in `.local/reactive`, independently of Git. This is ready for Jay to judge
whether musical strength, glow and density now meet the intended experience.

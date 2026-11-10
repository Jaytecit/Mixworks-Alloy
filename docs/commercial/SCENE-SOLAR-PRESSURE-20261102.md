> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Solar Wash pressure pulses — update13

2 November 2026. Jay confirms the four rebuilt scenes are headed in the desired
direction. His remaining concern is sustained wash becoming featureless haze:
the pulsing image should push that wash away. Legibility remains optional.

Solar Wash now combines bass attacks with an energy-weighted positive beat
envelope. The pulse slightly expands the incoming image footprint, applies
outward pressure to retained pigment, clears old colour within that footprint
(including darker areas) and strongly replenishes fresh image pigment there.
This happens inside the feedback state. The display still has no image sampler.
There is no shared sine reversal or global camera oscillation.

The delayed reservoir is also moved outward, aged and refreshed faster during
a pulse, so it cannot immediately refill the new openings with old blurred colour.
Between pulses, flow/colour evolution continues with a bounded haze lifetime.
Reduced reservoir contribution and low-tone lift keep the display from masking
those openings. `pulse` controls pressure/replenishment strength. There are no
renderer, native, session-format or other-scene changes.

Evidence is retained independently of Git in `.local/scene-pulse-13/`:

- Before/after GPU comparison: 21/0, eight seconds each of sustained audio,
  regular kicks and the authorised real-music excerpt, all at a 140-BPM clock.
  Both pigment and reservoir remained finite and bounded; paused state/pixels
  were stable. GPU shaders compiled successfully.
- Actual bundled renderer with the new source/payload identities: 6/0,
  including motion, pause and opaque image feeding.
- Four-file activation fixture: 3/0, including the running-session guard,
  exact installation and idempotence. The real guard also deferred activation.

Sampled temporal frames show clearer pulse openings with older colour continuing
outward, rather than the sustained near-full-image fog of update12. Moving WebPs
and before/after frames are retained. Diagnostic pigment in dark image gaps fell
in all three comparisons; each run derives its mask from its initial state, whose
footprint differs slightly due to the new pulse. That diagnostic is not a shared
pixel-mask oracle or an artistic score. Visual observations support the change;
Jay's judgement of pulse strength and softness remains pending.

Real PCM uses the unchanged authorised `01.flac` copy, seconds 60–68.1, SHA256
`9e9ac5390a030224ccf48bd15916071d2d81779efd074ae2b6a748649e7b7e73`.
Decoded PCM existed only in memory. The authored clock is not a verified track
grid; no native/listening acceptance, complete regression, stress or benchmark
is claimed. Verification was serial and below normal priority. Long-run behaviour
outside these short captures remains untested.

Receipts confirm updates11/12 installed. Main PID18296 was left running and
untouched. Update13's four Solar Wash files are staged in the same main release,
`out/releases/0.55.67-4258046694c2/scene-update-13-payload`, with hash receipt and
rollback ZIPs in the release/evidence root. The normal launcher applies update13
only after Alloy and its visual worker are closed. Direct EXE launch bypasses it.
Read `scene-update-13.json` for actual activation state; native reload is pending.
Executable remains SHA256
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
Original images, protected references, music, analysis and sessions are preserved.

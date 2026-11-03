# Matched-input scene tuning — 2 November 2026

Jay requested movement and visual character closer to the Synesthesia references
before adding scenes. This first pass tunes four existing scenes; catalogue count
remains 127. Particle families and the other reference pairs are deferred until
this comparison receives feedback. Spout remains deferred.

## Comparison and changes

The private comparison uses the preserved Martooni PNG and authorised `01.flac`
copy, source seconds 60–68, for all scenes. Image SHA256:
`52be27bc0ac94ab4778812e7d0398659a5b6c92e4532b06391edf9da5e92227e`.
Music SHA256:
`9e9ac5390a030224ccf48bd15916071d2d81779efd074ae2b6a748649e7b7e73`.
Independent evidence, scripts, source checkpoints and rollback:
`.local/scene-tuning-17/`.

| Pair | Observed gap and tuning | Remaining difference |
| --- | --- | --- |
| Vein Current / Vein Melter | Replaced broad bright quilting with finer flow forces and relief; stronger retained material displacement and darker valleys. | Still finer/noisier than the matched default reference; faint regular background folds and edge shimmer remain. |
| Microbial Mosaic / Sprinkles | Removed independent background seeds when artwork is supplied; reduced continuous feeding so lettering erodes; larger image-led colonies, saturated concentration bands. | Colonies remain more uniform and bead-like than the reference's flowing, broad ridges. |
| Posterised Mirage / Thresholder | Stored colour bands steer stronger shear; bright regions replenish selectively on attacks; surviving pigment has clearer contrast. | More isolated against black, less full-frame and less internally complex than the reference. Screenshot palette/settings were not reproduced. |
| Solar Wash / Sun Set | Delayed state recurs through a rotated, scaled chart with colour changes; less reservoir blur and fresh injection. Distinct nested echoes replace some diffuse haze. | More centred and regular, with less abrupt reseeding than the reference. |

Microbial uses a contiguous 192×108 reaction grid instead of 384×216, retaining
six updates per 60 Hz tick. A rejected wider-neighbour sampling experiment made
a visible grid artifact: `after-02` is not the final Microbial candidate. Final
Microbial and Posterised captures are `after-03`; final Vein and Solar are
`after-02`. Initial dim candidates and their captures remain retained.

Only owned shaders and the Microbial descriptor changed. No proprietary code or
assets were copied into the product. State semantics, controls, protocol, native
executable, catalogue size and saved format remain unchanged. Existing control
values do not need a new block schema. The requested image legibility is optional;
loss of lettering is not treated as failure.

## Evidence and limits

Reference rendering uses unchanged installed scenes in the existing private
compatibility harness. Per-frame Alloy music envelopes were mapped into reference
uniforms; MidHigh duplicates treble and presence uses band levels. The authored
clock is 140 BPM. Native Synesthesia DSP, beat alignment, native frame scheduling
and the user's original screenshot settings are **not matched**. All four reference
scripts reported no errors and installed scene hashes remained unchanged. The
first attempt used product Python, which lacks the private harness's `dukpy`;
the existing private venv resolved it without installing dependencies.

Before/reference/final captures share input hashes, excerpt, 60 Hz rendering and
480×270 output. Reference internal simulation sizes remain scene-defined. Review
videos sample at 15 fps and contain the same eight-second music excerpt. Their
embedded audio is intentionally retained for the requested audiovisual judgement;
decoded PCM existed only in memory. Review files are private local evidence.
`preview/index.html` selects four labelled reference/before/tuned videos.

Final serial, below-normal-priority checks:

- Flow smoke: 22/0, including isolated attacks, finite state and pause.
- Nested smoke: 11/0, including attacks, pause and opaque-image ingestion.
- Changed-grid handoffs: 51/0, Vein→Microbial and Microbial→Posterised; ownership,
  finite state, pause, generation versus picture blending and post-overlap memory.
- Final real-music captures: 15 finite-state/pause assertions across four scenes.
- Activation fixture: 5/0; running-session guard, predecessor, payload hashes,
  exact installation and idempotence. Nine protected reference hashes unchanged.

Actual temporal pixels were inspected and repaired, rather than accepting numeric
passes alone. This is a preview ready for artistic judgement, not reference parity
or listening acceptance. No native build/session manipulation, audio device test,
full regression, stress, endurance or performance benchmark was run. GPU shader
compilation used the bundled interpreter and verified update16 renderer modules.
The source renderer modules match those tested bundled modules exactly.

## Local delivery

Update17 stages ten hashed files after update16 under the existing
`out/releases/0.55.67-4258046694c2` release. At staging, update16 was still pending;
update17's expected predecessor hashes include its staged changes. The rollback
archive restores the post-update16 state. The normal launcher applies16 then17
after Alloy and its worker close normally. Both remain pending at this checkpoint.
Direct EXE launch bypasses that chain; inspect receipts before claiming activation.
No restart or running-runtime replacement occurred. EXE SHA256 remains
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.
Original music, reference screenshots, user sessions, analysis and caches were
untouched. No public package or release clearance is implied.

> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Nested image feedback — update12, 2 November 2026

Jay chose owned generative shaders with images embedded in their evolution;
Spout remains a later interoperability option. Jay subsequently clarified that
logos/sprites need not remain legible: judge evolving structure and musical
response, and allow fragmentation, dissolution and abstraction. This implements the image-feedback
part of the audit's second group: **Posterised Mirage** and **Solar Wash**.
Firefly Assembly/particle dynamics and generative scene transitions remain later
work. No NestDrop shader, sprite FX code, asset or runtime was copied or loaded.

## Behaviour

Posterised Mirage selectively captures bright/coloured image regions into a
persistent pigment target. Stored colour and its gradients steer subsequent
sampling; local shear pulls captured regions into strips. Bass opens the capture
threshold, high attacks replenish regions and add edge circulation, and mids
change shear. Its posterised display reads only stored pigment.

Solar Wash injects image masks and colour into a recurrent pigment system. A
second, temporally delayed and lightly blurred reservoir affects future sampling.
Stored colours steer currents; bass expands and partially erases accumulated
colour, mids change feeding/colour evolution, and high attacks inject new image
regions. Colour rotation preserves chroma; bounded gain avoids the noisy,
overexposed sharpening experiment. Its display reads pigment and reservoir only.

Both use the existing schema2 renderer at 60 fixed ticks/second and 512×288
RGBA16F state. They contain no shared beat-swing functions or direct fresh-image
sampling in the display stage. Images therefore survive source removal and keep
changing, instead of being recomposited over the final frame. The renderer itself,
Vein Current and Microbial Mosaic are unchanged by update12.

Existing controls have scene-specific semantics: `imageMix` is fresh injection
strength (zero holds captured material while feedback continues), `feedback` is
persistence, `motion` is evolution rate and `warp` controls colour-driven movement.
The existing native control keys remain; no new native control panel is claimed.
Premultiplied sampling before interpolation prevents hidden transparent RGB from
entering state. Opaque images and cutouts both feed the generators. Cutout fitting
was corrected during development so Martooni occupies a useful portion of the frame.

State remains clip/scene/revision/generation-owned. A seek/revision change resets
the feedback; paused control edits redraw without advancing it. Image replacement
does not clear previous pigment. No cross-scene buffer transfer, live-video input,
Spout bridge or generative scene transition is delivered here. The existing output
compositor remains an output blend.

## Evidence and limitations

Independent source/evidence: `.local/scene-nested-12/`. Primary artwork is the
preserved Martooni PNG, SHA256
`52be27bc0ac94ab4778812e7d0398659a5b6c92e4532b06391edf9da5e92227e`.
Its reference set, original music and user sessions remain unchanged.

Moving WebPs and temporal frames cover silence, sustained energy and isolated
hits at the same 140 BPM, plus tempo-aligned isolated hits at 120/160 BPM. The
capture-and-hold case removes the source and closes injection at two seconds;
six further seconds demonstrate retained image identity and continued evolution.
At held time, changing/removing the source produces identical pixels and state
when injection is closed. Palette-only edits preserve stored pigment. Additional
checks cover transparent RGB, pause, reset, 30/60 Hz held-input equality, finite
bounded state and opaque-media ingestion.

Accepted focused results (combined scope, not a complete regression):

- Posterised Mirage: 47 checks from `gpu-01/report.json`; its shaders are unchanged
  since that run.
- Solar Wash: 49/0 from `gpu-02/report.json` after repairing feedback gain.
- Final isolated bundled runtime: 11/0, including both shader builds, motion,
  pause and opaque image input.
- Existing descriptor/controller unit suite: 8/0.
- Ordered update11→update12 activation fixture: 4/0, including active-session
  preservation, wrong-predecessor rejection, exact payload installation and
  idempotence. The real active-session guard also left live files untouched.

Two final six-second real-PCM captures use the previously authorised track copy,
source seconds 60–66.1. Decoded PCM existed only in memory. The 140-BPM visual
clock is authored; track-grid alignment and listening acceptance are unverified.
Sampled motion frames show selective retained fragments in Posterised Mirage and
spreading/recoloured contours in Solar Wash, with different sustained/hit results.
Solar Wash is intentionally soft; long held imagery can lose fine spatial detail.
Posterised Mirage smears lettering as it evolves; that is allowed by the latest
direction and is not itself a defect. Neither is claimed to match the full
range or polish of NestDrop presets; final artistic judgement remains Jay's.

Exploratory captures are retained. `preview-01` was too small and faded too quickly;
`preview-03`/the old Solar part of `gpu-01` amplified high-frequency noise and washed
out bright areas. Its technical checks passed but visual quality was rejected.
Those Solar results are superseded, not counted as accepted final-candidate checks.
No full regression, stress, benchmark, native build, audio-device test or public
release occurred. The current Python/GLSL runtime compiles the shaders on the GPU;
no native executable/engine/session-format contract changed.

## Delivery

Eight files are staged in the main release's `scene-update-12-payload`, with exact
hashes in `scene-update-12.json` and rollback ZIPs in both release and evidence root.
Main release remains `out/releases/0.55.67-4258046694c2`, executable SHA256
`4258046694c22715251e3d44734eb46c1a060d760fb17027168ccc0f8b5cdd02`.

Update11 was still pending and main PID32944 was running. The ordinary launcher
now applies update11, then update12, before starting Alloy on a clean launch.
Both installers defer while the app/visual worker runs. Update12's catalogue
predecessor and rollback deliberately refer to update11's catalogue; do not
activate it against update09 or describe the two pending payloads as live.
Direct EXE launch bypasses these installers. No running session was restarted or
modified. Read the two receipts for actual activation state; native reload remains
unobserved. The generator preserves these authored feedback scenes on regeneration.

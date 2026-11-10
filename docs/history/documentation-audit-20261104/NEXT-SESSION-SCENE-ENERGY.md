> Historical snapshot preserved on 4 November 2026. Completed tasks and old next steps below are inactive. Use [the outstanding roadmap](../../../FINISHING-PLAN.md) and [current handover](../../../HANDOVER.md).

# New-session prompt: finish Alloy scene energy and screen coverage

Work in `C:\Users\jayte\Desktop\REA\Mixworks`, the Mixworks-Alloy repository,
not the parent REA repository. Read AGENTS.md and HANDOVER.md first. Implement
the remaining scene improvements described below; this is an implementation
request, not another shortlist or planning-only pass.

## Accepted direction and reference

I approved the stronger versions of Prism Bloom, Liquid Torus and Orbit Theatre:
large bass-driven pumps, abrupt snap rotations and camera surges. They are now
the reference for the level of energy I want. Preserve their approved changes.
Jumping Techno Tunnel is also an original Alloy scene. Preserve the first
cyan/pink rings-and-spokes shader's jumping motion; do not replace it with the
later raymarched tunnel or smooth away its intentional time-times-bass jumps.

The desired experience is activity across the whole screen over time. An empty
region is acceptable when large, frequent movement sweeps through it. Small
central objects gently rotating in a mostly black field are insufficient.
Keep each scene recognisable. Do not apply identical transforms, a generic
particle background, or the tunnel's depth formula to every scene.

Accepted comparison videos and exact proposals:
`.local/punch-motion-20261104`. Implementation and verification evidence:
`.local/punch-apply-20261104`. Read the current HANDOVER for activation status
and the exact candidate; do not assume a running instance has the latest bundle.

## Scope: all scenes that actually need it

Use `docs/SCENE-SPACE-AUDIT-20261104.md` and the per-scene evidence in
`.local/scene-backgrounds-20261104`. The original audit covered 203 scenes:
38 priority candidates, 19 secondary, five conditional motion-first cases and
one separate behaviour investigation, for 63 candidates. Three have now been
implemented: Prism Bloom, Liquid Torus and Orbit Theatre. Address the remaining
60 candidates, resolving each with changes or specific observed evidence that
its current movement already satisfies the brief. The library now has 204
originals including the tunnel; the older audit totals remain historical.

Use stronger scale/dolly excursions, wider/off-axis travel, rhythmic expansion,
growth, closer passes, snap rotations or camera surges as appropriate. Where
these still leave persistent dead regions, add matching moving depth, sky,
reflections, mist, secondary structures or image-derived activity. Maintain
whole-image readability where it matters. Do not replace approved Lucent Ranges
geometry/camera; improve its unused sky with restrained complementary activity.

Investigate Plasma Orchard's near-empty output before adding decoration. Its
earlier 10/30 fps probes do not establish the cause or failure for every input.
Keep Neon Run, Soma Lanterns and Circuit Confetti unless new motion evidence
shows a concrete problem. Avoid unnecessary changes to the 137 scenes without
an obvious empty-field concern. Most candidates were triaged from stills/source,
so inspect their actual motion before deciding what each needs.

## Implementation and acceptance

Work through coherent scene families, maintaining a durable per-scene progress
record. Preserve catalogue keys, distinct materials, image integration, named
looks and control behaviour. Update relevant shared authoring templates only
when needed, and keep approved instances from being overwritten by old recipes.
Keep deliberate discontinuities energetic while ensuring transport pause holds
exact pixels, seek/reset works, and camera surges do not produce broken or
unintelligible views. Add controls only when they serve a real artistic choice.

Compare before/after with identical images, audio and settings. Observe quiet
and strong input, beat hits and recovery, and enough time to judge repeated
screen occupancy. Use wide-aspect checks where projection/coverage needs them.
Numerical coverage is a diagnostic, not proof of artistic quality. Inspect real
pixels and motion. Refresh main/named-look library previews for changed scenes.
Do not mark all work complete based only on compilation or counters.

Follow the current low-disruption testing policy: serial, below-normal-priority
work, one build worker, no stress/benchmark/full regression unless requested,
no numeric RAM guard. Batch related changes and verify at useful checkpoints;
avoid rebuilding or repeating suites after each scene. Use focused GPU/render
checks and appropriate native scene/library workflows through
`verification.run_checks`. Use the development bridge with a fresh owned short
`.local/` root for app checks; save before graceful close. Stop owned work if
desktop responsiveness actually worsens. State deferred coverage honestly.

Keep source checkpoints, hashes, comparison media and verification evidence
independently of Git. Update HANDOVER and the per-scene progress record. Keep
`.local/`, build/out, user artwork/music, sessions and caches out of commits.
Preserve production sessions, analysis/corrections, protected downbeat references,
media-folder settings and external MilkDrop presets/JPGs. Do not start the full
NestDrop collection scan without checking the current user instruction. Stage
verified local changes for the launcher without restarting a running production
instance. Do not publish a public package or change licensing.

Proceed autonomously with concise progress updates. Ask only if an unresolved
artistic choice materially changes direction. Finish the full set of required
improvements, report what was demonstrated, and provide representative motion
previews for judgement rather than stopping after the first few examples.

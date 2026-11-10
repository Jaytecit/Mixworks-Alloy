> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Five image-driven examples — implementation checkpoint

Approved by Jay, 3 November 2026: implement five original examples informed by
Sprinkles, Swarm, Terrain Lattice, Molten and Glitch; include brighter palettes
and continuous particle movement; show a preview before expanding to eighty.
Execute inline, preserve existing dirty edits and production sessions.

Goal: five visibly different, music-reactive image generators in a review movie
and an isolated native save/reopen fixture. No reference parity claim.

Architecture: use the owned multi-pass renderer and its declared state semantics.
Each example owns its generation shaders. Shared display palettes are independent
of simulation; particle assembly is a signed passage through source coordinates.
Native palette selection uses the existing undoable scalar-control path.

Files: `visuals/original/scenes/{neon-culture,chromatic-shoal,terrain-prism,
liquid-memory,signal-fracture}` hold the five authored examples. Existing active
particle instances and their `kernels/sprite-*` templates receive continuous
assembly. `scripts/original_visual_gl.py` and `visual_catalogue.py` implement
palette rendering/control defaults. Active descriptors expose those choices.
`Source/SceneLane.cpp` adds the palette menu; `VisualScenes.cpp` keeps discrete
palette selection stable during automatic arrangement. Focused GPU review tooling
and evidence live in `.local/image-five-20261103`; durable reusable checks live
under `scripts/` and `tests/`.

- [x] Checkpoint source independently of Git; retain earlier failures.
- [x] Implement five bounded mechanisms and register their original provenance.
- [x] Add selectable palettes and continuous particle assembly across active scenes.
- [x] Freeze candidate; compile every changed shader with small focused GPU probes.
- [x] Verify finite state, actual image influence, music-dependent paths, pause,
      continuous assembly, palette state preservation and current live blends.
- [x] Build once with one worker at below-normal priority; run relevant runtime
      units/native visual checks serially through the retention harness.
- [x] Render a short soundtrack preview; inspect sampled pixels and motion.
- [x] Open/save/reopen the same candidate in a fresh bridge root, inspect native
      delivery, save and close it gracefully; keep production untouched.
- [x] Record identities, evidence, limitations and preview links in HANDOVER.

Reference source and proprietary assets remain private research. Product shaders
are owned implementations. No eighty-scene expansion or production activation is
part of this checkpoint. Use essential focused coverage, no stress/full gate.

# Glass mosaics and project maintenance — 3 November 2026

Jay authorised refracting, see-through mosaics over a complete image, removal of
obsolete documents/tests, a new local build/launcher and a source commit pushed to
[Jaytecit/Mixworks-Alloy](https://github.com/Jaytecit/Mixworks-Alloy).

## Visible change

Rhombus Foundry and Enamel Scales now refract a single complete image behind the
tiles, including visible gaps. Mosaic Kiln and Spinning Stamps offer **Glass image**
as a named look. Warp controls glass/refraction; Image mix still removes or restores
the image. Beat-driven scale and tile normals distort the complete image without
turning it into a different picture per square. Opaque whole-image tile looks remain.
The lane labels extended sweep/radial requests as Dissolve, matching actual playback.

## Audit scope and decisions

The starting Git-visible worktree contained 2,258 files (about 79.7 MB), including
173 documentation files, 233 Source files, 70 scripts and 20 test files. Inventory
and original bytes are independently preserved in
`.local/glass-release-20261103/inventory-before.json` and `before-source.zip`.
The audit inspected document ownership/links/current claims, CMake source membership,
test selectors, runtime API/fixture dependencies and public commit inputs. It is
not a claim of exhaustive behavioural or legal review of every implementation line.

Removed 17 duplicate/superseded history documents and 27 obsolete visual proof,
external-renderer and catalogue-generator scripts. Old generators could restore
retired scenes or overwrite curated looks. Historical one-off scripts depended on
retired scene sets, private fixtures, earlier target layouts or handoff semantics;
they were not maintained current regression coverage. Their exact source and unique
historical assertions remain recoverable; no historical result was reclassified as
passing current code. Full hashes/reasons are in `removal-plan.json`.

Removed the external-renderer native test function and `--visual-scenes-only`, plus
the stale 44-track `--visual-coverage-only` branch. The remaining scheduling tests
were migrated to the owned catalogue, current JSON precision and long-blend policy.
Current native coverage retains timing, bounded overlaps, Undo, save/reopen and
Auto Arrange workflow checks. The original-runtime suite retains 123-scene/preset
validation and actual rendering. A new authored-fixture GPU behaviour runner covers
long live blends, short handoffs, musical particle paths, complete glass image
regions, image controls and pause. Generic preset/method checks and runtime/media
unit tests remain. This is consolidation, not a claim of one-for-one migration of
every historical visual probe or performance experiment.

Consolidated the 1,586-line handover and historical verification narrative into
current entry points. Updated README, documentation index, visual direction and
roadmap to remove contradictory one-scene/100-scene stop instructions, stale
version statements and memory guards. Detailed P01–P56 requests, original wishlist,
commercial provenance, operating guides and dated research/audit evidence remain.
Original user artwork is excluded from the source commit; no music, AppData,
protected downbeat references, sessions, caches, retired scene source or evidence
bundles were deleted. No production app is restarted by launcher activation.

## Maintained verification

- `tests/test_original_visual.py`, `tests/test_visual_media.py`.
- `scripts/verify_visual_behaviour.py`: deterministic authored input, no private media.
- `scripts/verify_scene_looks.py`: selected catalogue presets/methods and pause.
- Native `--original-visual-only` and `--visual-arrangement-only`, via retention.
- `scripts/check-project.py`: document file links and CMake membership.
- `scripts/stage_visual_runtime.py`: immutable active-catalogue snapshot; no retired
  entries are restored and existing output folders are refused.

Build version **0.55.68** passed the single-worker build and these focused checks:

- 17 visual runtime/media unit tests.
- 45 native renderer and 47 native scheduling/workflow checks, through retention.
- 41 GPU behaviour checks and 16 selected preset/method checks.
- Both retired selectors exit 2 with an explicit refusal before suite setup.
- Document links/CMake membership passed after correcting one obsolete Video link;
  the original failed document report remains alongside the passing report.
- A 24-second soundtrack glass preview, reference images and the native Mosaic Kiln
  browser image were inspected. Isolated `.local/gla1` saved/reopened the four-scene
  session, confirmed playback stopped and closed gracefully. Its owned review audio
  and images are deliberately retained; ordinary native test fixtures were cleaned.

The launcher now selects `out/releases/0.55.68-110a215dcfdd/Mixworks.exe`.
Executable SHA-256:
`110a215dcfdd16fb8cc4ba82dfcbc086c1e3ccedd7fc00fc2e4323052f29cd8d`.
`activation-receipt.json` binds focused native reports, source/runtime identities,
all bundle payload hashes and old/new launcher hashes. The previous bundle and
launcher backup remain available. The desktop shortcut already routes through
`Mixworks.cmd`; image-folder settings were preserved and no production app restarted.

The source checkpoint includes accumulated authorised development since the prior
Git checkpoint, with user inputs, local binaries and evidence excluded. Its exact
commit/push identity is retained in the local `publication.json` after publication.
This is a focused local update, not a public binary release or complete regression,
stress, hardware, sustained-performance or listening qualification.

## Recovery inventory

Every file below was byte-verified against `before-source.zip` before deletion.
Restore a needed file from its listed ZIP member to a new inspection directory,
compare its removal-plan SHA-256, and review relevance before reintroducing it.
Never restore an old generator over the active catalogue automatically. The archive
also retains previous root documents and native test functions before consolidation.

| Archived path | Reason |
| --- | --- |
| `docs/history/DESIGN-20261024.md` | Duplicate continuity/guide |
| `docs/history/EXPORTING-20261024.md` | Duplicate continuity/guide |
| `docs/history/FINISHING-PLAN-PRE-CLEANUP.md` | Duplicate continuity/guide |
| `docs/history/HANDOVER-20261024.md` | Duplicate continuity/guide |
| `docs/history/HANDOVER-PRE-CLEANUP.md` | Duplicate continuity/guide |
| `docs/history/HANDOVER-VISUAL-CHECKPOINTS-20261102.md` | Duplicate continuity/guide |
| `docs/history/LIVE-DESK-20261024.md` | Duplicate continuity/guide |
| `docs/history/ORIGINAL-SCENES-POC-PLAN-SUPERSEDED-20261102.md` | Duplicate continuity/guide |
| `docs/history/PERFORMANCE-MODE-PLAN-20261024.md` | Duplicate continuity/guide |
| `docs/history/README-PRE-CLEANUP.md` | Duplicate continuity/guide |
| `docs/history/SWAP-HANDLES-20261024.md` | Duplicate continuity/guide |
| `docs/history/TIMELINE-EDITING-20261024.md` | Duplicate continuity/guide |
| `docs/history/TIMELINE-LAYOUT-049-20261024.md` | Duplicate continuity/guide |
| `docs/history/TRANSITION-FILTERS-20261024.md` | Duplicate continuity/guide |
| `docs/history/TRANSITION-PRESETS-20261024.md` | Duplicate continuity/guide |
| `docs/history/VERIFICATION-PRE-CLEANUP.md` | Duplicate continuity/guide |
| `docs/history/VISUAL-ROADMAP-SUPERSEDED-20261102.md` | Duplicate continuity/guide |
| `scripts/verify_visual_blend.py` | Historical visual proof/generator |
| `scripts/verify_visual_renderer.py` | Historical visual proof/generator |
| `scripts/verify_visual_package.py` | Historical visual proof/generator |
| `scripts/verify_visual_presentation.py` | Historical visual proof/generator |
| `scripts/verify_original_visual.py` | Historical visual proof/generator |
| `scripts/verify_original_scene_pack.py` | Historical visual proof/generator |
| `scripts/verify_visual_library.py` | Historical visual proof/generator |
| `scripts/verify_scene_prototypes.py` | Historical visual proof/generator |
| `scripts/verify_scene_poc.py` | Historical visual proof/generator |
| `scripts/verify_particle_scenes.py` | Historical visual proof/generator |
| `scripts/verify_flow_scenes.py` | Historical visual proof/generator |
| `scripts/verify_generative_handoff.py` | Historical visual proof/generator |
| `scripts/verify_edm_media.py` | Historical visual proof/generator |
| `scripts/verify_edm_scenes.py` | Historical visual proof/generator |
| `scripts/verify_nested_scenes.py` | Historical visual proof/generator |
| `scripts/verify_beats4d.py` | Historical visual proof/generator |
| `scripts/verify_assembly_direction.py` | Historical visual proof/generator |
| `scripts/verify_estuary_direction.py` | Historical visual proof/generator |
| `scripts/verify_visual_direction.py` | Historical visual proof/generator |
| `scripts/prepare_scene_poc.py` | Historical visual proof/generator |
| `scripts/prepare_visual_direction.py` | Historical visual proof/generator |
| `scripts/render_scene_poc.py` | Historical visual proof/generator |
| `scripts/render_visual_direction.py` | Historical visual proof/generator |
| `scripts/generate_visual_library.py` | Historical visual proof/generator |
| `scripts/generate_particle_scenes.py` | Historical visual proof/generator |
| `scripts/build_relief_library.py` | Historical visual proof/generator |
| `scripts/build_scene_looks.py` | Historical visual proof/generator |

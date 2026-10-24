# Project maintenance

Reconciled on 19 October 2026. [HANDOVER.md](../HANDOVER.md) owns current continuity; [the documentation index](README.md) separates operating guides from dated research/history.

## Folder ownership

Keep app source in `Source/`, real plugin fixtures/harness tests in `tests/`, reusable build/research/verification entry points in `scripts/`, and operating guides in `docs/`. The project root keeps its entry documents, CMake configuration, launcher and notices. `docs/research/` preserves investigation provenance; `docs/history/` preserves superseded continuity documents. Those files are not instructions to resume old tasks.

`build/` is regenerable, but retain the current executable and real VST3 fixtures while receipts refer to them. Read `build/mixworks-Release.path` to identify the selected executable; its directory can have a historical name (`build/detection-Release/` currently holds 0.54). Folder names do not establish obsolescence. `.local/deps/` contains pinned dependencies and must remain available for offline rebuilds. `.local/source-baselines/` and the cleanup's before/final archives preserve this untracked prototype independently of Git. Future snapshots must include the full `docs/` tree, source, scripts, tests, design references, licences and root configuration/entry files.

`out/Launch.cmd` selects an immutable versioned release; `Mixworks.cmd` is the public entry point. Keep the current and immediately previous release, plus the latest launcher backup. Older generated deployment copies may be retired after checking no process uses them and preserving unique artifacts/receipts needed for history. Under the user's 20 October duplicate-build cleanup authorisation, redundant historical assets may be removed only with an exact-hash retained copy and restoration mapping. Keep current/previous accepted bundles intact and reports/source checkpoints preserved; historical bundles can be reconstructed before revalidating their receipts.

## Safe cleanup

Before moving/removing files, resolve exact absolute paths inside this workspace, reject links/reparse points, inspect relevant running process paths and record the plan. Preserve unique source/evidence before deleting generated copies. The 19 October cleanup stores original paths, sizes and hashes and a deduplicated recovery archive under `.local/spring-clean-20261019/`; its report records completed removals and recovery instructions.

Do not delete `.local/` wholesale. It contains source recovery, real analysis observations, failed experiments, reference material, frozen candidates and session recovery alongside scratch output. Large historical evidence was deliberately retained. An obsolete-looking filename, malformed test fixture or `.bak` extension alone is insufficient proof that a file is disposable. Cached bytecode and independently preserved one-off edit scripts can be removed.

AppData, original audio, analysis records/corrections, saved arrangements, decoded/prepared caches, library state, recovery and MIDI mappings are outside routine folder cleanup. A verified detector update has its separate explicit reset policy in AGENTS.md. Cache-compatible performance and maintenance work preserve those inputs. Never stop the user's app to tidy build outputs.

## Checks

Run `python scripts/check-project.py` to check local Markdown file targets and CMake source membership. It checks file existence, not web URLs, anchor spelling, factual musical accuracy or runtime behaviour. Review changed claims against current source and accepted evidence.

Follow [proportionate testing](TESTING.md): tooling changes need relevant harness tests; cosmetic changes need build/visual review; local UI behaviour needs affected workflow checks. Engine/audio/data/cross-cutting changes and final release milestones require the full [22-suite gate](../VERIFICATION.md). Scoped local UI receipts are accepted by deployment. Folder/doc cleanup does not require a new detector revision, session migration, application release or musical benchmark.

Ordinary tests discard generated audio, fixture arrangements/backups and isolated TEMP caches after each native suite, retaining compact evidence and hashes. Research consumers explicitly retain required measurement audio. Historical cleanup is restricted to recorded plans, with per-file hashes and protected real-session/source-checkpoint verification. See [retention details](TESTING.md#test-data-retention).

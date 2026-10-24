# Audit rule contract and release decision

Use [README](README.md) as the canonical index. Durable rules live here and in the catalogues below; results belong to new immutable run folders under `.local/ra-<short-id>/`.

| Catalogue | Rules |
| --- | --- |
| [UI and workflows](rules/UI.md) | UI input/dispatch, accessibility, layouts, complete journeys |
| [Time and editing](rules/TIME.md) | coordinate clocks, geometry, transitions, nodes, range/HC/section operations |
| [Analysis and library](rules/ANALYSIS.md) | formats, scans/cache identity, beat/bar/key/energy, corrections/retries |
| [Audio and plugins](rules/AUDIO.md) | routing, DSP, real-time ownership, inserts, latency, metering |
| [Performance and integrations](rules/PERFORMANCE.md) | automation/MIDI, launch, takes, private preparation, live WAV |
| [Persistence and stems](rules/DATA.md) | JSON/save/relink/recovery/cache, external stem lifecycle/playback |
| [Arrangement and export](rules/OUTPUT.md) | intelligent mixing, rehearsals, render/WAV/tracklist validation |
| [Release engineering](rules/RELEASE.md) | trust, dependencies, build/package, capacity, diagnostics/tests/claims |

## Required fields and shared preconditions

Each rule has a stable `MW-<family><number>` ID, title, contract/evidence source, scope, concrete cases/procedure, expected result/independent oracle and evidence/mode/severity. Scope names expand to the exact files/dependencies/consumers in [COVERAGE](COVERAGE.md) and `inventory/files.csv`; operating environment defaults to **Windows x64, qualified release build**, with device/rate/plugin/scale partitions specified in RUNBOOK. Every rule is **mandatory if applicable**, initially **NOT RUN**, unless explicitly marked otherwise. P0 = crash/data destruction/audience routing breach or equivalent; P1 = core audio/timing/workflow/security/release invalidity; P2 = other functional/access/diagnostic defect; P3 = bounded presentation/document defect. Severity ranks repair order, not permission to ship known defects.

All runtime cases require an immutable candidate and source archive, fresh owned fixture root, no production attachment, valid dependencies or an intentional missing/corrupt fixture, captured starting state, and no overlapped tests/builds. Use [fixture profiles and numerical contracts](RUNBOOK.md). An invalid operation must preserve the prior document/state/output and emit an actionable reason unless the rule explicitly defines a partial artifact. After every applicable cancellation/failure: wait for jobs/children to finish, attempt the valid operation again, save/reopen, and verify usability and no stale mutation. This shared recovery procedure is required, not optional boilerplate.

Where a rule specifies enumerations, enumerate all values including invalid sentinels through the actual boundary. For larger state spaces, run all values of the primary control, pairwise combinations of independent state dimensions, plus the listed high-risk intersections. Pairwise UI admission does not replace exhaustive finite MIDI/action IDs or analytical timing sweeps. Record generated cases, uncovered interactions and reasoning. No randomized/fuzz/stress expansion is automatic under the responsiveness policy.

## Evidence statuses

Only **NOT RUN**, **PASS**, **FAIL**, **BLOCKED**, **NOT APPLICABLE** are check statuses. A stale prior PASS becomes NOT RUN for the new candidate, with its prior record retained as historical evidence. Partial evidence is attached to the current status without introducing a sixth status. BLOCKED requires the missing prerequisite/reason and owner; NOT APPLICABLE needs the specific excluded feature/platform/package claim and evidence of its absence. Missing hardware is BLOCKED, never NOT APPLICABLE for a claimed integration.

PASS requires all applicable cases and every evidence mode in the rule: source inspection, real runtime, pixels, listening, hardware and installation are not interchangeable. A bridge acknowledgement, successful exit, screenshot existence, rendered WAV header or native mock alone cannot satisfy outcomes it does not observe. Tests that use the implementation to compute their own expected values need an independent oracle as specified. No thresholds may be relaxed after viewing results merely to obtain PASS.

Per-rule result fields: `ruleId`, status, applicability/reason, run/candidate ID, source manifest and bundle/dependency/plugin/fixture hashes, requirement revision, OS/hardware/backend/sample rate/block/scale, exact command or manual steps, case IDs, expected/actual measurements, oracle/version, links+hashes to reports/logs/decoded measurements/captures/review notes, failures and partial evidence, defects/ambiguities, tester/time, retention journal, residual uncertainty. Use the [result template](templates/result.json). Keep human/private music paths local; published reports may use stable anonymized source hashes.

Confirmed defects require conclusive reproduction or static evidence, affected candidate/scope, impact/severity, rule IDs, steps/fixtures, expected/actual result, evidence, repair disposition and regression verification. Suspicions have a separate ledger and cannot be silently promoted/dismissed. Optional enhancements and aesthetic preferences are not defects. Historical findings must be re-evaluated against current source rather than copied as open or closed.

## Strict release gate

Release qualification requires all of the following:

1. Every inventory item/control family, shipping feature and end-to-end journey maps to a rule; all mandatory applicable rules PASS with evidence on the candidate.
2. Zero unresolved confirmed in-scope defects, including P2/P3; zero release-critical unresolved requirements. No hidden FAIL/BLOCKED/NOT RUN, empty/skipped suites or unreported environment limitations.
3. A complete final candidate regression checkpoint **when explicitly authorized**, plus supplemental newer-feature suites, live workflows, independent waveform/measurement checks, listening, physical integrations and clean-package testing. A 22-suite receipt alone is not whole-product acceptance.
4. Tested package bytes tied to source/build/dependency manifests, retained separately from Git. No edits/repackaging after acceptance without documented invalidation and appropriate rerun.
5. Public distribution additionally needs resolved dependency/asset rights, exact notices and owner authorization for publication. Local staging is not legal clearance.

The current task authorizes **none** of the execution/fixes/package publication above. Later use of RUN_AUDIT_PROMPT authorizes local audit and repairs, but respects AGENTS: focused, batched verification during iteration; full gate/stress/capacity/benchmark remains deferred until the user explicitly requests that checkpoint. If that authorization is absent, mark the final gate BLOCKED and continue useful safe work; do not repeatedly ask or run it secretly. Do not declare the target reached with the block outstanding.

## Invalidation and multi-session operation

Hash source, resources, model/runtime, test plugins, fixture inputs/reference annotations, tools and package. A changed hash invalidates rules depending on it and downstream consumers. Engine/model/shared timing/persistence changes conservatively invalidate audio, export, take/private, arrangement and round-trip evidence; detector changes invalidate detector/cache/consumer evidence. UI wording only invalidates its visual/command/docs claims, subject to semantic review. Tool/oracle changes invalidate results they produced. Documentation-only changes require claim/link review, not an app-wide rerun. A changed executable cannot reuse a native full receipt: rerun final candidate checks at the authorized checkpoint. Document the dependency analysis; unexplained impact stays NOT RUN.

At every session end update STATUS with candidate, last completed case, next exact command, running owned jobs, open defects/blocks and preservation requirements. Never overwrite an earlier failed or interrupted run, replay a timed-out bridge mutation blindly, replace reference reviews, or infer success from a previous session's completion text.

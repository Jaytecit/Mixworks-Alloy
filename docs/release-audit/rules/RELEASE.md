# Build, trust boundaries, diagnostics and qualification

Inherit RULES. Scope tooling/rights/app/research/docs/assets and all cross-cutting consumers. All mandatory, NOT RUN; individual absent integrations need explicit NOT APPLICABLE rationale.

### MW-R01 — Startup and lifecycle use only intended entry points
- Contract/source: Main.cpp, AppWindow close guard, Workstation teardown; moreThanOneInstanceAllowed=true.
- Cases/procedure: no args/audio/session CLI, malformed/conflicting flags, empty/missing files, dirty close Save/Discard/Cancel, open/new during owned background work, multiple owned windows and graceful restart. Review all option sites in CLI inventory including maintenance/research/self-test reachability.
- Pass/oracle: documented entry chosen or actionable safe rejection, no accidental production-mode launch when dev flag unsupported; dirty state not lost on cancelled/failed save. Jobs/callbacks/dev lease/device handles released without affecting other processes; no hidden feature experiment advertised as ordinary shipping workflow.
- Evidence/mode/severity: dispatch map, isolated process/file/state trace, actual native close/picker tests; static + runtime/manual; P1.

### MW-R02 — Bridge isolation and exactly-once admission survive failure
- Contract/source: PromoControl/Main, alloy_bridge.py/verify-bridge.py, docs/DEVELOPMENT-BRIDGE.md protocol1.
- Cases/procedure: fresh root/protocol/PID/capabilities, all supported commands and argument endpoints, invalid token/time/replay/64 KiB size, path escapes/links/hardlinks/claimed request/no response, same-root lease, read-only status while busy, timeout after claim/crash, restart same versus changed candidate.
- Pass/oracle: one admitted action per request, atomic response/history, no out-of-root reads/writes or production caches/MIDI, no blind mutation retry. Token omitted from shareable command log; diagnostics don't leak it. Client result checked against actual state/output, asynchronous acknowledgement alone insufficient.
- Evidence/mode/severity: client/harness + verify-bridge native/live via run_checks, sanitized wire/claim/state/identity logs and inspected screenshots; static + automated/integration; P0 for isolation breach, otherwise P1.

### MW-R03 — Dependencies and generated assets have reproducible identities
- Contract/source: CMake/build/fetch scripts, analysis manifest, THIRD-PARTY-NOTICES and DEPENDENCIES ledger; derived JUCE MP3 patch/Auftakt tables/reconstructed work.
- Cases/procedure: inventory direct/transitive compiled/bundled/runtime dependencies, versions/commits/hashes, patches/toolchain/feature flags and fonts/icons/artwork/model/weights. Missing/changed downloads, dirty dependency checkout and unknown patch input; record generated source producers and independently compare known reference.
- Pass/oracle: no floating/unknown package input in accepted package, patch idempotent only for pinned expected source, exact provenance/version/consumer recorded or BLOCKED. Do not claim vendor-line review. Independently verify download content hash and current security advisories at execution; installed existence/local functionality proves no rights or safety.
- Evidence/mode/severity: source+SBOM/asset manifest, upstream licence/notices/advisory review with dates, resolved runtime/config/lock evidence; static + build verification; P1.

### MW-R04 — Clean build and package derive from the identified snapshot
- Contract/source: CMakeLists/build.ps1/fetch-analysis/deploy.ps1; project0.55.64 and README build claims.
- Cases/procedure: read-only preflight pins/tools/disk/memory, then explicitly scheduled clean Release x64 build and optional test fixtures; compiler failures/warnings, version RC regeneration, missing model/runtime, test flag OFF/ON. Build below normal with `--parallel 1` and `_CL_ /MP1`; inspect the effective policy of build.ps1 and its children (ra01 repairs the earlier parallel3 default).
- Pass/oracle: source archive and all compile inputs, toolchain/SDK/config/patch/dependency hashes bind output; version/resource metadata agree. Actionable warnings triaged, no accidental dev path or test fixture required by ordinary package. Bit-reproducibility not promised: independent rebuild content differences explained (timestamps/signing etc.), functional/asset identity retained.
- Evidence/mode/severity: full build logs, input/output/build-environment manifests and exact command, independent package manifest; build/manual inspect; P1.

### MW-R05 — Real package works as an ordinary user in a clean environment
- Contract/source: local launcher/deploy artifacts; package/Windows support form pending REQ-01/06, no installer found.
- Cases/procedure: actual selected zip/portable/installer in a separate user/clean supported Windows environment with no repo/.local/developer tools; Unicode/read-only/long path, missing optional stem runtime/VST/device. First launch/import/edit/save/reopen/export; package replace/rollback, session41 warning for old app, removal of package while preserving user music/data. Installer upgrade/uninstall only if one is claimed.
- Pass/oracle: no admin/dev dependency/hidden repo reference, adjacent DLL/model/resources/notices found, correct app/version/shortcut and usable errors. Upgrade doesn't silently destroy current data; portable removal leaves external user data by documented contract. Signing/SmartScreen/update requirements resolved before any trust claim; no invented updater tests for absent service.
- Evidence/mode/severity: package hash/tree, clean environment account/OS/prerequisites and journey capture/output, install/removal observations; installation/manual; P1. Host-local launch cannot satisfy this rule.

### MW-R06 — Imported paths, child processes and logs respect trust boundaries
- Contract/source: Model/media/session/playlist/preset parsers, write validators, bridge, stem child CLI/setup and network compile flags.
- Cases/procedure: crafted paths/quotes/Unicode/traversal/reparse/hardlink/UNC, malformed media/JSON/plugin descriptions, source replaced after validation, child arguments containing shell syntax; inspect child invocation for shell interpolation. Inventory network usage in setup/downloads versus offline application/worker. Inspect logs/reports for tokens/private paths/media/credentials and oversized input bounds.
- Pass/oracle: no arbitrary overwrite/code execution from ordinary imported text/path, no unvalidated external load prior to admission, expected source identities rechecked at publication; explicit installation downloads only, no unsupported account/telemetry/updater claims. VST native code is trusted in-process input and disclosed, not treated as sandboxed. Private tokens never published.
- Evidence/mode/severity: static trust/dataflow map and bounded malicious fixtures with protected canary hashes, network/child/log observations; static + isolated automated; P0.

### MW-R07 — Routine workload responsiveness has a declared qualification envelope
- Contract/source: user freeze reports, docs/TESTING.md, callback diagnostics, measured import/loading histories; no general speed promises established.
- Cases/procedure: W-small/typical in RUNBOOK: cold/warm launch/import/analysis/add/edit/seek/reopen/export, no work overlap; capture foreground responsiveness, wall/stage time, CPU/RSS/disk/free space, callback/driver misses. Baseline same inputs/tooling and3 sequential samples when explicitly scheduled; use one sample for initial diagnosis if repeated work disruptive.
- Pass/oracle: approved REQ-05 host/workload/time/memory/disk bounds met, no app-attributed callback deadline misses or unbounded queue growth; cancellation safely completes under approved bound. Current timing is a measurement, not an acceptance threshold. Responsiveness degradation stops only owned testing; no production restart/cache clear.
- Evidence/mode/severity: host/storage/backend/rate/block manifest, per-stage time/resources/max values and event-loop/callback counters; runtime/hardware; P1. Threshold unresolved or run deferred → BLOCKED/NOT RUN.

### MW-R08 — Sustained use and capacity are bounded, explicit checkpoint work
- Contract/source: SourceStreaming/LongSession tests, capacity_fixture90min, max prepared/decoded budgets, RF64 and take limits.
- Cases/procedure: **only with explicit capacity/stress checkpoint authorization**: W-large3h plan, existing90min stereo44.1k fixture (238,140,000frames;1,428,840,044bytes),30min playback/edit session,60min repeated lifecycle/resource observation,20 open/close cycles; optional >4 GiB RF64 live/offline recording with computed disk/time budget. Record overruns/temperature/responsiveness and stop condition.
- Pass/oracle: approved capacity envelope and memory/disk bounds, no crash/leak trend/unbounded queues or missed deadlines, expected final frame/header/sample markers; limits refuse before exhaustion. Repeating large copies or old stress-unguarded binaries prohibited. Capacity not a claim of all possible plugin/library combinations.
- Evidence/mode/severity: authorization/workload/host, resource time series, file/frames/retention and lifecycle outcomes; scheduled runtime/hardware; P1. Current policy defers this rule; no automatic execution.

### MW-R09 — Tests and receipts assert real, independent, current outcomes
- Contract/source: SelfTest dispatch, verification.py run_checks/check_report/validate_regressions, verify-analysis.py/change_verification.py/test_artifacts.py; historical superseded link fixtures.
- Cases/procedure: map every suite/branch to real feature invocation and expected assertion; inspect plugin-conditional omissions, early returns, positive check count/footer/exit consistency, skipped/empty branches/mocks/circular oracles. Harness corrupt/missing report/hash/wrong flag/duplicate suite/retention failure; targeted mutation of expected broken behavior when useful, only in isolated source/test work at execution.
- Pass/oracle: no false pass from zero checks, missing plugin/assertion, swallowed failure or changed executable;22 named suites accounted AND supplemental new-feature suites mapped to actual calls. Current workflow defines expected behaviour: repair obsolete tests only after independent requirement validation, never erase a real regression. run_checks cleans owned transient files, retains immutable failures/retry provenance and source evidence.
- Evidence/mode/severity: suite→function→assertion→rule matrix, independent oracle review, harness results and signed-off omissions; static + automated; P1.

### MW-R10 — Distribution rights and notices clear the exact package
- Contract/source: LICENSE, docs/LICENSING.md and THIRD-PARTY-NOTICES; no agreements accepted by agent.
- Additional commercial scope (2 November2026): close the relevant items in docs/commercial/CLEARANCE-REGISTER.csv, including extracted Synesthesia helpers/original scene content, compiled recovered AUFTAKT research, visual Python/native-codec runtime rights, published profile data, artwork/fonts and branding. External installation or download-on-demand is not itself clearance; preserve research and historical-publication evidence. The assessment and exact-package technical hashes do not substitute for professional rights review.
- Cases/procedure: exact source/runtime/model/plugin/font/icon/sample package manifest; owner-provided JUCE entitlement, ASIO distribution choice/Steinberg agreement evidence, SoundTouch static relinking/object/source/debugging requirements or separate licence; Beat This/ONNX/PocketFFT/r8brain/codec and optional Demucs/Torch/CUDA/weight/transitive terms. Verify actual selected modules, not generic product names.
- Pass/oracle: documented qualified rights review and required notices/source/relink artifacts for every distributed component, consistent with proprietary Personal Beta terms. Unknown weight licence/entitlement stays BLOCKED; working installed runtime not clearance. Separate technical acceptance from permission to publish, and require explicit user publication authorization.
- Evidence/mode/severity: exact package/SBOM, dated entitlement/clearance references and notice/relink manifest (sensitive agreements retained privately); owner/licensing review + static packaging; P1 mandatory public release blocker.

### MW-R11 — Documentation and diagnostics match supported release behaviour
- Contract/source: all claims in README/HANDOVER/FINISHING-PLAN/docs/help/package metadata and historical audits.
- Cases/procedure: extract each current advertised feature/format/version/shortcut/limit and follow its entry/result; compare current0.55.64/analysis11/session41 with stale README34/version0.55.46, old stem/performance claims and master/export format history. Triage runtime/build warnings and reproduce known issues; absence of live root-cause proof remains disclosed.
- Pass/oracle: current-facing instructions accurate, history explicitly dated, limitations/blocked integrations disclosed; no claim based solely on prior completion text. Diagnostics name action/source/driver/error with safe paths, no credentials, useful recovery. Full user manual release requirement is REQ-07; roadmap aesthetic/feature wishes not automatically defects.
- Evidence/mode/severity: claim→source→rule/evidence table, known-defect/suspicion dispositions, reviewed user docs/warnings; static + manual/runtime; P2/P3, P1 for material false release claim.

### MW-R12 — The audit cannot hide an uncovered area or incomplete result
- Contract/source: user audit request, README/RULES/COVERAGE/CONTROLS and inventory refresh/check utility.
- Cases/procedure: refresh files/options/control sites; compare all nonignored files and CMake membership; expand each dynamic control family/entry and every A–O scope row/journey; inspect cross-boundary cases, thresholds, blocked methods and absent features. Validate links/unique IDs/references, evidence invalidation and report status counts. Challenge at least one test per critical boundary for circular expected values.
- Pass/oracle: zero unclassified/unmapped first-party files or controls, no duplicate/vague unassessable mandatory rule, every required mode recorded; missing tooling/hardware becomes visible block, not exclusion. Framework checks PASS establishes structural integrity only; product rules remain NOT RUN until executed.
- Evidence/mode/severity: inventory/check outputs, self-review table and gaps corrected/dispositioned, independent reviewer not required; documentation/static; P1.

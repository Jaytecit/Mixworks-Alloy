# Mixworks development continuity

Read `HANDOVER.md` before implementation and keep it current. The project repository is https://github.com/Jaytecit/Mixworks-Alloy; use the Git repository rooted in this folder, not the parent REA repository. Preserve source checkpoints and local verification evidence independently of Git. Keep `.local/`, `build/`, `out/`, user music, sessions and caches out of commits.

Original application material uses the root Personal Beta Licence. Third-party material retains its own licences. Read `docs/LICENSING.md` before any public binary/package release: JUCE entitlement, ASIO proprietary terms and SoundTouch relinking obligations are unresolved release prerequisites. Do not publish the existing local executable under restrictive terms or accept/purchase agreements on the owner's behalf. Local technical verification does not establish distribution clearance.

## Current development scope - user instruction, 24 October 2026

Legacy features and backward compatibility with old mixes/arrangements are not required during development. Do not add compatibility layers or spend verification effort preserving legacy behaviour unless explicitly requested. Prioritise the current workflow; this does not authorise deleting original music, protected reference evidence, or restarting/modifying the user's running session.

## Development bridge — future Codex sessions

Read [docs/DEVELOPMENT-BRIDGE.md](docs/DEVELOPMENT-BRIDGE.md) before controlling Alloy. Prefer the repository-owned `scripts/alloy_bridge.py` client for supported state inspection, fixture setup, controls, save/reopen and screenshots. Launch a current trusted candidate with `start` into a fresh short `.local/` root; verify protocol/root/PID/capabilities. Never repurpose production data or an existing promo workspace. Wait for asynchronous work and assert its outcome, save before graceful close, and inspect captured pixels when visual acceptance matters.

For bridge changes run the client/harness tests and `scripts/verify-bridge.py` through the shared `verification.run_checks` retention path. This is supplemental integration coverage; select other focused/full checks by the policy below. Keep source/bundle identities and local evidence independently of Git. A bridge pass does not establish mouse/keyboard gestures, physical MIDI/ASIO or musical listening acceptance. Do not auto-retry mutations after a timeout; inspect the response/claim and actual state first.

## Low-disruption verification — user instruction, 23 October 2026

**1 November override:** Jay explicitly removed the free-memory guard as too
restrictive. Do not refuse, defer or terminate builds/tests merely because
available RAM crosses a numeric threshold. This supersedes historical local
memory-preflight/kill wrappers and handover instructions to wait for memory.
Keep single-worker, below-normal-priority builds, serial focused tests, stress
opt-in and the instruction to stop owned testing if actual responsiveness worsens.

The user reports severe computer freezes during project testing and requests absolutely essential checks only, less frequently, while making successive requests. This instruction supersedes automatic full-gate requirements below during ongoing development. Batch related edits and verify once at a useful checkpoint; do not rebuild or rerun suites after every small request. Prefer source inspection and the smallest relevant focused check. No automatic full regression, stress, capacity or benchmark runs during this iterative work. Reserve a complete gate for an explicitly requested release/verification checkpoint; state deferred coverage honestly and never substitute a focused result for a complete receipt.

Use one build worker (`--parallel 1`), one test process at a time, and no overlap between builds/tests. Native checks use `verification.run_checks`, which launches ordinary children below normal priority on Windows and disables deliberate Alloy stress by default. Stress requires explicit `include_stress=True` (full-gate CLI: `--include-stress`); do not enable it merely because audio/engine code changed. Lower priority is not a memory or disk limit: stop owned testing if responsiveness worsens. Keep production apps, audio devices and user data untouched. Avoid launching older Alloy test binaries that predate the stress opt-in guard.

## Proportionate verification — user instruction, 20 October 2026

Choose verification by changed behaviour, not by the fact that an executable changed. Follow [docs/TESTING.md](docs/TESTING.md). Colour, text, spacing and other presentation-only edits need a successful build and an actual visual check of the affected UI; do not run the full regression suite or add tests that mirror paint constants. Local UI interactions need the affected workflow and relevant focused suites. Tooling changes need relevant harness tests. Documentation-only changes need relevant document checks.

Run the complete gate for audio/DSP, detector/grid/timing, engine/cache/ownership, saved-format changes, cross-cutting changes or unresolved regression risk, and once on the final candidate for an explicitly requested release milestone. Do not repeat a full gate for cosmetic follow-ups. Record scope and limitations accurately; scoped receipts are supported by deployment and must not be described as complete regressions. A failed suite on an unchanged candidate may be rerun alone after an evidenced environmental failure; retain original failures and retry provenance.

Ordinary native test runs must use `scripts/verification.py:run_checks` so generated audio, fixture `.mixworks` arrangements/backups and isolated TEMP caches are removed after process exit while analysis JSON, reports, hashes, screenshots and source snapshots survive. Keep research audio only when a subsequent measurement needs it; identify that exception explicitly. Never clean production AppData, real arrangements or original music as test retention. Do not retain repeated full model/runtime build copies; preserve current/previous working bundles and use exact-hash canonical assets plus restoration mappings for retired historical duplicates.

The 19 October loading/performance request explicitly preserves analysed data. Keep existing analysis, corrections, arrangements and audio caches for cache-compatible performance updates; these are not a reason to apply the detector-development reset below. Reuse valid analysis for timeline loading and invalidate only where source identity or detector compatibility requires it. The 0.48 optimisation retains analysis revision 9 and session format 22.

During the 19 October peak-meter/forge-effects update, the user reported that the running instance was scanning **354 tracks** and explicitly required keeping its generated data after completion. Leave that instance and scan running. Preserve all generated analysis, corrections, arrangements, audio caches, library snapshots and recovery; this visual update retains analysis revision 9 and session format 24 and requires no AppData reset. Activate the new version only when the user is ready after the scan; do not restart the app automatically.

The user's current priority is accurate detection and grid alignment for **constant-tempo 4/4 music**. Variable-tempo playback and other meters come later. Beat alignment needs accurate BPM and pulse timing; bar alignment additionally needs a reliable downbeat anchor anywhere in the source. Extend that phase backwards and forwards and map it correctly onto project bars. Finding the earliest downbeat is optional for arrangement, not a requirement for grid accuracy. A four-on-the-floor kick is not necessarily beat one.

During detection development, the user explicitly considers retained analysis, manual corrections and arrangements disposable. **Clear retained app analysis/corrections, audio preparation caches, library snapshots and recovered mix state with each verified detector update, not on every launch.** Validate the exact owned AppData targets and running processes before cleanup. Use fresh analysis and fresh mixes for evaluation so old manual overrides or captured playback clocks cannot mask detector changes. Keep original music, source code and verification evidence. Record exactly what was cleared in the handover.

On 22 October the user requested preserving their manually reviewed downbeats for about 20 tracks against automatic detection. **Keep `%LOCALAPPDATA%/Mixworks/DownbeatReferences-v1` and independent `.local/downbeat-reference-20261022` baselines when clearing disposable analysis/corrections.** These dated source-identity comparison observations are deliberate reference evidence, not detector caches. Never overwrite earlier reviews or imply that a reviewed-choice snapshot proves an active source-save succeeded. See `docs/DOWNBEAT-REFERENCES.md`.

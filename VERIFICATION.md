# Verification and evidence

Current policy: [docs/TESTING.md](docs/TESTING.md). Current candidate and exact
results: [HANDOVER.md](HANDOVER.md). Dated reports in `docs/release-audit/` and
private evidence under `.local/` belong to their original candidates, not this one.

## Proportionate checks

Jay's responsiveness override requires essential batched checks during iterative
work. Build with `scripts/build.ps1`: one worker, `/MP1`, below-normal priority.
Numeric free-memory guards were explicitly removed. Do not overlap builds/tests;
stop owned testing if actual responsiveness worsens. Do not run stress/capacity or
the complete regression gate automatically for visual edits or a local activation.

Native checks use `scripts/verification.py:run_checks`, one child at a time, with
isolated TEMP and stress off. It removes owned generated audio, fixture sessions
and caches while retaining reports, hashes and screenshots. Never clean production
AppData/music/arrangements as test retention. A deliberately retained review excerpt
must be identified in the evidence record.

## Current visual checks

- `python -m unittest discover -s tests -p test_original_visual.py` and
  `test_visual_media.py` with the bundled visual interpreter.
- `scripts/stage_visual_runtime.py --out <fresh-repository-.local-root>` freezes
  only the active catalogue and current runtime modules.
- `scripts/verify_visual_behaviour.py --runtime <frozen-runtime> --out <fresh-root>`
  checks live long blends, short handoffs, particle paths and complete glass imagery
  using authored fixtures. It opens no audio device.
- `scripts/verify_scene_looks.py --runtime <frozen-runtime> --out <fresh-root>
  --image <authorised-image> --scene <slug>` checks selected presets/methods, finite
  targets and exact pause. Omit selectors only at an appropriate broad checkpoint.
- Native `--original-visual-only` and `--visual-arrangement-only` cover the current
  owned runtime/workflow. The external-renderer suite and oversized historical
  visual-coverage selector were retired in the [maintenance audit](docs/MAINTENANCE-20261103.md).

Soundtrack renderers create preview evidence, not audio-device/listening acceptance.
Static references are snapshots, not a promise that cycling colours stay fixed.

## Complete regression contract

The authoritative 23-suite catalogue and fixture roles are in
`scripts/verification.py`; full-gate orchestration is `scripts/verify-analysis.py`.
Run the complete gate only at an explicitly requested qualification checkpoint.
Full receipts require stress opt-in, all named suites, reviewed invocations and
matching executable/model/runtime/report identities. Missing/truncated reports,
startup failures, timeouts and retention errors cannot pass.

Schema-2 complete receipts and schema-3 scoped receipts are distinct. Validate a
standard deployment receipt with `scripts/deploy.ps1 -VerificationManifest <receipt>
-VerifyOnly`. Local visual activation may preserve an explicit focused activation
receipt binding native checks, source and complete runtime payload; it must not be
called a complete regression. Keep the previous bundle and launcher backup.

## Scope and recovery

A passing technical receipt does not establish independent musical truth, hearing
acceptance, hardware/driver behaviour, sustained performance or public distribution
clearance. See [capacity](docs/SOURCE-CAPACITY.md),
[protected references](docs/DOWNBEAT-REFERENCES.md),
[development bridge](docs/DEVELOPMENT-BRIDGE.md) and
[licensing](docs/LICENSING.md). Real-recording benchmarks are opt-in and use explicitly
identified originals plus owned outputs; do not resume old corpus runs implicitly.

The 3 November cleanup preserved the previous complete document/test/source bytes
in `.local/glass-release-20261103/before-source.zip`, with per-removal hashes and
reasons. Earlier verification narratives were consolidated here; their dated
receipts and failed evidence remain intact. See the maintenance record for recovery.

> Historical snapshot before the 19 October 2026 cleanup. Statements of current state and pending work below belong to their original checkpoints; use the root HANDOVER.md and FINISHING-PLAN.md today. Exact original bytes are in the cleanup source archive.

# Mixworks verification and source recovery

## Recoverable source baseline

The pre-implementation baseline is `.local/source-baselines/20261018T184854Z-b750e85d/`. It contains `source.zip`, a SHA-256 manifest and a successful restore check of all **117 files**. It includes source, scripts, tests, documentation, notices and the approved design assets. Git-ignored dependencies, generated builds, recordings, sessions and local evidence are excluded.

Mixworks remains in its existing workspace. No nested repository was created and no unrelated parent REA changes were committed. The source remains untracked by the parent repository; this verified archive provides recovery independently of that Git state. Keep the archive and manifest together when backing up the project.

To inspect the baseline, open `manifest.json`. To recover, extract `source.zip` into a **new empty folder**, then verify each file against the embedded `BASELINE.json` before copying selected files back. The retained `restore-check/` is an already verified extraction. Never restore over current work without preserving it first. Dependency commits and download hashes remain in the archived build scripts and `scripts/analysis-dependencies.json`.

## Consolidated regression verification

Use Python **3.11 or newer**, the existing Windows build tools, the local test-gain, test-delay and test-latency VST3 modules and installed Vital. Build with `scripts/build.ps1 -TestPlugin` when source changes require it. Verification does not install missing plugins or change the launcher.

Freeze the candidate executable, its adjacent `onnxruntime.dll` and `beat-this-final0.onnx` in a fresh directory before testing. From the Mixworks folder, this PowerShell sequence selects the current built output and creates a new evidence directory:

```powershell
$runRoot = Join-Path (Get-Location).Path ('.local/verification-' + [guid]::NewGuid().ToString('N'))
$candidateRoot = Join-Path $runRoot 'bundle'
New-Item -ItemType Directory -Path $candidateRoot -ErrorAction Stop | Out-Null
$builtAppPath = (Get-Content -LiteralPath build/mixworks-Release.path -Raw).Trim()
Copy-Item -LiteralPath $builtAppPath -Destination (Join-Path $candidateRoot 'Mixworks.exe')
foreach ($assetName in @('onnxruntime.dll', 'beat-this-final0.onnx')) {
    Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $builtAppPath) $assetName) -Destination (Join-Path $candidateRoot $assetName)
}
python scripts/verify-analysis.py --exe (Join-Path $candidateRoot 'Mixworks.exe') --output (Join-Path $runRoot 'tests') --receipt (Join-Path $runRoot 'receipt.json')
if ($LASTEXITCODE -ne 0) { throw 'Verification failed; inspect the retained summary and individual reports.' }
```

The single named suite catalog lives in `scripts/verification.py`: rhythm, beat grid, placement, tempo/UI, tempo/browser, track ownership, analysis refresh, grid correction, experimental rhythm decoder, library assets, playback/cache, long-source streaming/cancellation, timeline editing, Alloy, window, Fusion workflows, mastering/spectrum, cancellable WAV export with a real delay VST3, reported-latency guards with a real delayed-impulse VST3, core with real VST3/Vital, and missing/corrupt model handling. Native suites run sequentially, each in its own process. The streaming suite creates or validates an owned 90-minute PCM24 capacity fixture, records its identity before and after verification, and never replaces an unknown or incomplete existing fixture. The separate real-source long-session acceptance runner and its coverage limits are described in [the capacity guide](../SOURCE-CAPACITY.md). Analysis-cache checks also run inside core.

Each suite retains its invocation, executable hash, process exit, individual check report, report hash and process log. Passing requires one final footer, positive check coverage, matching individual result counts, no failed assertions and a consistent process exit. Core's MIDI availability inventory is retained separately in `midi-input-inventory.json`. Missing/truncated reports, startup failures and timeouts cannot pass; partial evidence remains available. The candidate executable, runtime and model identities must remain unchanged throughout verification.

The optional receipt is written only after all **22 named suites** pass. Receipt schema 2 binds the exact bundle, a hashed regression summary and all underlying check reports. Deployment revalidates that evidence and rejects missing/duplicate suites, mismatched invocations, changed reports/assets and legacy receipts. The former hand-set `verifiedImprovement` field is superseded: the new receipt explicitly establishes **complete regressions**, without implying improved musical detection or listening acceptance.

Validate deployment eligibility without copying a release or touching the launcher:

```powershell
& ./scripts/deploy.ps1 -VerificationManifest (Join-Path $runRoot 'receipt.json') -VerifyOnly
```

The built output selected by `build/mixworks-Release.path` must still match the tested candidate. Actual deployment uses the same command without `-VerifyOnly`; it retains the existing immutable-release and launcher-backup behaviour. Preserve the evidence directory: deleting or changing reports invalidates its receipt. Old release receipts remain historical evidence and must be regenerated before use with the strengthened gate.

## Real recordings and musical readiness

Run `scripts/run-real-benchmark.py --exe <absolute-frozen-executable> --paths <absolute-path-list-json> --output <fresh-absolute-evidence-directory>`. The path-list file is a nonempty JSON array of absolute audio paths. Add `--workflow` for arrangement/render workflows; the default performs fresh analysis probes. This requires the existing `soundfile` Python package; downstream comparison scripts additionally use their existing NumPy/SciPy dependencies.

The manifest retains requested paths, input hashes, each child result, decoder evidence and unique-source coverage. An odd final recording is probed twice because the native interface takes a pair; it counts as **one recording**, with padding identified explicitly. Successful analysis-only probes release their owned decoded WAV copies after retaining their hashed PCM heads. Failed/review probes preserve decoded evidence; workflow renders remain available.

| Runner exit | Meaning |
| --- | --- |
| 0 | Complete execution; all assertions and the probe's pulse/downbeat readiness checks passed |
| 2 | Complete execution; only explicitly classified musical-readiness checks need review |
| 1 | Failed assertion outside readiness, incomplete/malformed evidence, source identity change, timeout or process failure |

Readiness classification comes from `RealTrackTests.cpp`'s structured `readinessChecks`, not inferred failure wording. Those checks currently cover usable source pulse grids and availability of a predicted downbeat. Older executables lack that classification: their failed checks remain strict failures. Neither exit 0 nor a downbeat's availability establishes its musical correctness. Key accuracy, annotated beat-one truth, audible quality and the long-mix acceptance in [the completion plan](../../FINISHING-PLAN.md) remain separate gates.

## Harness regression tests

```powershell
python -m unittest discover -s tests -p test_verification.py -v
```

These tests use isolated fixture files and simulated child outcomes to check failed/incomplete/contradictory reports, timeout/start failure handling, named coverage, evidence tampering, musical-review exits and odd-source counting. They do not substitute for running the actual application suites, real plugins or real recordings.

## Verified milestone 1 checkpoint

The completed implementation is also backed up in `.local/source-baselines/20261018-milestone-1-verified/`, with its own source archive, per-file hashes and restore verification. The original pre-change baseline remains available separately.

Current local evidence is under `.local/verification-consolidation/`: `tests-02/summary.json` records **873 passing checks across 16 suites**; `harness-tests.txt` records **eight passing harness tests**. `receipt-02.json` passed deployment validation with `-VerifyOnly`. Frozen `bundle-02/Mixworks.exe` has SHA-256 `29165de3769ba5b93fd91ac6d6fabf8f8ca7e460a3bf3e346ec5062098d4a697`.

`real-02/manifest.json` records complete coverage of **three unique recordings**, no strict failures and **review required / exit 2** for Nude's unavailable downbeat. Odd-probe padding is explicitly identified. `tests-01/` retains the rejected diagnostic-trailer report that led to fixing its C++ producer. The deployed release remains `0.32.0-b55c54b8c78c`; no deployment or user-session change was made. Musical accuracy and long-mix acceptance remain open.


Mastering follow-up23 adds the required mastering suite (19 suites total). Current deployed0.35.0-2c56b3a702ab passed1,375 checks with exact source/bundle/receipt correspondence. See HANDOVER.md for identities and preserved superseded experiments. Passing numeric and native UI checks does not establish musical listening accuracy or physical device behavior.

# Alloy development bridge

Use the repository's local bridge to set up reproducible sessions, inspect real application state, exercise supported workflows and capture the actual Workstation component. It is available to ordinary Codex development sessions without OBS, Remotion, browser automation, MIDI hardware or a separate MCP server. The implementation is `Source/PromoControl.cpp`; its historical class name, `.alloy-promo` marker and `--promo-control` flag remain compatible with the separate promo project.

## Start here in a future Codex session

1. Read [HANDOVER](../HANDOVER.md) and [testing policy](TESTING.md). Use this repository, not the parent REA repository. Identify the behaviour being changed and the exact candidate to test.
2. Prefer `scripts/alloy_bridge.py` for supported application actions and state inspection. Use a new, short, owned directory under this repository's `.local/`; never attach it to production AppData, a real arrangement or the sibling promo workspace.
3. Build the changed candidate, launch through the client, and check the returned protocol, root, PID and capabilities. Use known disposable fixtures copied/generated under that root's `audio/`. Keep originals elsewhere.
4. After asynchronous actions, wait for readiness and assert the expected outcome. Read the returned status text; a command acknowledgement or `busy: false` alone does not prove an import/open succeeded.
5. Save under a new name, collect state and screenshots, inspect relevant pixels, then close the owned app. Keep evidence and source checkpoints independently of Git. Record the tested behaviour and remaining limits in HANDOVER.

The bridge starts a separate application with isolated analysis, audio preparation caches, library/workspace preferences and MIDI profile. It does not restore the production session. Physical MIDI input is disabled. Normal application launches retain their usual behaviour. The app still opens an audio device; use a desktop session with working audio for live transport checks. Device discovery text in the UI does not prove MIDI input is enabled or tested.

Normal Save and Export dialogs in a bridge instance default to its owned
`sessions/` folder, reported by `state.saveFolder`. Inspect the destination before submitting a dialog; desktop
interaction can still explicitly choose a path outside the bridge.

## Quick start

Prerequisites: Python 3.11 or newer (standard library only) and a trusted local build containing `--dev-control`. Existing build prerequisites are described in the [app README](../README.md). From the repository root in PowerShell:

```powershell
./scripts/build.ps1
$candidateExe = (Get-Content build/mixworks-Release.path -Raw).Trim()
python scripts/alloy_bridge.py init .local/bdev1
python scripts/alloy_bridge.py start .local/bdev1 --exe $candidateExe
python scripts/alloy_bridge.py call .local/bdev1 status
```

Choose a fresh root name each time you change the candidate. `init` refuses an existing directory. The launcher rejects old binaries lacking the bridge markers before they can interpret the new flag as an ordinary production launch. This is a compatibility check, not executable signature verification.

The build script uses one build worker, MSVC `/MP1` and below-normal priority,
then restores its caller's environment and priority. Schedule the build at a useful checkpoint; do not overlap it with tests. Numeric
RAM guards were removed; stop owned work only if actual responsiveness worsens.

The launcher copies the executable to `root/bundle/Mixworks.exe` and hard-links the matching model/runtime on the same volume. It stages adjacent packaged visual resources when present. It records their SHA-256 identities in `evidence/bundle.json`; keep canonical linked assets intact. This permits rebuilding without replacing the running executable. A stopped root can restart with the **same** candidate; a different candidate requires a fresh root. Sessions are loaded explicitly with `open`, even after restarting an existing root.

For arguments, prefer a JSON file to shell-escaped inline JSON. Example:

```powershell
Set-Content -Encoding UTF8 .local/bdev1/save-request.json '{"name":"checkpoint-01"}'
python scripts/alloy_bridge.py call .local/bdev1 save --args-file .local/bdev1/save-request.json
python scripts/alloy_bridge.py call .local/bdev1 close
```

Saving and closing is also safe for an empty session. Saved sessions and screenshots never overwrite existing names. `close` refuses unsaved changes; save under another name before closing or opening another session. There is no bridge discard/force-close command. The CLI does not kill a discovered PID. The automated test runner may terminate only its own child handle on test failure, and reports that as a failed graceful completion.

For a script running from the repository root:

```python
import sys
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from alloy_bridge import Client

client = Client(Path('.local/bdev1'))
state = client.call('status')['state']
assert state['protocolVersion'] == 1
assert 'import' in state['capabilities']
# Supply an owned disposable audio file here before importing it.
audio = (client.root / 'audio' / 'demo.wav').resolve()
before = len(state['library'])
client.call('import', path=str(audio))
state = client.ready(timeout=120)
assert len(state['library']) == before + 1, state['status']
client.call('add', index=before)
state = client.ready(timeout=120)
assert state['clips'], state['status']
client.call('save', name='demo-01')
client.call('screenshot', name='demo-01')
client.call('close')
```

Do not apply known BPM/downbeat corrections when evaluating automatic detection. The smoke test intentionally uses authored timing corrections to test control plumbing; it does not measure detector accuracy.

## Supported commands (protocol 1)

All indices are zero-based except built-in preset IDs. Successful replies contain `id`, `ok: true` and actual `state`; failed replies contain `ok: false` and `error`. Client failures raise `BridgeError`; a timeout raises `TimeoutError` and identifies the request.

| Command | Arguments and behaviour |
| --- | --- |
| `status` | No arguments. Available while busy. Returns version, protocolVersion, root, PID, capabilities, dirty/busy/playing/ready, position (seconds), status, selection, library, clips, lane controls, dimensions, peak, monitorMuted, audibleEnd, visual delivery facts and timestamp. |
| `import` | Absolute `path` to existing WAV/FLAC/AIFF/MP3/OGG below owned `audio/`. Asynchronous: wait and check library. |
| `add` | Library `index`. Asynchronous: wait and verify clips. |
| `correct_source` | Library `index`, `bpm` (40–300), `downbeat` (source seconds, 0–10800). Applies a real correction in the isolated store. |
| `select_clip` | Timeline clip `index`; selects that clip and its lane. |
| `select_transition` | Incoming clip `index`; selects its transition. Verify `selectedTransition`. |
| `transition_preset` | Built-in `index`: 1 None, 2 Bass Swap, 3 Crossfade; requires a selected transition. |
| `insertion_preset` | Built-in `index`: 1 Align IN/OUT, 2 Bass swap, 3 Crossfade. Sets the actual new-track picker before Add. State reports `insertionPreset` and `sectionInsertion`; existing transitions stay unchanged. |
| `preview` | Owned-library `index` and `fraction` 0–1 start/seek source audition; `stop:true` stops it. Same-source seeking does not toggle off. Poll `state.preview` sourceId/loading/fraction/duration/privateOutput and inspect status; acknowledgement alone does not prove decoded output. |
| `work_mode` | `name`: `arrange` or `record-perform`. Selects the real mode menu while stopped and outside take capture. Wait for ready and assert `state.workMode`; menu completion is asynchronous. `capturingTake`/`takeCount` expose the actual capture lifecycle. |
| `timeline_cue` | Selected segment's zero-based `slot` 0–8, through the actual whole-mix cue owner. Verify position, playing and capture state; an empty slot opens its real editor and is not a successful seek. |
| `transition_confirm` | Explicit boolean `replace` and exact `incoming` from `state.transitionQuestion`. Only answers the current **Replace transition curves?** dialog; other dialogs/targets are refused. Inspect its actual message first. Wait for ready and assert publication or cancellation; answering does not bypass the editor's stale-state checks. |
| `control` | `lane` 0–3, `parameter`, `value`. Volume is dB (−60 to +12); pan is −1 left to +1 right; cutoff is −1 to +1 (negative LP, 0 off, positive HP). Calls the real control path with Undo. |
| `undo` | Actual application Undo. |
| `play` | Boolean `enabled`. Starting requires nonempty, prepared playback. |
| `stop` | Stops transport. |
| `seek` | `seconds` inside the audible mix. Live apps prepare asynchronously while the old position continues. Wait with `ready()`, then assert empty `seekError`, `seekPending:false`, the actual position and playing state. |
| `jump_bars` | `bars`: 4, 8, 16 or 32; `direction`: −1 or +1. Uses native mix-bar navigation, including end limits. |
| `timing` | Clip `index`, `source_seconds` within a forward, non-looped source. Returns source/project/grid/waveform coordinates, current cursor source time, detected sections and actual paired bass-swap times. Read-only. |
| `audio_probe` | Stopped transport; `seconds` inside the mix, integer `frames` 1–8192, optional `lane` 0–3. Returns stereo float PCM and sample rate from a separate real export renderer. Leaves live transport and controls unchanged. |
| `move_clip` | Stopped transport; clip `index`, target `seconds` (0–10800), explicit boolean `snap`, `mode`: body or following. Uses native model movement and real Undo. |
| `transition_edit` | Stopped transport and selected transition; `action`: window, start, end, outgoing or incoming; `delta` seconds (±3600), explicit boolean `snap`. Uses native model transition edits and real Undo. |
| `view` | Timeline `start` (0–10800 seconds), visible `seconds` (2–10800); disables follow-playhead. Optional boolean `monitorMuted` uses the actual isolated app's monitor control; mute before audition verification. |
| `panel` | `name`: controller, transitions or editing. |
| `save` | New simple `name`; writes `sessions/name.mixworks`, returns `path`, clears dirty state. |
| `open` | Existing session `name`; requires a clean current session and owned references. Asynchronous: wait and assert expected state. |
| `screenshot` | New simple `name`; writes `evidence/name.png`, returns `path`. Captures the actual Workstation component, not desktop/window chrome or every OS popup. |
| `close` | Requires clean, idle state. Stops transport and schedules normal app exit after writing the response. |

Names allow ASCII letters, numbers, hyphens and underscores, maximum 96 characters; omit extensions. Lane values in `state.lanes` are manual values, not the complete automation curves or effective values during automated playback. Query capabilities rather than assuming future commands exist. Unsupported actions still need appropriate native tests or UI/hardware interaction.

`state.seekPending` also contributes to `busy`. `state.seekError` reports the
latest completed preparation failure. A seek acknowledgement is acceptance of
the request; it does not establish destination publication. A newer seek
supersedes the earlier destination; Stop or Play/Pause cancels a pending seek.
Source/device/arrangement changes are revalidated before publication. Timeline
cues and bar jumps use this same owner: wait and inspect their result too.

`state.audioCallbacks` reads the engine's existing lock-free cumulative callback
counters: `callbacks`, `overBudget`, `lockMisses`, `oversized`, `exportSilence`,
`totalMs` and `peakMs`. Subtract snapshots around an interaction. The counters
measure work after callback entry, not driver underruns or physical output;
combine them with source-wait counts, actual clocks, PCM and moving pictures.
Status inspection does not reset counters or acquire the audio render lock.

`state.visuals` is read-only: `validFrame`, `status`, the renderer's `frame` facts,
`paintedFrames`, `lastPaintMs`, `maxGapMs`, and `gapHistogramCeilMs`. Paint counts
deduplicate the same delivered image across preview and output. Histogram keys
are upward-rounded integer milliseconds between distinct paints during playback;
pause/clock-generation changes break the interval. Counters are cumulative for
the scene lane lifetime, so subtract snapshots for a measured section. The last
paint timestamp uses the native monotonic clock. Open a visible visual view before
measuring; these facts establish native paints, not physical display scanout.
`frame.position` and `frame.scenes` identify the actual rendered musical moment.

`timing` reports the application's coordinate mapping; it is not independent onset truth. Compare it with annotated source samples and `audio_probe` when investigating timing. Each PCM probe starts a fresh renderer without preceding filter/plugin history; it verifies offline sample timing, not continuous effect tails, physical device latency or listening acceptance. Movement commands test model paths; native mouse tests remain necessary for gesture handling.

## Focused testing and evidence

For bridge changes, run the client/harness tests and real native plus interactive checks:

```powershell
python -m unittest discover -s tests -p test_alloy_bridge.py -v
python -m unittest discover -s tests -p test_verification.py -v
python scripts/verify-bridge.py --exe "build/detection-Release/Mixworks Alloy.exe" --output .local/bv1
python scripts/check-project.py
```

Use a fresh short output directory. The live suite creates two procedural fixtures, imports/adds them, applies controls and Undo, saves/reopens, runs muted transport, captures the UI, checks authentication/replay and inbox archiving, closes, restarts the same frozen bundle and reopens the saved session. Inspect `live/w/evidence/bridge-smoke.png` when pixels matter. The native suite retains the compatible `--promo-control-only` name.

Both suites run through `verification.run_checks`. Its optional interactive driver contract is `run(exe, flags, folder, environment, log, startup, deadline) -> exit code`; a driver owns and reaps its children, observes its deadline and writes standard `checks.txt` PASS/FAIL rows plus the final count. This lets future scenarios share hashing, report validation and owned artifact retention. Do not bypass `run_checks` for ordinary native tests. Once children exit, generated audio, test arrangements/backups and TEMP caches are removed; reports, measurements, screenshots, command evidence and bundle identities remain. A manually created development workspace is **not** automatically cleaned on close.

The bridge report is `summary.json` with scope `bridge-integration`. It is supplemental integration evidence, not a deployment receipt or a substitute for the 23-suite native gate. `validate_regressions` rejects interactive invocations masquerading as native regression suites. Apply [the existing risk policy](TESTING.md): a tooling-only change needs focused harness checks; changes to startup isolation, cache/engine ownership or shared persistence additionally need the complete gate. Unrelated cosmetic work does not require rerunning the bridge suite or full gate.

Retain candidate/source hashes, a source archive, exact commands, reports, the visual review note and limitations. Record failed attempts separately; never replace them with a passing report under the same name. `.local/`, bundles, tokens, music and sessions stay out of commits. Keep canonical model/runtime assets with restoration mappings rather than repeated full copies.

## Protocol and failure recovery

The transport is a local file inbox, not a network service. The client atomically publishes `control/requests/<id>.json` with a random ID, token and timestamp. Requests are at most 64 KiB and accepted from 60 seconds in the past to 5 seconds in the future. The app processes at most one actionable request per 40 ms tick on its message thread. It writes a durable `.claimed` file before execution and an atomic response under `control/responses/` afterward. A per-root lease prevents two apps owning the same inbox.

The app now moves completed/replayed requests to `control/history/` itself. This prevents the growing-inbox slowdown found during promo capture and allows clients to remain simple. Old promo clients may also archive completed requests; those files and native history are local evidence. Full wire requests contain the token: keep the workspace private. `evidence/commands.jsonl` omits the token; responses retain outcomes. `evidence/process.json` identifies the latest launched child.

On timeout, **do not automatically repeat a mutation**. Inspect the named response/claim, the recorded child process and current `status` using a new read-only request. An existing response is the cached outcome; a claim without a response may mean the action ran before a crash. Inspect saved files and actual state before deciding on any new mutation. Do not remove claims or forge timestamps to force replay. A rejected busy response can be followed by `ready()` and a new request after the app is idle. If a modal dialog keeps it busy, resolve that dialog through the appropriate UI workflow.

A bridge smoke pass establishes the stated in-process and file-protocol behaviour. Mouse hit testing, keyboard focus, dragging, physical MIDI/ASIO, audible quality and musical acceptance require their own checks. Known synthetic timing and muted transport do not establish detector accuracy, hardware latency or listening acceptance.

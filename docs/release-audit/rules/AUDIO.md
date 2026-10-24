# Programme audio, DSP and VST3

Inherit RULES. F-AUDIO/F-PLUGIN; scopes audio plus Model/Automation/Engine/cache and output/private/take consumers. All mandatory, NOT RUN. Digital PCM evidence never proves physical-device or musical-listening acceptance.

### MW-A01 — Channel routing and gain staging are mathematically correct
- Contract/source: Engine::render, Clip/TrackSettings, StemMixing and Mastering; four lanes and stereo programme.
- Cases/procedure: distinct L/R tagged impulses/tones on every lane, same-lane overlaps, original-owner splits and separate placements; all mute/solo boolean combinations across four lanes (256 combinations for eight bits), parent/stem solo interaction, hard-left/centre/right pan, min/unity/max gains and silence.
- Pass/oracle: independently summed/routed expected PCM within2e-6 full-scale float on deterministic unity paths; exact silent lanes, no L/R swaps, duplicated/missing voice, clip/track/stem-owner scope error. No normalize/limiter claim unless enabled by contract.
- Evidence/mode/severity: enumerated route matrix and channel residual/RMS/peak against analytic samples, device listener check; automated + listening; P1.

### MW-A02 — EQ and cutoff implement declared neutral and active responses
- Contract/source: ToneEq/CutoffFilterProcessor/TransitionFilterProcessor, Mastering and centre-Off DJ filter UI.
- Cases/procedure: impulse and logarithmic tones, low/mid/high boost/cut bounds, lowpass/off/highpass crossover, rapid changes/reset/bypass, sample rates44.1/48/96k and odd blocks; compare shared original-track/clip/lane/transition/master owners.
- Pass/oracle: neutral preserves signal (2e-6 float limit); active spectral response matches independently evaluated coefficients/design specification and finite gain within declared domain, cutoff centre exact Off. Numerical dB tolerance0.1dB at settled nontransition test frequencies is provisional and justified by FFT/window precision; freeze frequency/time windows first. Listening must detect no control-induced clicks under F-AUDIO envelope protocol.
- Evidence/mode/severity: analytic response versus FFT/errors, transient/bypass captures and listening; automated + listening; P1.

### MW-A03 — Master tone, width, saturation and monitor mute have correct scope
- Contract/source: Mastering.*, Engine deliverMonitor and docs/MASTERING.md.
- Cases/procedure: width0/100/>100%, antiphase/mono, tone/drive extrema, saturation off/on/reset, master automation; compare arrangement/live/take/rehearsal/export with monitor mute off/on.
- Pass/oracle: independent mid/side formula holds, neutral exact within2e-6, saturation finite/bounded by intended transfer, reset retains gain/automation. Monitor mute zeros audience device output only; metering, transport, recorded/exported programme stay unchanged; private routing follows its separate gain.
- Evidence/mode/severity: analytic PCM and actual output-channel observations, manual control/Undo/reopen; automated + hardware/listening; P1.

### MW-A04 — Seeks, loops and boundaries have defined processor history
- Contract/source: Engine reset/render, ElasticAudio, EffectTail and docs/EXPORTING.md; normal seek resets, take seek reconstructs.
- Cases/procedure: tagged impulse near start/end/loop, trim/reverse/overlap, seek into finite delay/reverb, pause/resume and stop, bypass/mute changes, both tail options, split pieces with gaps/deleted spans; 1/63/127/511/512/1024 frame blocks where supported.
- Pass/oracle: no off-by-one attack/cut, nonfinite values or unintended old tails after reset; continuous tails follow documented history, half-open clip boundaries and sample-clock action edges are block-size invariant. Known finite-delay taps and source-index formula are independent oracle; ordinary seeks need not null history-reconstructed playback.
- Evidence/mode/severity: PCM at every boundary/block partition, state and listening click notes; automated + listening; P1.

### MW-A05 — Resampling and stretching preserve signal without unsafe state
- Contract/source: Model rate conversion, SoundTouch/ElasticAudio/PcmView; intended tempo-only/key-preserving playback.
- Cases/procedure: known pitch/sweep, rate44.1↔48↔96k, tempo ratios at accepted limits, source trim/reverse/loop, stream/cache window edges, cancellation/reuse and cache pressure.
- Pass/oracle: expected pitch/rate/duration and sample bounds hold; spectral alias/pitch/THD and audible artifacts measured against independent reference and declared algorithm limits. Do not demand byte equality across algorithms. Missing fidelity thresholds are REQ-03 BLOCKED, not chosen from observed output. Exact frame-clock/routing and T03 attack limits still mandatory.
- Evidence/mode/severity: independent frequency/duration/attack report, configuration and listening reference A/B; automated + listening; P1.

### MW-A06 — Real-time callback avoids unsafe work and exposes missed budgets
- Contract/source: Engine callback try-lock diagnostics, render/elastic/plugin/preview/take call graph and queues.
- Cases/procedure: static trace all callback descendants for allocation, map/vector growth, disk/mapping faults, locks/waits, logging, plugin state/instantiation; instrument owned representative playback with prewarmed/cold cases and simultaneous allowed GUI/MIDI/preparation. Deliberate pressure/stress only at explicitly authorized checkpoint.
- Pass/oracle: no unbounded blocking/file access/allocation in host-owned steady callback; bounded external plugin work measured separately. Every callback duration must be below frames/rate deadline on the declared qualification workload, no lock-miss/oversized/unexpected-silence increments or driver xruns attributed to app. Report p50/p95/p99/max plus deadline misses; try-lock silence is an observable failure, not a proof of safety. REQ-05 fixes target hardware/workload before performance PASS.
- Evidence/mode/severity: line/call ownership review, allocation/lock/I/O trace and diagnostic deltas/time series with measurement overhead; static + runtime/hardware; P1. Existing reported breakup remains unconfirmed pending reproduction.

### MW-A07 — Device changes and loss leave a usable and truthful transport
- Contract/source: Engine startDevice/device callbacks, MidiConnectionUI and launch/take cancellation reasons.
- Cases/procedure: no device, supported Windows backend changes stopped/playing, sample-rate/block/output-channel changes, device unplug/replug, unavailable ASIO and sleep/resume on disposable test setup; private device loss separately.
- Pass/oracle: no crash/hang/stale queued launch, buffers/clock reset safely, displayed driver/rate/latency facts match actual device; take completeness reflects lost frames. No automatic unsafe route to audience/private fallback or concealed silence. Valid reconnect/restart recovers without editing musical state or installing/changing system drivers.
- Evidence/mode/severity: physical device IDs/config/capture, callback state and cancellation logs; hardware/manual + native simulation supplemental; P1/P0 for routing breach.

### MW-A08 — Metering and spectrum observe actual programme samples
- Contract/source: OutputPeaks/PeakMeter/Spectrum and docs/MASTERING.md: independent L/R peaks,1.2s hold, pre-monitor mute, FFT4096 Hann.
- Cases/procedure: single-frame spikes between UI ticks, distinct L/R sine/noise/antiphase/silence, clipping/reset, hide/show/master panel, offline render/private preview and rate/queue overflow.
- Pass/oracle: sample peak dB matches analytic amplitude within0.1dB display quantization, peak retention/hold matches1.2s within one measured UI timer tick; clip latch/reset correct. Spectrum reports bin power per declared Hann normalization, antiphase not cancelled; stale/discontinuous history cleared. Offline preparation/export cannot drive live programme meter.
- Evidence/mode/severity: analytic input/queue/peak reports and inspected live captures/time series; automated + visual; P2.

### MW-A09 — VST instances, state and automation belong to stable owners
- Contract/source: PluginSlot/ClipEffects, Engine instantiate/addPlugin/capture/restore, WavExport cloned chains.
- Cases/procedure: test gain/delay native VST and optional qualified instrument, two separate placements and split shared owner, add/remove/reorder/bypass/editor/MIDI parameter, save/reopen/Undo and export/private/replay clone; missing/invalid module and failed state restore.
- Pass/oracle: correct chain order and stable parameter/slot identities, one shared DSP history per original owner, no cross-placement contamination; independent clones do not mutate live plugin state. Analytic test gain/taps verify actual processBlock rather than a mock. Failure preserves live project and identifies module.
- Evidence/mode/severity: module/parameter/state hashes and owner graph, native PCM/control/editor results; automated + manual; P1.

### MW-A10 — Reported plugin latency blocks unsafe playback/export
- Contract/source: PluginLatency and docs/PLUGIN-LATENCY.md; only active reported-zero inserts supported.
- Cases/procedure: test latency plugin reports0/nonzero/dynamic change, bypass/remove/restore/clone, active lane/clip paths, playing/rehearsal/take/private/export start and final admission; late message-thread callback with cancellation/close.
- Pass/oracle: nonzero active reported latency explicitly blocks affected operation without silently bypassing/shifting source; bypass/removal permits it. Native latency value rechecked on correct thread for final export; late callback cannot reference freed renderer. Wrong-reporting plugin is disclosed residual risk, no invented compensation promise.
- Evidence/mode/severity: native plugin latency/config and admission/cancel/reopen traces; automated/integration; P1.

### MW-A11 — Processor teardown and extreme valid input remain safe
- Contract/source: Engine destruct/reset/prepareClipProcessing, locks and plugin/audio-worker ownership.
- Cases/procedure: finite extremes, silence/subnormal/NaN-producing intentional test plugin, dense allowed automation, repeated load/bypass/close while job pending, owned shutdown after device callback stopped; fault injection only in fixtures.
- Pass/oracle: nonfinite data never becomes successful export/record result; no use-after-free/double release/deadlock, workers/owned handles finish; engine usable after handled failures. In-process plugin crash isolation is unimplemented: declare trusted-plugin qualification and limitation, do not execute arbitrary crashing vendor binaries on user's desktop.
- Evidence/mode/severity: static ownership graph, diagnostics/error PCM and bounded lifecycle/resource report; static + focused runtime; P0 crash/data damage, otherwise P1.

# Track import and analysis timing — 19 October 2026

Measured Mixworks Alloy 0.47.0 and processing components shipped with DJ.Studio 4.2.15 on this Windows PC. Fusion 7.7.0.1 remains unmeasured: no reliable automated import/completion path is available; earlier startup-notice and Windows automation failures are recorded in HANDOVER.md. This is a partial three-app comparison.

Four previously used MP3 recordings, two trials each, sequential execution. Alloy used fresh isolated analysis/PCM caches for every pair; its immediate cached reload was measured separately. DJ.Studio used its installed decoder and unchanged frontend fixed-grid/key/waveform worker in the current Node V8 context with browser message shims (90–180 BPM, stereo averaged to mono), plus separately timed native AI (folds=1). No original music was edited; all source hashes were reverified. No detector update, cache reset, app restart or reserved-recording use.

| Recording | Length | Alloy first load | Alloy cached load | DJ fixed processing |
| --- | ---: | ---: | ---: | ---: |
| 04. Arc Mode (Mark Broom Remix) | 4:58 | 28.73 s | 0.79 s | 3.39 s |
| 11. Chilli (Original Mix) | 6:45 | 33.51 s | 1.10 s | 5.24 s |
| 03. Ain't Nobody - Sean Finn (Funkera Extended Remix) 125 | 5:01 | 30.48 s | 0.78 s | 3.61 s |
| 01. Airsand - Better Off Alone (Radio Edit) | 4:43 | 28.37 s | 0.87 s | 3.41 s |
| **Mean per track** | | **30.27 s** | **0.88 s** | **3.91 s** |

Alloy's measured first-load path took 7.7 times as long as the measured DJ fixed-processing path. This is not a full-app import speed ratio: DJ library/cache/IPC/UI work and Mixed in Key are excluded. Alloy's cache writing is included. The measurements do show that Alloy's main wait is analysis/finalisation, rather than MP3 decoding.

| Stage (mean per track) | Alloy | DJ.Studio measured components |
| --- | ---: | ---: |
| Decode | 0.66 s | 0.87 s |
| Analysis | 27.64 s including finalisation | 2.23 s fixed-grid/key |
| PCM cache writing | 1.78 s | Not measured |
| Waveform | Spans waveform and finalisation stages | 0.78 s |

The separately invoked raw DJ AI detector averaged 3.16 s; decode, mono conversion and raw AI together averaged 4.06 s. This excludes fixed-grid analysis, renderer refinement and key/MIK work. Do not add or substitute these figures as a measured app import time.

## Conditions and verification

AMD Ryzen 7 5700X (8 cores/16 threads), approximately 16 GiB RAM, Windows 11. The user’s Alloy process remained running and consumed CPU during testing. These are observed desktop-load measurements, not an idle-machine laboratory benchmark; OS file cache was not flushed. Two repetitions and four MP3s do not establish universal speed, scaling or musical accuracy.

All four Alloy probes exited successfully: 30 checks; 0 failures; 30 checks; 0 failures; 30 checks; 0 failures; 30 checks; 0 failures. Every first load reported an analysis-cache miss, and cached reload checks passed. DJ probes returned valid grids and waveform data; all four fixed BPM values match their historical saved DJ.Studio BPMs. That is execution agreement, not independently annotated accuracy. One initial DJ harness attempt failed because its isolated JavaScript VM lacked the browser performance global; it produced no timing rows and is retained/excluded as dj-1. Corrected current-context runs are dj-1-current and dj-2-current.

Evidence: [.local/import-timing-39/summary.json](../.local/import-timing-39/summary.json), [manifest](../.local/import-timing-39/manifest.json), per-run measurements/logs and hardware/process snapshots in the same directory. Inputs, binaries, native addons, model, installed worker and final harnesses are digest-bound. Production source and retained user data were not cleared.

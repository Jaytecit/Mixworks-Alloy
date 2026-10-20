# Alloy 0.48 loading and analysis performance

Two trials on each of four previously used MP3 development recordings. Source-loading measurements exclude subsequent project tempo preparation. Existing user Alloy remained running; OS file cache was not flushed. Original audio hashes were reverified. These are desktop-load observations rather than universal speed guarantees.

| Recording | 0.47 fresh | 0.48 fresh | 0.47 cached | 0.48 cached |
| --- | ---: | ---: | ---: | ---: |
| 04. Arc Mode (Mark Broom Remix) | 28.73 s | 20.21 s | 0.794 s | 0.195 s |
| 11. Chilli (Original Mix) | 33.51 s | 22.80 s | 1.097 s | 0.239 s |
| 03. Ain't Nobody - Sean Finn (Funkera Extended Remix) 125 | 30.48 s | 19.69 s | 0.779 s | 0.196 s |
| 01. Airsand - Better Off Alone (Radio Edit) | 28.37 s | 19.35 s | 0.865 s | 0.204 s |
| **Mean** | **30.27 s** | **20.51 s** | **0.884 s** | **0.208 s** |

Fresh source-load time fell 32.2%; cached-load time fell 76.4%. Final mean decoding: 0.708 s; analysis/finalisation: 18.354 s; PCM cache writing: 1.268 s.

The accepted paired-track runs reached 1.56 GiB peak working set and 2.45 GiB peak observed private committed usage. Workspace is reused during a source and unused arena regions are returned after its final model window; model/initial-region memory remains owned. An earlier no-shrink candidate reached 2.41 GiB working set and is retained as provisional evidence, not deployed.

The exact accepted binary passed 1868 checks across 22 suites. All recorded automatic facts and decoded-head hashes match the previous accepted detector on seven development recordings. 70438 raw beat/downbeat activation values on two full recordings and the derived research beat/meter outputs match 0.47 exactly. Four frozen dense-producer spectrogram goldens match bit-for-bit. This establishes tested result equivalence, not new independent musical accuracy or commercial parity.

No source resolution, context coverage, numeric precision, model, thresholds, analysis revision (9) or session format (22) was reduced or changed. Cached loads bypass detection; a new tempo geometry can still require time stretching, and an evicted decoded entry can require decoding without repeating valid analysis. Existing analyses, corrections, library snapshots, prepared caches and recovered arrangements were preserved. The user app was not restarted.

Raw evidence and the accepted receipt are under `.local/loading-performance-40/`; regression reports are `.local/p40-g2/`. Bundle-01 and gate-01 are superseded by the workspace-release change. Interrupted provisional timing/focused probes remain partial evidence and are not release gates. See [loading and analysis](LOADING-AND-ANALYSIS.md) for cache ownership and the reference-app comparison.

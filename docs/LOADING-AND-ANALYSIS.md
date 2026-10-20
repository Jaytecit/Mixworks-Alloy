# Loading and analysis

Alloy keeps analysis, display data and playback preparation separately. Adding a previously analysed, unchanged track should reuse its analysis. It does not need to run the neural detector just because it enters a new mix.

## What is retained

- `Analysis-v1` in local Mixworks application data stores content-bound automatic BPM, pulse/downbeat anchors, confidence/uncertainty, key, waveform data and manual corrections. These records have no PCM-cache eviction quota.
- `library.json` stores the music catalogue and compact analysis/artwork/waveform snapshots. This lets the library display saved tracks before playback audio is loaded. A matching snapshot also supplies analysis when its separate record is unavailable.
- `AudioCache-v1` stores validated decoded PCM and tempo-prepared PCM. Its existing 8 GiB disk budget is shared by those disposable audio preparations. Evicting decoded audio does not evict the independent analysis.
- Saved projects retain source clocks, transitions and arrangements. Refreshing automatic analysis does not silently rearrange an existing mix.

Reopening or restarting Alloy preserves these stores. Detection runs again when the source content changes, an automatic-analysis revision changes, a valid record/snapshot is unavailable, or the user explicitly requests re-analysis. Unknown beat-one/key results are retained as unknown; uncertainty alone is not a reason to discard a completed analysis.

## What a timeline drop still does

The timeline displays the new clip immediately and loads needed playback audio in the background. An already hydrated source can be reused directly. Otherwise Alloy verifies the original audio identity and checks its saved analysis and decoded PCM. A missing/evicted decoded entry is decoded again while valid analysis remains reusable. Corrupt PCM is rejected and rebuilt, independently of analysis.

Matching an arbitrary new project tempo can require new time-stretch preparation. A matching prepared cache skips that work. This depends on source, rate, offsets, loop/reverse and tempo geometry; one preparation cannot cover every possible arrangement. During live playback the existing elastic path keeps the running mix going.

Zero repeated **detection** is achievable on a valid analysis hit. Literally zero CPU or disk access is not: validation, file mapping, arrangement geometry and audio playback still have work to do. Releasing integrity checks or assuming that an unchanged filename means unchanged audio would weaken correctness.

## Compatible performance changes in 0.48

The pinned model, three inference threads, numeric precision, graph optimisation, context coverage, detector rules, analysis revision 9 and then-current session format 22 were retained by 0.48 (0.53–0.54.1 saves use format 24; 0.54.2 returns to four lanes in format 25). Existing analyses and arrangements remain compatible; this performance update does not clear them.

ONNX Runtime now uses its CPU allocation-arena and memory-pattern reuse across context windows, and requests arena shrinkage after the final window of a completed source. This returns unused workspace before mix preparation instead of retaining the peak workspace indefinitely. The spectrogram producer stores only nonzero mel-filter weights in their original ascending FFT-bin order, removing zero multiplications without changing their nonzero weights or accumulation order. Frozen outputs from the old dense producer cover reflection boundaries, silence, impulses, deterministic noise and mixed signal levels.

Source/model/PCM SHA-256 validation uses the Windows SHA-256 implementation through one streaming owner, with the existing portable implementation as a fallback. It still hashes every byte and checks PCM dimensions and finite samples. Known SHA vectors, padding/block boundaries, short reads, failed reads and existing corruption/identity workflows verify the boundary. No new dependency or weaker timestamp-only trust shortcut is introduced.

The model and its initial allocator region remain owned for reuse. Analysis still needs transient runtime working memory. A provisional no-shrink build reached about 2.4 GiB peak working set in the paired-track workflow; the completed-source shrinkage test reduced that to about 1.6 GiB. Peak private committed usage was about 2.45 GiB, so this is not a promise of a 1.6 GiB total allocation limit. Performance and memory observations, numerical equivalence, raw timing reports and regression/release evidence are retained under `.local/loading-performance-40/`. Release status and measured outcomes are recorded in HANDOVER.md after verification.

## How the reference apps do it

DJ.Studio stores analysis and cached application data in its database and refers to original audio files in its 4.1-and-later storage model. Its installed 4.2.15 records examined in this workspace retain source hashes, analysis versions, beat grids and key results; those stored facts support reuse without re-detecting the track on every arrangement operation. Its [database documentation](https://help.dj.studio/en/articles/12315405-file-storage-and-database-management-in-dj-studio) describes this retained-data architecture. Newer documentation about version-5 import behaviour should not be treated as a measurement of the installed 4.2.15 build.

Fusion retains library and `.mxm` data. The installed engine/profile investigation already retained in this workspace recovered source beat/profile records; [inMusic's support documentation](https://support.inmusicstore.com/en/support/solutions/articles/69000848901-mixmeister-troubleshooting-mixmeister-crashes-at-launch-or-when-scanning-your-content-library) also identifies Fusion's retained library files and explains that removing them triggers rescanning. This establishes persistence, not a fresh Fusion speed measurement or complete knowledge of its invalidation policy.

The common design is to separate expensive analysis from arranging and playback, retain the result, and validate it before reuse. Alloy already follows that design; 0.48 accelerates its computation and validation without replacing its detector.

# Loading and analysis

Alloy keeps analysis, display data and playback preparation separately. Adding a previously analysed, unchanged track should reuse its analysis. It does not need to run the neural detector just because it enters a new mix.

Library and Playlist support Ctrl/Shift multi-selection and a right-click batch
menu. See [batch and focused analysis](FOCUSED-ANALYSIS.md) for the secondary
BPM/key retry, completed-attempt reuse, and protected manual corrections.

The default Music folder scan includes all ordinary subfolders. **Library (855)** shows the total number of library entries, including retained unavailable sources. Search changes it to **Library (12 / 855)** for matching / total entries; it does not discard the rest. The notice below the library controls shows scanning/waiting, audio files checked, additions, refreshes and warnings. Hover for the current file/stage and actual folder path. Checked files include unchanged tracks whose analysis is reused; additions count only new entries in that pass. There is no guessed completion percentage while recursive discovery is still in progress.

Use **Tools → Scan Music folder now**, **Automatically scan Music folder** or **Stop current Music scan**. Scanning waits during playback and exclusive preparation. Completion, stopped and unavailable-folder notices remain visible in the Library; no modal popup interrupts editing. Automatic passes run when idle. Scanning does not move files, clear analysis or restart playback.

## What is retained

- `Analysis-v1` in local Mixworks application data stores content-bound automatic BPM, pulse/downbeat anchors, confidence/uncertainty, key, waveform data and manual corrections. These records have no PCM-cache eviction quota.
- `library.json` stores the music catalogue and compact analysis/artwork/waveform snapshots. This lets the library display saved tracks before playback audio is loaded. A matching snapshot also supplies analysis when its separate record is unavailable.
- `AudioCache-v1` stores validated decoded PCM and tempo-prepared PCM with a shared **128 GiB** disk budget. Evicting decoded audio does not evict the independent analysis. The local `audio-cache-location.json` profile can select another drive and budget; this workstation uses `E:\Mixworks\AudioCache-v1`. Existing cache files are copied and verified, with the original C: cache retained as a read-only fallback for entries completed by an already-running scan. New writes and quota eviction use E:. Analysis, library and workspace settings stay in local application data. The profile takes effect on the next launch; it does not restart the current scan or preallocate 128 GiB. BPM/Key details show the active cache path and limit.
- Saved projects retain source clocks, transitions and arrangements. Refreshing automatic analysis does not silently rearrange an existing mix.

Reopening or restarting Alloy preserves these stores. Detection runs again when the source content changes, an automatic-analysis revision changes, a valid record/snapshot is unavailable, or the user explicitly requests re-analysis. Unknown beat-one/key results are retained as unknown; uncertainty alone is not a reason to discard a completed analysis.

Automatic grid timing is checked against clear, repeated audio-attack beginnings instead of a later rise inside a kick or bass hit. New analysis uses the corrected onset search. When an older automatic grid is loaded for playback, a bounded pass examines at most 24 seconds spread through the source, reusing its existing BPM/key analysis and audio cache. Where a bar anchor is known, it compares the same position in repeated bars so anticipated notes on other beats do not pull the whole grid early. It refines timing only when multiple passages agree, saves the completed attempt, and keeps uncertain grids unchanged. Manual BPM or downbeat overrides bypass this pass. Auto Arrange rebuilds its selected shortlist after loading refined timing, without decoding the rest of the library. This update requires no analysis/cache reset and retains the existing choice of beat one within the bar.

## Opening a saved mix

Alloy checks the songs used by the arrangement before opening it. Other library entries retain their saved order, names and source corrections, and are checked in the background while the mix is ready to use. Missing or corrupt unused songs stay in the library with an unavailable label and a detailed path/error on hover. New background checks wait during playback; they do not make the mix dirty. Already available library artwork and analysis stay visible during checks.

If a required song is missing, unreadable/corrupt or unsupported, the current mix stays in place. **Locate source...** lets you choose its replacement and retry opening; **Cancel**, including cancelling the file picker, returns to the current mix. Alloy validates the chosen audio and saved clip geometry before admitting it. Use the same recording: locating a different edit does not automatically realign the saved mix. A successful located-source open needs **Save** to keep the new path. The original session file is never rewritten just by locating a source.

Sessions continue to carry the complete library reference list. This repair retains analysis revision 11 and session format 30 and does not clear analysis, corrections or playback caches.

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

## Key evidence display (0.55.25)

The library header is **CONF** and cells contain only a number. **0** means unrated: manual override, no key, or unavailable/invalid evidence; the tooltip distinguishes them. Valid automatic scores use **1 Limited** below0.05, **2 Fair** from0.05 to below0.15, **3 Good** from0.15 to below0.30 and **4 Strong** from0.30 to1.00. These are presentation bands, not calibrated accuracy or probabilities. A score of zero can accompany an existing detected key and receives1; a missing key receives0 regardless of its stored score. Sorting uses the ordinal and then the retained score, keeping unrated entries last in either direction.

The original score multiplies leading-key margin, fraction of passages won and agreement of two profile models. The chosen key, raw score, detector compatibility and harmonic-admission policy are unchanged. A read-only local observation found359 retained scores: median0.127765, only6 at or above the former0.55 Good threshold and none reaching the former0.80 Excellent threshold. This explains the compressed old display, not detection correctness. Independent key-reference acceptance remains open.

Key labels in note, Camelot and combined modes no longer carry a question-mark suffix. Unknown remains explicit; low-confidence transition movement still says it is unknown and asks for source-key review. Numbered section labels such as S1 also omit the suffix; they remain optional energy suggestions, without proven verse/chorus meaning. None of these display changes rerun detection or alter saved analysis, manual corrections, audio caches or arrangements.


## File genre tags (0.55.15)

The Library has a sortable **Genre** column. It reads the file's genre metadata through the existing Windows property handler; it does not infer genre from the sound. Missing or unsupported tags remain blank, with an explanatory tooltip. Sorting ignores case and keeps blanks last in either direction. Genre also participates in library search.

Genre is retained as an optional catalogue snapshot field. Older snapshots stay valid and receive a metadata-only background backfill when their sources are available; this does not decode audio or reset analysis, corrections, arrangements or prepared caches. The normal background source-check policy waits while playback is running. Offline sources retain any previously saved tag, or stay blank until available. Tag support depends on the installed format handler.

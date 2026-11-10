> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Stem separation direction — 23 October 2026

## Implementation and qualification update —0.55.47

The following historical research section describes0.55.46. The new local
[separation workflow](../STEM-SEPARATION.md) implements explicit background
four-stem jobs, progress/Cancel, independent content-bound storage, validated
reuse, CPU fallback and atomic publication. It does not implement EP08 stem
mixing, sublanes, transitions or extraction. Analysis11/session34 and existing
reviewed downbeats remain unchanged.

Maintainer Demucs4.1.0 at commit2883f3d was compared with4.0.1 using the identical
HTDemucs955717e8 weights, Torch2.6.0+cu124, deterministic shift and12 seconds of
the same music. The arrays were exactly equal (maximum and RMS difference0).
Inference measured4.95s/2.60s respectively in separate runs; this single pair,
including device warmup differences, is not a general speedup claim. The
maintainer version was selected for its local safetensors loader and callback
support. Model SHA-256:
`d9fa14133cfcc034a6758923bb3a8ca9f8dfd0b582134643bbf83f72c17576dd`.

Final worker2 measurements on this RTX4060Ti8GB/16GB-RAM PC:

| Audio | Duration | Whole worker wall time | Inference/output validation | Sum residual RMS/source | Sum lag at start/middle/end |
| --- | ---: | ---: | ---: | ---: | --- |
| Crazibiza, Dance & Shout (JackinGod Remix), full source |249.59s|21.66s|12.50s|1.94%|0/0/0 samples|
| Airsand, Better Off Alone, excerpt |45s|11.38s|4.47s|2.80%|0/0/0 samples|
| Anyma, Pictures Of You, excerpt |45s|14.33s|7.23s|2.50%|0/0/0 samples|
| Anyma excerpt attenuated60dB |45s|12.08s|5.83s|2.50%|0/0/0 samples|
| Airsand excerpt, explicit CPU |12s|24.97s|18.45s|3.85%|0/0/0 samples|

The full track used2.47GB observed peak process RSS,0.576GB peak allocated CUDA
memory and352.2MB for four float WAVs. CUDA allocated memory excludes driver and
other process use. The earlier worker1 full-track run took32.39s; system load and
cold caches matter. These runs overlapped native regression work, so the range
is more useful than a universal throughput promise. All accepted outputs have
exact original frame count/rate/channels and finite PCM. Silence at48kHz mono
remained exactly silent. The first three source inputs were read-only; the
benchmark creates float-WAV fixtures with SoundFile. Native integration separately
uses Alloy's decoder, storage and real command path.

Bounds were recorded before the first measurement: exact clocks, at most one
sample summed delay and15% relative RMS reconstruction error for music. Initial
synthetic impulse and antiphase-tone results failed (72.68× and21.24% residual).
This is retained evidence of out-of-domain weakness, not a passed alignment or
separation-quality result. Worker2 enforces the error bound before publication
and rejects both without a Ready manifest. A forced CUDA1%-memory limit also
exercised a real allocation failure: the explicit fallback status appeared,
CPU completed successfully, and the final manifest recorded CPU.

Additional music checks accepted48kHz stereo and44.1kHz mono with exact lengths,
zero measured sum delay and2.50%/1.85% residual respectively. The resampler alone
round-tripped five known impulses at48kHz, including chunk-sized landmarks and
an odd624017-frame length: all peak offsets were zero and duration was exact.
This isolates the sample-clock mapping from the neural model's documented
failure on sparse impulses. Sliding1s windows at250ms steps had95th-percentile
residual ratios3.42%,5.65%,3.77% on the three music cases. The full song's very
quiet tail reached86.7% relative residual in one window; the global bound does
not promise uniform local reconstruction quality. Retain that tail for listening
alongside vocals, bass/kicks and chunk joins.

No ground-truth isolated recordings were available for reference SDR; numerical
recombination does not establish lack of vocal bleed, kick/bass damage or cymbal
smearing. Musical listening acceptance remains open. Named real-music research
stems are retained for that next listening review; synthetic and duplicate
measurement audio is removed after measurements with hashes/journals retained.

Evidence: `.local/stems-20261023/`, `.local/stem-bench1/` (initial failures),
`.local/stem-bench2/` (final worker), and the frozen native candidate/full gate
identified in HANDOVER. Independent source and user-data inventories accompany
the task. The skills.sh search found hosted-service guidance rather than a
suitable local integration recipe; implementation follows primary Demucs code.
No hosted separation service or copied competitor implementation is used.

## Current Alloy status

Stem separation is **not implemented or finalised** in 0.55.46. The source list contains no stem inference backend, asset/job owner, separation command or independent stem routing. The `stems` variable in `LibraryAssets.cpp` is a list of cover-art filenames. [EP07 and EP08](../DJSTUDIO-EDITING-PARITY-PLAN.md) remain open; existing bass/filter transitions process the complete track.

## Resources reviewed

- [StemDeck repository](https://github.com/stemdeckapp/stemdeck): a local application using Demucs, with CPU and NVIDIA Windows packages. Useful reference for background separation, progress, library reuse and deployment. Its complete practice studio, download services and analysis stack are unnecessary for Alloy's first separation increment.
- [StemDeck dependencies](https://github.com/stemdeckapp/stemdeck/blob/main/pyproject.toml): pins Demucs below 4.1 and Torch/torchaudio below 2.7 for compatibility. Its dependency comments document audio-writing and platform problems. These are reasons to qualify a complete runtime bundle, not blindly copy the pins. [Model notes](https://github.com/stemdeckapp/stemdeck/blob/main/docs/models.md) describe its six-stem choice and distinguish checkpoint rights from a wrapper's licence.
- [Stem Splitter tools directory](https://stemsplitter.github.io/tools/): useful discovery list, not benchmark evidence. In particular, its GPU requirement is too strong: Demucs supports CPU processing. Its quality rankings and commercial comparisons have not been independently validated here.
- [Maintainer's Demucs repository](https://github.com/adefossez/demucs): current upstream documents 4.1.0 and CPU/GPU operation. The old Meta repository is unmaintained; the maintainer also warns that development is slow. Four-stem `htdemucs` produces vocals, drums, bass and other. Six-stem guitar/piano is experimental; upstream reports piano bleed and artifacts.

## Proposed direction

Qualify local four-stem Demucs first, in an isolated background process with a pinned runtime and exact model hash. Benchmark the current upstream bundle against a known compatible bundle before choosing a version. Keep the JUCE audio callback free of inference. CPU is the fallback; GPU acceleration is optional and needs explicit capability and failure handling.

Start with explicit **Split track** and progress/Cancel. Use original source PCM once; return aligned float WAV assets plus a manifest tied to the source hash, model/runtime version and settings. Validate sample rate, channels, sample count, finite samples and source-time alignment before atomically publishing the whole result. Cancellation, a model download failure, GPU memory exhaustion or a crashed worker must leave the original and the current mix usable. Never adopt partial outputs as a successful split.

Cache by source identity and separation settings independently of grid analysis. Preserve reviewed downbeats and manual corrections. Each stem inherits the original source clock and follows the same trim, speed, slip and transition timing. Independent gain/mute/solo and stem transitions belong to EP08 after EP07 qualification, rather than treating four imported songs as equivalent.

Before acceptance, measure wall time, peak RAM/VRAM, disk usage, cancellation/recovery, impulse and long-file alignment, chunk boundaries and recombination residual. Listen to preselected dance tracks for vocal leakage, kick/bass damage, cymbal smearing and transition artifacts. Declare numerical bounds before measuring; no bit-perfect reconstruction or listening-quality claim follows from creating files. This task researched direction only: no model download, separation execution, performance measurement or stem implementation.

StemDeck's Apache-2.0 application licence does not determine the licences of models, runtimes or copied dependencies. Record exact artefacts and notices under [licensing](../LICENSING.md) before distribution. No public package or agreement acceptance is part of this research.

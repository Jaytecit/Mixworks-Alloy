# Targeted BPM and key re-analysis options

Implemented in 0.55.64, 24 October 2026. Normal analysis remains revision 11; explicit focused retries use method revision 3. Existing analysis, corrections and PCM remain valid. Import decoding repair is separate; unread audio cannot produce a meaningful BPM/key estimate.

## Delivered retry path

- Key eligibility now shares the library's **0.55** review threshold. Manual corrections and accepted fields remain protected. Missing bar anchors are eligible without replacing an already accepted stable pulse.
- BPM and key merge independently: a failed/unavailable rhythm result cannot discard useful tonal evidence. A replacement still needs stronger field-specific evidence; uncertainty remains visible rather than forcing a result.
- Retries shift the bounded passage schedule, use the existing physical low/mid-band and harmonic/profile/tuning paths, and avoid asking an unavailable rhythm model again. Existing SoundTouch BPMDetect compares three separated passages of up to 20 seconds for tracks over 30 seconds, retaining raw BPM and agreement/half-double/disagreement reasons. It does not establish beat one or override pulse validation by itself.
- Source-bound analysis JSON and human-readable `AnalysisRetries` reports retain before/candidate/result values, passages, method, previous failure and separate decisions. Completed unchanged attempts are remembered; cancellation does not publish an attempted-method marker.
- No Essentia/libkeyfinder dependency was added. Their external comparison remains an option for specific unresolved cases, not a prerequisite or automatic library run.

The final candidate passed 12 focused retry checks, including the .40 eligibility case, independent key success during tempo failure, cache persistence, manual preservation, cancellation, silence abstention and a bounded SoundTouch comparison. No full-library retry, real-music accuracy benchmark or listening claim. See [focused analysis](FOCUSED-ANALYSIS.md) and HANDOVER for evidence.

## Original findings

Mixworks already has `AnalysisCacheMode::focused`, `needsFocusedTempo`, `needsFocusedKey` and `retryMusicAnalysis`. `Model.cpp` protects manual BPM, key and reviewed beat origins. Reuse this path and valid cached PCM.

The two original findings, now resolved, were:

- `Media::needsAnalysis` flagged automatic key confidence below **0.55**, but `needsFocusedKey` admitted only unknown keys or confidence below **0.15**. Some tracks shown for review were excluded. These are internal scores, not calibrated probabilities.
- `retryMusicAnalysis` returned the previous result entirely when a tempo candidate had a rhythm issue, even if key was also requested, losing an independently improved key candidate.

Focused-revision flags also prevent endlessly repeating the same attempted method. Keep that protection but record which alternative/sections/settings were actually tried so a newly useful method can run without resetting everything.

## Candidate tools

| Tool | Proposed role | Limits |
| --- | --- | --- |
| SoundTouch BPMDetect | Cheap second BPM estimate; already compiled in the project's SoundTouch dependency | Its repeating low-frequency rhythm approach can report a lower tempo for complex bass patterns. Treat it as a candidate, not a replacement for pulse/downbeat validation. |
| Essentia RhythmExtractor2013, multifeature | Independent BPM/beat-position comparison, with confidence and interval evidence | Requires 44.1 kHz input. Scores are not directly comparable to Mixworks scores; calibrate on checked examples. |
| Essentia KeyExtractor | Compare tonal profiles and tuning correction on selected tonal passages | A strong profile score does not prove a unique whole-track key; preserve plausible alternatives/modulations. |
| Mixxx / libkeyfinder | Free desktop comparison workflow; libkeyfinder is also a C++ key-estimation component | Use another analysis as evidence rather than ground truth. Mixxx permits full-track analysis and changing key detectors; availability depends on its build. |

Primary references: [SoundTouch BPM detection](https://www.surina.net/soundtouch/README.html), [Essentia rhythm extraction](https://essentia.upf.edu/reference/streaming_RhythmExtractor2013.html), [Essentia key extraction](https://essentia.upf.edu/reference/std_KeyExtractor.html), [Mixxx beat settings](https://manual.mixxx.org/2.5/en/chapters/preferences/beat_detection), [Mixxx key settings](https://manual.mixxx.org/2.5/en/chapters/preferences/key_detection), [libkeyfinder](https://github.com/mixxxdj/libkeyfinder).

Essentia documents AGPL/commercial licensing and libkeyfinder is GPLv3-or-later. Evaluate externally before choosing a bundled dependency; any redistribution decision must follow `docs/LICENSING.md` and the [upstream Essentia terms](https://essentia.upf.edu/licensing_information.html). No dependency has been installed or licensed here.

## Recommended method

1. Categorise the miss: unreadable audio, missing BPM, half/double-tempo ambiguity, weak/unstable pulse, missing downbeat, weak tonal evidence, detuning, or disagreement between sections. Retry only the affected field; preserve accepted data and manual corrections.
2. Select a few informative passages from existing section/energy evidence. Use clear rhythmic passages for BPM and sustained tonal passages for key. Expand coverage only when the first retry remains ambiguous. Avoid treating repeated runs on identical audio/settings as new evidence.
3. For constant-tempo 4/4 tracks, compare the old BPM, half/double candidates and the independent estimate. Validate pulse phase and drift across separated passages. BPM agreement alone does not establish beat one; preserve reviewed downbeats.
4. For key, estimate tuning, compare alternate profiles and tonal passages, and retain the best competing keys. Harmonic/percussive separation is a possible later experiment for percussion-heavy passages; do not begin with full-library neural stem separation.
5. Accept a replacement only with stronger, relevant evidence. Detector scores use different scales, so do not select whichever reports the largest number. Record previous/candidate values, method/version, segments, decision and reason; leave genuinely ambiguous results for review.
6. Evaluate a small manually checked sample of actual misses, with one low-priority worker, cached PCM, cancellation and bounded windows. Show before/after evidence and improvement count. No full-library rescan, cache reset or heavy benchmark is required to choose the first useful alternative.

The eligibility/independent-merge fixes and bounded SoundTouch comparison are delivered. Evaluate further detectors only against specific unresolved cases with reviewed evidence; do not invalidate accepted analysis or reset the library to do so.

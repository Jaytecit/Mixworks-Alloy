# DJ arrangement foundation implementation plan

**Goal:** replace recording-level intensity ranking with explainable musical-activity features, add a nonblocking genre preference and conservative technique variety. Follow the [research targets](research/DJ-ARRANGEMENT-TARGETS-20261023.md). Execute inline; the user requested no subagents or separate repetitive review cycles. Preserve independent source/evidence checkpoints; no commit/public release is requested.

## 1. Energy module and verification

- [x] Add Source/MusicalEnergy.h/.cpp with independently versioned source-time windows, strict bounded validation, complete/unknown handling and local feature extraction from read-only PCM. Keep signal gain out of the score; average channel powers rather than stereo samples. Check cancellation throughout.
- [x] Add Source/MusicalEnergyTests.cpp and a focused self-test selector, also exercised in the complete native gate. Use authored sparse/busy percussion, bass/drone, dynamics, practical gain changes, local structural changes, silence/DC/nonfinite/stereo/rate and cancellation fixtures. Retain measured descriptors and assertions through verification.run_checks.

## 2. Cache and media integration

- [x] Append a supplementary profile to Media/TrackAnalysisRecord; serialize it independently of rhythm analysis. Missing/malformed/future data remains unknown without discarding valid beat facts or manual corrections.
- [x] Integrate extraction while new/playback PCM already exists and via a dedicated energy-upgrade read purpose for existing sources. Ordinary warm catalogue reads must stay decode-free. Publish only complete profiles for the matching source identity. No change to detector revision11/session34 and no data reset.
- [x] Extend cache and refresh coverage for exact source binding, malformed fields, versioning, round-trip, warm reads, manual corrections and cancellation. Do not write test data into production AppData.

## 3. Arranger and genre

- [x] Replace body-RMS ranking with supplementary musical score and mapped local handover evidence. Unknown scores remain neutral/BPM-guided and explicitly reported; RMS remains only a recording-level treatment signal.
- [x] Add genre preference to options/native dialog, draw choices from catalogue tags, normalize a small alias set and apply a bounded preference cost after hard feasibility checks. Count unknown/other-tag fallback use. Preserve explicit starts.
- [x] Test equal-RMS different rhythms, quieter driving material, gain-invariant choices, chosen starts, sparse metadata, all existing styles/directions, actual cache-backed background preparation and Undo/Redo.

## 4. Supported variety

- [x] Add DJ set (varied) separately from the six existing choices. Prefer a plain phrase blend or bass handover unless evidence justifies a restrained filter-assisted join; allow a short feasible phrase preference. Track prior techniques and never force unsafe variety. Leave echo/loop/stem/drop-semantic techniques as explicit researched targets.
- [x] Retain decision reasons/technique counts and verify real generated nodes, feasible ranges, absence of inherited unwanted fades, source clocks, native selection, rendering and session round-trip.

## 5. Acceptance and local stage

- [x] Build once features are integrated; retain original red/failure evidence and final source/bundle identities. Run focused reproduction, actual native UI captures and inspect pixels.
- [x] Exercise representative real sources read-only with isolated caches and retain measurements rather than repeated audio. Full-catalogue decode is not a hidden startup cost.
- [x] Run the complete final-candidate gate once. Preserve any failure and provenance; retry only a failed unchanged-candidate suite after evidence justifies it. No assertion weakening.
- [x] Validate and stage locally for the next normal launch only. Update HANDOVER and the arranger guide with actual coverage, listening limits, profile upgrade behavior and exact preservation observations. Hard-link identical model/runtime assets and preserve independent recovery evidence.

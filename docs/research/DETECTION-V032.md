> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# v0.32.0 detection and alignment — 18 October 2026

Status: verified and deployed as **0.32.0-b55c54b8c78c**. Final executable SHA-256 `b55c54b8c78c6f9f7dd679e9d30685c553900a7d585e4205a84568b6e121e5a5`. Previous releases remain available; save and close an older running instance before reopening Mixworks.cmd.

## Real-music comparison

The complete fresh comparison is `.local/detection-v032/comparison-final-03.json`, produced with `scripts/compare-djstudio.py`, the authenticated 77-track benchmark and `tests/real-music-comparison.json`. Its executable was immutable `bundle-03/Mixworks.exe`, SHA-256 `6245f51715715a1dc08428255d3e18a00123f91c997cac8a32400d4370426bd1`. Subsequent changes concern arranging, source-clock ownership, analysis refresh and verification; the numerical detector implementation and pinned model are unchanged.

| Comparison, 77 verified recordings | v0.31 | v0.32 detector |
|---|---:|---:|
| BPM within 0.1 of DJ.Studio | 73 | 77 |
| Key agrees with DJ.Studio | 34 | 43 |
| Key agrees with stored Mixed in Key value | 35 | 48 |
| Downbeat unavailable | 30 | 11 |
| Bar phase agrees within 50 ms at p95 | 25 | 44 |
| Available bar phase disagrees | 22 | 22 |
| Large phase disagreements | 10 | 9 |

All 77 source identities and production decoders were verified; all analyses were fresh. Fresh readiness checks total 456 pass / 12 fail, including the two-input harness's duplicated last track. Those 12 failures remain visible: 11 downbeats unavailable and one grid requiring review. Key remains unknown on four tracks. Reference sources agree with each other on only 46/77 keys. These are comparison results, not independently established musical accuracy.

Arc Mode now measures 129.0027, Chilli 129.9977 and Nude 125.0007 BPM. Nude's competing rhythmic layers still leave musical downbeat unknown. No comparison labels, track identities or restricted DJ tempo range enter detection.

## Implementation and independent checks

Analysis revision 7 adds local Beat This! final0 inference through pinned ONNX Runtime, reconciled with source attack timing, metrical evidence and recurring structure. The previous single-window winner/median is no longer the sole choice between metrical interpretations. Source timing fits remain useful evidence and fallback. Key estimation adds register normalization, stereo harmonic evidence and published profiles; its score is evidence strength, not an accuracy probability. Missing/corrupt neural models are explicit failures and are not cached as successful current analysis.

The model is public MIT material, not extracted vendor detector code. Pins and hashes are in `scripts/analysis-dependencies.json`; attribution and licenses are bundled. This is local CPU analysis, with no cloud audio upload. The model is approximately 83 MB; measured real-workflow peak working set reached 1,038 MiB. The internal 512 MiB feature admission budget is not a total process-memory limit.

Eight additional recordings were reserved before tuning and tested as holdouts. All eight tempos agree within 0.1 with independently run installed DJ.Studio native AI. Decoder-aligned pulse p95 is 17.9–37.1 ms. Five raw native AI bar phases disagree; raw and aligned vendor outputs themselves differ, so none is promoted to truth. No native key field was returned. The other eight reserved tracks remain untouched. See `.local/detection-v032/holdout-comparison-03.json` and its digest-bound inputs.

Independent source harmonic/attack measurements are retained under `independent-evidence/`. They expose modal/major-minor/tonic ambiguity in Bel Canto, Disclose, Emergency Exit and American Boy; they do not establish a global human-adjudicated key. Source bass attacks do not independently settle Nude's bar phase. These limitations remain unresolved.

Bundle 03 passed 612 strict regression checks. Its Arc/Chilli real production workflow passed 76/76; measured automatic-transition source/render displacement p95 was 6/0 ms, maxima 7/6 ms, with an 18 ms maximum across all moves and edits. Arc/Nude retains six expected bar-availability failures because Nude's bar is unknown; all render outputs and failures are preserved. These counts are not general music-accuracy claims.

## Arranging and re-analysis

The user clarified that OUT and the following IN must remain joined when either whole track or either handle moves, with Snap on or off. The current implementation stores transition partner identities, propagates whole-track movement backward and forward through a chain, uses one undo operation and preserves source-owned cues/effects. Legacy sessions adopt only already-aligned relationships without moving their arrangements. Reopening captures/restores source clocks instead of reinterpreting old geometry through newly detected BPM. Ordinary preparation and insertion also retain those captured clocks; explicit Beatmatch can adopt new analysis.

Real-music verification exposed a second source of apparent snapping offsets: the default incoming cue was the file edge, before the first musical beat in recordings with a pickup. Automatic IN now uses the first inferred musical bar (detected pulse when the bar is unknown), and its owned tempo target reaches the source tempo at IN. Pickup audio remains before the marker. Both cues are initialized through one transition owner, with automation in the correct source coordinates. A linked partner's old pulses no longer act as stationary magnetic targets while the pair moves.

A concrete deletion bug was reproduced: collapsing coincident ramp/cut endpoints after deleting their track changed the global clock while established live clips retained the old map. Three regression checks failed before preserving both endpoints. Session 13 can retain their independent timing even after ownership is detached. Bundle 05 passed 621 strict checks and 80 real Arc/Chilli workflow checks, including sample-identical save/reopen. This predates the latest linked-movement and re-analysis UI changes.

Full-height track guides now reuse the timeline's computed beat positions. Separate short ticks retain detected source timing, including real offsets after free edits or uncertain detection. This makes the musical ruler consistent without concealing source-analysis disagreements. Native raster tests explicitly distinguish the common ruler from deliberately offset source ticks.

**Beatmatch → Re-analyse** now offers selected sources, tracks needing analysis, all library sources, and selecting tracks needing analysis. Library and Playlist support multiple selection; repeated playlist clips are deduplicated by source. Missing, outdated or visibly uncertain BPM/key/downbeat results qualify; valid manual corrections take precedence. Refresh bypasses automatic analysis caches, preserves manual corrections and captured arrangements, and offers progress/stop after the current track. It requires playback and pending timeline preparation to be stopped. Production refresh tests also check cached sentinel bypass, unchanged original audio, prepared playback and arrangement JSON, persisted manual corrections, selection identity and cancellation. Final integration verification passed and the exact verified executable is deployed.

## Final release verification

Immutable bundle 11 passed **759 checks with zero failures** across 13 suites, including native re-analysis, linked movements, tempo/browser controls, playback/cache, source ownership, real VST3 loading, malformed sessions and missing/corrupt model handling. Evidence: `.local/detection-v032/final-tests-11/summary.json`.

The exact final executable passed **82/82** real Arc/Chilli workflow checks and **21/21** remove/add checks on the user's Hammer MP3 and Ain't Nobody FLAC. Save/reopen retains geometry, source clocks, links and sampled PCM. Native raster evidence covers common grid pixels, separate source ticks, cues and multi-selection/progress. Source/render envelope transfer p95 for automatic insertion is 6/0 ms, maxima 7/6 ms; the maximum across twelve source/scenario transfers is **18 ms**. Weak/ambiguous envelopes are excluded with counts; this measures playback timing, not musical detection truth. Evidence: `final-real-workflow-11/`, `user-grid-11/`, and `release-verification-11.json` under `.local/detection-v032/`.

The 77-track numerical comparison and holdout results above were measured on frozen bundle 03, not relabeled as a final-executable run. Detection code and model are unchanged; later work concerns arranging, cache/manual ownership, refresh UI and regression checks. The existing unsaved user mix was preserved and not claimed as an exact reproduction.

## Separate assessments and next UI priority

The four zplane demos were examined separately with REA. Their functionality is complementary: AUFTAKT rhythm, TONART key, BARBEATQ structure and ELASTIQUE stretching. Supported SDK integration, demo limitations, native evidence and documentation-version differences are recorded in `.local/auftakt-assessment/ZPLANE-FUNCTIONS.md`. None was integrated, used as detector labels, or used to alter the benchmark. Installed applications and user audio were preserved.

Normalization remains a controlled future experiment. Register/feature normalization is already used where appropriate; whole-file gain cannot by itself resolve tempo aliases or tonic ambiguity. No new normalization experiment changed the frozen comparison.

DJ.Studio's visible transition span with an adjacent lock remains the next UI priority after this foundation. Persistent pairing is groundwork; that transition-span UI is not yet implemented.

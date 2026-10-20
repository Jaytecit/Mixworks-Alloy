# Long-session navigation and live insertion

Version0.39 fixes an alignment cue at the outgoing endpoint and retains the0.38 detector and0.37 capacity improvements. Its exact bundle passed1,519 checks across22 suites and a separate24-check replay against the accepted90-minute reference WAV. Previous releases, receipts and source checkpoints remain available.

Random forward/backward seeks near540s,2700s,5130s and the ending were compared with the accepted full render after two seconds of actual Gain/Delay/EQ history. Every subsequent sample stayed within four PCM24 steps; maximum observed differences were about1.34e-7. Device clocks retained their integer-frame anchors, and the final partial block stopped at the exact audible end. This is numerical callback evidence, not a physical driver or listening test.

The replay explicitly unlocked the owned fixture's first pair before a live append that adds a tempo target. Normal timeline insertion used an already hydrated development recording. Transport and original-region audio continued, existing placements/controls/automation remained intact, and paused Undo/Redo restored the expected models. An equivalent explicit OUT cue may be adopted while retaining its original source-time meaning. The saved reference session, WAV and all source recording hashes were unchanged.

The first replay found an edge-case defect: when the incoming cue equalled the outgoing endpoint, a strict fade-window test replaced that cue with file start. In the90-minute case, the effective OUT moved about162ms earlier. Alignment now admits the endpoint using the existing source-relation tolerance; crossfade/bass fade-window behavior is retained. Focused cases cover exact and adjacent floating representations, with placement, spans and manual fades preserved. Both the original failures and passing replays remain in `.local/long-session-28/`.

This does not establish new material's musical alignment or sound quality, private cue routing, physical underruns or FLkey behavior. Actual OS picker acceptance also remains incomplete because the automation helper could not activate its native modal window. The code-driven controller tests are separate evidence.

Reproduce the replay with a frozen executable using `--self-test --long-navigation-only --minutes 90 --session <absolute owned session> --reference <absolute accepted WAV> --report <new report>`. The source identities and structured results are recorded in `navigation-metrics.json`. Use a fresh case directory; a reused owned PCM/analysis cache is a warm-open test and does not clear the OS cache.

The QA05 continuation passes 27 checks, adding a new owned post-insertion Save/Open, exact model/clock comparison and finite nonzero PCM from the new material after seek and effect-history preroll. This continuation was included in0.40 and its same exact bundle passes all27 checks. It still does not establish musical alignment or listening quality.

# Mixworks request status as of build 0.55.64

As of25 October2026, the latest staged application is **0.55.64-66ebcab993b3**, including the take/private-callback audit repairs with185 focused checks. The launcher is updated for the next launch; the existing r646 session remains running. Arrange and Record/Perform, shared track controls, private preparation, master WAV capture and targeted analysis retries are implemented. The historical572-import recovery checkpoint remains retained; the running app's own scan is preserved. Real-output audio/MIDI reliability, independent musical accuracy and release qualification remain open.

This register consolidates the recorded error reports, requested fixes, workflow changes and all **P01–P56** roadmap items. Repeated requests are merged; later decisions replace earlier ones. Sources are the current and archived handovers, finishing plan, wishlist, product/automation/editing audits, implementation contracts and recent project chats. It is a record-based status report, not a new application test or a claim to have recovered every historical chat message.

## Candidate and evidence

| Item | Status |
| --- | --- |
| Normal launcher | `out/releases/0.55.64-66ebcab993b3/Mixworks.exe` |
| Executable SHA256 | `66ebcab993b3ad79b01faacec4adaf912edaf9932e6b220009cbf304dd129575` |
| Formats | Analysis 11; session 41; focused retry 3 |
| Latest application acceptance | **185 focused checks passed**: private callbacks47, take UI29, performance/replay/export/parser87, preview22; zero failures/infrastructure/retention errors. [Exact report](../.local/ra05/final.json), [scoped local stage](../.local/ra05/staging-receipt.json). No complete regressions claimed. |
| Earlier full regression | 0.55.52 recorded 5,692 checks across 22 suites. This is historical evidence for that candidate, not full coverage of 0.55.64. |
| Later audit repairs | Documentation/build/verification tooling only; application source and staged executable unchanged. **37 tests passed, 1 symlink privilege skip**. [Tooling report](../.local/ra01/focused-tooling.json). |
| 25 October audit continuation | SUS-06 take-validation/capture-limit repair in an **unstaged** candidate; **74 focused checks passed** after retained red and fixture-failure runs. Staged r646 and launcher unchanged. [Current audit status](release-audit/STATUS.md), [focused report](../.local/ra02/final.json). |
| Next 25 October audit pass | SUS-07 unsupported take-event admission repaired in a newer **unstaged** candidate; **34 parser checks passed**, after4 reproduced failures. Static supplemental/conditional coverage and take/private controls reviewed; whole audit remains unpassed. [Parser report](../.local/ra03/final.json). |
| Stale take-popup follow-up | SUS-08 repaired as AUD-D07 in another **unstaged** candidate; **29 callback checks passed**, after8 reproduced failures. Stable IDs, content fingerprints and completion-time admission protect take actions/event drafts. [UI report](../.local/ra04/final.json). Prior candidate checks are historical. |
| Private-popup follow-up and requested stage | SUS-09 repaired as AUD-D08;185 checks on the final candidate cover current private fixes and earlier take repairs. **Staged and launcher updated**, without restarting production. Earlier unstaged statuses above describe their original checkpoints. |
| Conditional coverage follow-up | AUD-D09 repairs the missing split-effects gate branch and reduced-child receipt acceptance.23 suites now required; no revised full gate run.31 harness tests and29 focused split-effects checks pass. Application/launcher unchanged. [Evidence](../.local/ra06/split-effects.json). |
| Release audit | **78 NOT RUN, 16 BLOCKED** across 94 rules; zero rule PASS/FAIL. Four repaired defect cases do not complete their containing rules. [Authoritative status](release-audit/STATUS.md). |
| Current qualification | Focused local beta update. Full current regression, physical hardware/listening, clean-package acceptance and distribution clearance remain open. |

**Implemented** below means delivered with recorded evidence at the named checkpoint and carried into the current source. It does not mean every older workflow was retested on 0.55.64. **Partial** means useful implementation exists with specific work or acceptance remaining. **Open** means the reported outcome is unresolved. **Planned** means no completed implementation is established. **Superseded/stopped** means a later instruction changed or halted the request.

## Reported problems and requested corrections

| Request or issue | Status at latest build | Delivery and remaining work |
| --- | --- | --- |
| Nearly 600 music files missing; scan warnings rise per track | **Reader fix implemented; recovery partial** | 0.55.63 repairs untagged MP3 frame-length estimation. Of 579 missing files, **7 imported, 0 failed, 572 deferred** at the user's explicit checkpoint. No later recovery is established by this report. [Recovery](../.local/arp-20261024/recovery.json), [remaining sources](../.local/arp-20261024/remaining-imports.json). |
| Scan warnings hide the actual cause | **Implemented, 0.55.63** | Import failures and cache warnings are separate; per-file reasons persist under Tools / Music scan reports. Explicit scanning retries failures and skips valid existing results. The MP3 diagnosis does not establish one cause for every historical warning. |
| PCM cache fills despite adequate disk space | **Implemented, 0.55.60** | 128 GiB cache on E:, with 133 prior entries copied and hash-verified; original C: entries retained for reuse. A larger quota does not repair decoder failures. |
| Slow imports/reopening and repeated analysis | **Partial** | Persistent source analysis/catalogue/PCM/prepared caches, immediate pending placements, prioritized required-source loading and faster analysis are delivered. Fresh model analysis still takes time; current broad speed/RAM targets are unresolved. See [loading](LOADING-AND-ANALYSIS.md). |
| “Tempo ramp audio exceeds the 1 GiB preparation budget” | **Implemented, 0.10.1** | Adopt disk mappings and count resident allocation rather than charging mapped logical PCM; check valid cache hits before new allocation. Larger sources remain subject to actual storage/preparation bounds. |
| Unused missing library files prevent a valid mix opening (AUD-003) | **Implemented, 0.55.23** | Only arrangement-required sources gate preparation. Unused unavailable catalogue entries remain visible; required failures have Locate/Cancel and preserve the current document. |
| Occasional crackling, silent skips and UI/audio contention (AUD-004) | **Partial; listening outcome open** | Actual callback lock-skip silence was recorded on 0.55.52. 0.55.53 releases the engine lock before PerformanceDeck UI updates and adds supported buffer/rate choices and diagnostics. Physical crackle-free playback on the repaired candidate is not established; other causes remain possible. |
| Computer freezes during testing | **Mitigation implemented; resolution open** | Essential batched checks, one build worker and `/MP1`, below-normal children, no build/test overlap, stress opt-in. Audit also repairs build-script enforcement. Last audit recorded about 1.3 GiB free RAM and withheld heavy work. No claim that priority fixes system responsiveness. |
| ASIO4ALL / wrong physical output | **Open** | Prior diagnosis found Oculus routing; selecting Realtek still showed Not Connected. Software route tests and ASIO compilation do not resolve the physical driver/device issue. |
| FLkey stops responding; wrong profile returns (AUD-013) | **Partial; original cause open** | Mapping lifecycle, reconnect policy, active names, Learn/Cancel, validation, import/export and reversible deletion delivered by 0.55.8. The USB outage and active-versus-startup mismatch remain unproven; physical reconnect/startup acceptance required. |
| Physical transport and on-screen pad order disagree | **Implemented with historical device evidence** | 0.54.1 desk order and subsequently measured physical assignments documented; actual MIDI port/CC/realtime messages were captured. Full latest-build hardware acceptance remains open. |
| Mapped Snap/mute/solo bypass Undo | **Implemented, 0.54.3** | Canonical switch path captures history before mutation; repeated/held/release and Undo/Redo cases covered. |
| Beatmatch Sections command initiates Quit (AUD-001) | **Implemented, 0.55.13** | Disjoint typed menu identities repair dispatch. Changing new-track length preserves the current mix and transport. |
| BPM/key correction targets hidden old Library selection (AUD-002) | **Implemented, 0.55.13** | Active selection surface and explicit context source resolve the intended song; invalid targets cannot fall back silently. |
| Source correction appears saved after a write failure | **Implemented, 0.55.14** | Durable identity-checked write precedes dismissal/rebuild; failure restores state and leaves the error/draft visible. Save source only and Save + rebuild are distinct. |
| Grids appear late relative to waveform attacks; movements lose manual alignment | **Targeted fixes implemented; general accuracy open** | 0.55.21 corrects attack timestamping and snaps movement displacement. 0.55.61 extends quiet-boundary search to 100 ms and fits repeated existing-bar attacks, preserving manual overrides. One measured source improved to within 1.322 ms of its opening reference. This is not a full-library timing/downbeat guarantee. |
| Accurate constant-tempo 4/4 BPM, pulse and bar alignment | **Partial** | Shared source/project/render clocks, physical attack refinement, explicit ambiguity and correction tools exist. Independent reference corpus, accuracy thresholds and listening acceptance remain open. A kick is not proof of beat one; earliest downbeat is optional. |
| Seven tracks where MixMeister and DJ.Studio agree, with guided comparison | **Planned; investigation paused** | [Consensus plan](research/CONSENSUS-GRID-BASELINE-PLAN.md) exists; no seven-track set is qualified. Reserved recordings and earlier observations remain protected. |
| Low key confidence despite apparently correct results | **Presentation implemented; calibration open** | Compact keys and numeric CONF ordinal bands delivered; raw detector evidence retained. Ordinal values are not accuracy probabilities or a 99% correctness claim. |
| Weak BPM/key retry misses review-flagged keys and loses good key evidence | **Implemented, 0.55.64** | Common .55 threshold, independent BPM/key outcomes, shifted passages, existing tuning/harmonic options and three bounded SoundTouch BPM passages. Accepted/manual fields protected; source-bound before/after/reasons persist. No automatic full-library retry or general accuracy claim. |
| Guesses disappear when analysis abstains | **Implemented, 0.55.52** | Supplementary best key/4/4 phase candidates can be retained and labelled as guesses; silence or lone pitches can still have no useful key. |
| Keep reviewed downbeats; save reusable section boundaries | **Implemented, 0.55.38 / 0.55.50** | Independent immutable review observations, source section defaults, exact positions and labels; S and HC remain independently editable. A reviewed-choice record does not itself prove an active source save succeeded. |
| Nodes hidden by title bars, selecting a curve makes another line, Filter/FX editing inconsistent | **Implemented through 0.55.24–0.55.42; human acceptance open** | Shared curve editing, footer tools, precise editors, explicit owners, view-only selection and ordinary preset nodes address producing paths. Later request supersedes automatic endpoints: empty curves add only the requested node. |
| Excessive automatic Filter/gain nodes | **Implemented, 0.55.51 / 0.55.53 / 0.55.57** | Fit sparse ordinary curves, retain needed boundaries and genuine shape; clamp floating-point edge times and preserve silent endpoint markers. Existing manually authored/dense saved curves are not automatically rewritten. |
| Move swaps in time; change cut depth without moving time | **Implemented** | Paired timing handles, ordinary-node swap detection, vertical amount handles, Pair bass swap and explicit Swaps menu; accessible precise editing and one Undo. Physical feel/listening acceptance remains open. |
| Automation captions should be percentages only | **Implemented, 0.55.53** | Selected node/cut captions use percentages; parameter names and physical units remain in tools/hints where needed. |
| Transition width/header grab jumps playback to frame start | **Implemented, 0.55.19** | Geometry selection no longer invokes navigation seeking; explicit transition navigation retains its behavior. |
| Quiet intro cannot move earlier than musical bar 1 | **Implemented, 0.55.20** | Explicit pre-roll rebases musical zero while preserving intro audio and coordinated clocks. |
| Ordinary tracks/split pieces unexpectedly link and move together | **Implemented, 0.55.58–0.55.59** | Unlink and independent default placements; ordinary add/drop/split/edit no longer manufactures transitions. Add Transition is explicit; Auto Arrange intentionally links. |
| Transition frames should move independently and overlap freely | **Implemented, 0.55.59** | Source-relative frame edges do not move songs; four-lane frames may overlap and exceed 64 bars within shared audio. Mute outside bounds is saved, optional and off by default. |
| Delete only one split piece; effects should continue across pieces and gaps | **Implemented, 0.55.62–0.55.64** | Piece selection/Delete this piece is distinct from group/range removal. Original-track VST and built-in controls share ownership; tails continue through gaps/endings. Separate placements remain independent. |
| Delete leaves unwanted automatic tempo markers; New leaves stale state/Bass line | **Implemented, 0.55.53 / 0.55.57 / 0.55.62** | Remove owned automatic markers, retain valid surviving ownership, invalidate pending New jobs and clear selection/view state. Manual tempo markers remain intentional. |
| Later tracks retain track two's tempo; ramps too short | **Implemented, 0.54.7 / 0.55.56** | Live additions preserve existing voices/manual horizon, then add future targets. New changed-BPM targets need 64-beat spacing; insufficient room uses the existing map. Manual maps are not retroactively spread. |
| Auto Arrange stops after one/few tracks or fails with generic blend-room message | **Implemented, 0.54.9 / 0.55.57** | Remove uncalibrated key-margin admission, avoid deferred count overwrite and repair source/fade roundoff. Copied-catalogue probe completed 40/40; musical listening acceptance remains open. |
| Energy directions cannot complete; genre overly restrictive; joins repetitive | **Implemented foundations, 0.55.44–0.55.45** | Soft whole-mix progression, gain-independent activity hints, soft genre and seven supported blend styles. Semantic vocals/drops, deeper route search, genre palettes and richer DJ techniques remain planned. |
| Extend existing mix instead of replacing it | **Implemented, 0.55.57 / 0.55.62** | Extend retains existing clocks/joins, excludes used sources and counts additional material; defaults on for a nonempty mix. Final outgoing handover may change. |
| Bass swap at a shared natural section, not arbitrary midpoint | **Partial, 0.55.22 onward** | Paired measured section identities drive placement/handover; explicit fallbacks remain. Reliable semantic phrasing, vocal clashes and musical quality still require independent review/listening. |
| Stem extraction exists but cannot be mixed/soloed or reused | **Implemented foundations, 0.55.47–0.55.53** | Offline Demucs jobs; Use on timeline; aligned subrows, global stem Solo, volume/pan authored curves, original A/B and validated cached reuse. Independent stem EQ/VST, extraction editing and live stem writes remain open. Separation quality/listening is not fully accepted. |
| Library preview should use master except during performance | **Implemented, 0.55.63–0.55.64** | Arrange preview uses master gain/mute and pauses arrangement without moving its cursor. Record/Perform uses explicit private routing and never falls back to audience output. |
| Arm lane/record/preview terminology is incoherent; preserve the whole performance | **Implemented, 0.55.64** | Two modes: Arrange and Record/Perform. Independent takes preserve supported continuous/discrete actions, pause/tails, Return, seek/replay, event editing and export. Original arrangement remains available. |
| Prepare tracks and complete transitions privately while audience continues | **Implemented, 0.55.64; hardware acceptance open** | Independent transport/DSP, track/grid/transition/A/B/C preparation, private level and explicit Apply/quantized Launch. Separate device or disjoint stereo pair; stale targets reject. |
| Preserve exactly what listeners heard | **Partial, 0.55.64** | Optional threaded master WAV captures after master processing/before monitor mute, excluding private audio. Failure retains a labelled partial file without stopping programme/take. Live notes/opaque plugin state/random plugins require WAV; editable MIDI-note recording and long RF64 qualification remain open. |
| Testing leaves large fixture data and duplicate runtimes | **Implemented retention; maintenance ongoing** | Ordinary runs remove generated audio/sessions/TEMP, retain compact evidence, and use exact-hash canonical runtime assets/restoration mappings. This cleanup's exact removals are recorded separately. |

## Complete numbered request register

Detailed acceptance criteria survive in the [archived finishing plan](history/FINISHING-PLAN-20261024.md). This table replaces its stale status labels.

| ID | Request | Current status and remaining scope |
| --- | --- | --- |
| P01 | Compact musical key notation | **Implemented 0.55.1.** Underlying key identity/unknowns retained. |
| P02 | Orange application popup/menu borders | **Implemented 0.55.2; extended 0.55.62** across alerts/callouts/detached/effect-host frames. Third-party editor pixels retain their owner. |
| P03 | Ruler double-click starts playback | **Implemented 0.55.2.** |
| P04 | Active mapping title on desk | **Implemented 0.55.3 / 0.55.8.** Original startup mismatch remains open under MIDI reliability. |
| P05 | One controller/audio settings button and device status | **Implemented 0.55.3.** |
| P06 | Reachable automation lines/nodes and unity detent | **Implemented through 0.55.15 / 0.55.39 / 0.55.42.** 0–130% gain scale, 100% movement detent, ordinary node editing; human dense-curve acceptance open. |
| P07 | Popup hints and on/off switch | **Implemented 0.55.7.** Complete normal foreground/assistive journey remains acceptance work. |
| P08 | Mapping lifecycle/reconnect reliability | **Partial.** Software lifecycle implemented 0.55.8; original outage/profile cause and physical acceptance open. |
| P09 | Useful confidence display/calibration | **Partial.** Numeric CONF implemented 0.55.25; independent calibration open. |
| P10 | Diagnose/fix playback breakup | **Partial.** Counters, bounded diagnosis and 0.55.53 lock-scope repair delivered; physical listening outcome open. |
| P11 | Detachable lower Editing console; VST at right | **Implemented 0.55.11**, compacted 0.55.46; dock/collapse behavior extended 0.55.52. |
| P12 | Full segment/range/group editing | **Partial.** Clipboard, reverse/loop, split/trim/slip, global ripple/gap close, before/after insert, ranges, rigid groups, Repeat/Crop/Replace delivered through 0.55.57. Partial linked-transition splicing, node groups, protected ranges and changing-tempo group retiming remain. |
| P13 | Reusable treatments/snippets/sample launching/live loops | **Partial.** Named/portable transition presets exist; snippets, sample child lanes and general live loop capture/release remain planned. |
| P14 | Named/rearrangeable workspaces | **Partial.** Named layouts and six console orders delivered 0.55.32; arbitrary docking and full human display acceptance remain. |
| P15 | Private track and whole-transition preparation/performance | **Implemented 0.55.64 with acceptance open.** Current two-mode contract supersedes the separate Performance-mode plan; physical routing/listening and broader qualification remain. |
| P16 | Plugin compensation/isolation, band effects and mastering | **Partial.** Master EQ/normalise/limiter/spectrum and in-process VST hosting exist. Full plugin delay compensation/crash isolation, new band routing and the complete optional pre-export mastering workflow remain. Active reported-latency inserts are blocked. |
| P17 | Continuous recovery and faster preparation | **Partial.** Recovery snapshots/restore and cache/loading foundations exist; genuine interruption, pressure/corruption/long-session acceptance and further measured optimization remain. |
| P18 | Live controls and polished FLkey workflow | **Partial.** Scheduled pause, target hotcues, takes and private launch exist. Physical pickup/feedback, full controller guidance/notification and complete next-track/fade/cue workflow remain. |
| P19 | Independently accurate constant-tempo 4/4 detection/key | **Partial.** Targeted repairs/retries delivered; independent corpus/thresholds and musical acceptance open. |
| P20 | Editable phrase/section/vocal annotations | **Partial.** Measured hints, reusable labelled S boundaries and activity profiles exist; semantic recognition/vocal clash warnings remain planned. |
| P21 | Categories/tags/crates and audio key shifting | **Planned beyond current metadata/search/genre foundations.** Smart crates and semitone/key-sync audio audition not established. |
| P22 | Section-aware assisted arrangement and alternatives | **Partial.** Paired sections, editable treatments, styles, soft energy/genre, Extend and timeline-order mixing exist. Reliable musical phrasing, richer techniques and complete listening acceptance remain. |
| P23 | Exact-duration mixes and drawn energy curve | **Planned.** Approximate duration and soft directions do not fulfil this. |
| P24 | Opt-in preference learning and related mix versions | **Planned.** |
| P25 | MixMeister `.mxm` and DJ-library exchange | **Planned.** Existing playlist interchange is not native arrangement compatibility. |
| P26 | FL Studio send/return, Ableton exchange/live sync | **Planned.** |
| P27 | Variable-tempo source maps and other meters | **Deferred.** Constant-tempo 4/4 remains the priority. Project tempo ramps already exist. |
| P28 | Aligned stems and advanced stem creation/editing | **Partial.** Separation and timeline mixing delivered; stem effects/controller modes, transition/extraction editing and drum replacement remain. |
| P29 | Fills/loops/sequencer/sampler/note recording/MIDI export | **Planned beyond existing instrument input and control automation.** Live master WAV is implemented separately. |
| P30 | Built-in synth/effects creator and key-aware note/chord tools | **Planned.** |
| P31 | Stronger forge/attract/weld/fusion effects | **Partial.** Attract, optional forge playhead/sparks and reactive desk delivered; further intensity/smoothing/fusion work awaits playback acceptance. |
| P32 | Audio-reactive dancer and Synesthesia/video lane | **Wishlist/planned.** |
| P33 | Six light/dark themes and full portable skins | **Planned.** Current Alloy theme is implemented; six variants/full skin sharing are not. |
| P34 | API AI mixing/music platform integrations | **Feasibility planned.** |
| P35 | Dedicated controller hardware | **Feasibility planned after existing controller workflows settle.** |
| P36 | WAV/MP3/FLAC choice, label splitting, publishing/CD | **Partial.** Arrangement/take/live WAV paths implemented; compressed formats and later distribution features remain. |
| P37 | First-run and revisitable tutor | **Planned.** Hints/shortcut reference do not fulfil the guided tutor. |
| P38 | Exhaustive function/control/background-work audit | **In progress.** Product/editing/automation audits and 94-rule framework exist; 281 preliminary control obligations remain NOT RUN/incompletely expanded. |
| P39 | End-to-end acceptance and distributable package | **Open/blocked.** Current regression, hardware/listening, clean environment, requirements and licence prerequisites remain. |
| P40 | Classify manual content | **Planned; manual last.** |
| P41 | Plain-language quick start | **Planned final deliverable.** Existing README/guides are interim help. |
| P42 | Complete user manual | **Planned after agreed workflows settle.** |
| P43 | Verify manual/screenshots and newcomer journey | **Planned after P40–P42.** |
| P44 | Follow playhead toggle | **Implemented 0.55.2**, saved outside musical Undo; manual panning disables follow. |
| P45 | Subtler grid and brightness dimmer | **Implemented 0.55.3.** Zoom remains available through navigation controls. |
| P46 | Draggable section flags | **Implemented 0.55.33 / 0.55.38 / 0.55.50** for supported forward/unlooped occurrences, reusable exact source positions/labels; broader occurrences/semantics remain. |
| P47 | Remove question-mark suffixes | **Implemented 0.55.9**, with explicit unknown/manual evidence retained. |
| P48 | Horizontal waveform layouts and colour/rendering variants | **Planned beyond existing waveform views and real stem subrows.** Full overview/detail stacks, stationary playhead, mirrored/RGB/three-band variants remain. Vertical layouts were excluded. |
| P49 | Build-derived version in header | **Implemented 0.55.25.** |
| P50 | Vertical amount handles for supported hard cuts | **Implemented 0.55.25 / 0.55.31 / 0.55.42**, with precise/native access; human feel/listening acceptance open. |
| P51 | Coloured HC1–HC9 edit/navigation/live launch | **Partial.** Instance cue editing/stopped locate 0.55.36, target-only quantized playing jumps 0.55.41, event recording 0.55.64. Full HC MIDI/controller acceptance remains. |
| P52 | Consistent song/segment/lane and EQ terminology | **Implemented pass 0.55.25**, owner labels 0.55.26 and later UI cleanup; continuing complete-control/manual review remains. |
| P53 | Both overview zoom edges | **Implemented 0.55.25**, maximum doubled to 1,600 px/s in 0.55.40. |
| P54 | Standalone mobile/tablet controller | **Feasibility/planned.** No accepted prototype, platform or measured latency. |
| P55 | One coherent automation editing area | **Implemented core 0.55.24**, ordinary preset nodes and later fixes; broader human workflow acceptance remains. |
| P56 | Fully collapse/restore right inspector | **Implemented 0.55.25**; left Library/Playlist collapse added 0.55.42. Full human display/accessibility acceptance remains. |

## Earlier requests and changed decisions

| Earlier request | Current disposition |
| --- | --- |
| Native MixMeister-style arrangement workflow and Alloy hardware appearance | Implemented foundations: library/Playlist, native menus, configurable layout, graphite/copper hexagonal controls and dark title bars. Full MixMeister equivalence is not claimed. |
| Window title bar and bottom controls must fit the monitor | Window fitting delivered 0.3 and subsequent layout increments. Full DPI/multiple-monitor qualification remains open. |
| Detailed waveforms at high zoom, grid lines and precise tempo strip | Detailed PCM views/pixel alignment and bottom tempo strip delivered through 0.12/0.16/0.49; maximum zoom now 1,600 px/s. Broader P48 variants remain planned. |
| Remember library artwork/artist/analysis, show pending work immediately | Delivered 0.8/0.10/0.11/0.15 onward. Current canonical storage/cache ownership and preserved production data take precedence over early same-folder-sidecar descriptions. |
| Label markers and playlist/tracklist exchange | Implemented foundations 0.9 onward. Does not establish `.mmp`/`.mxm` compatibility or full exchange parity. |
| Split, duplicate, independent IN/OUT, trim, reverse/loop and audio slip | Implemented across earlier editing releases and current P12. Current original-track effect sharing supersedes early per-piece effect descriptions. |
| Live addition preserves currently playing audio and goes to incoming tempo | Implemented foundations and later live-tempo fixes; physical long listening remains acceptance work. |
| Independent transition bands, paired filters, locks, named presets and A/B/C | Implemented through 0.40 and later ordinary-node/private-rendering updates. Old procedural-generator instructions are historical. |
| Stereo peak panel, master gain/mute, spectrum, normalization/limiter and endings/tails | Implemented foundations; current shared-tail/master capture refinements included. Physical clipping/fidelity acceptance and broader P16 remain. |
| Long navigation, source streaming and 60/90-minute playback/export | Historical native capacity/navigation evidence exists. It does not qualify the changed 0.55.64 engine, current hardware or long live recording. |
| Eight mixer lanes | **Superseded:** explicit four-lane maximum delivered 0.54.2. Current lane count is four. |
| Automatic transition and Crossfade/16-bar creation on every ordinary addition | **Superseded by 0.55.59:** ordinary placements independent; explicit Add Transition/arranging owns joins. |
| Always mute outside transition edges | **Superseded by 0.55.59:** optional per-transition Mute outside bounds, off by default. |
| Empty parameter curves should invent start/end nodes | **Superseded by 0.55.42:** Add creates the requested point; explicit endpoints remain available. Curve bending is retained. |
| Couple S boundaries and HC cues | **Retracted:** independently editable; reusable source S defaults and placed HC slots have separate ownership. |
| Separate Arrange, Record and Performance modes | **Consolidated:** Arrange plus Record/Perform, with optional private routing and master WAV. |
| Require old mixes/arrangements to retain legacy behavior | **Removed from development scope:** no compatibility layer or legacy-only test effort required. Original user data still protected. |
| Reset analysis on every detector-development update | Standing historical policy, constrained by later preservation instructions. Latest compatible updates explicitly retained analysis/corrections/caches/references; this cleanup performs no production reset. |
| Alignment-study experiment / reserved-source investigations | **Stopped/paused:** retained as historical evidence; this status/cleanup request does not resume experiments or analyse reserved recordings. |
| Three-app import timing comparison | Report delivered, **partial**: Alloy/DJ.Studio measurements exist; no defensible Fusion timing. Earlier timings are historical measurements. |
| Promotional video and development control bridge | Isolated native bridge/source integration implemented and documented. Sibling promo workflow/output is separate from app release qualification; no finished promotional film is established by this report. |

## Audit findings and acceptance still open

The 21 October audit's thirteen findings map as follows: **AUD-001/002** repaired in 0.55.13; **AUD-003** in 0.55.23; **AUD-004** has the 0.55.53 lock repair but unresolved listening; **AUD-005** has owner labels and 0.55.64 original-track sharing with full control/hardware acceptance open; **AUD-006** has native rows/cells/status/segments/transitions/labels/nodes/tempo/amount access through 0.55.31, with the screen-reader journey open; **AUD-007** current-file Save/Save As/document identity delivered 0.55.25; **AUD-008** treatment truth improved 0.55.35/0.55.39; **AUD-009** compact metadata improved; **AUD-010** remains a visual-policy decision, with the later explicit emblem-animation request preserved; **AUD-011** has hints, shortcut reference and build version but tutor/manual outstanding; **AUD-012** suffix scope and movement clarified, with human acceptance open; **AUD-013** startup mapping root cause remains open.

Editing-parity status: **EP01** audit/instructions delivered; **EP02** selection implemented with full gesture acceptance open; **EP03** partial despite delivered group/Repeat/Crop/Replace; **EP04–EP06** snippets/effect regions/effect families remain planned beyond existing preset/VST foundations; **EP07** separation backend implemented with listening limits; **EP08** timeline subrows/mixing implemented, advanced stem editing/extraction incomplete; **EP09** complete journey acceptance open.

The active release audit fixed four confirmed cases: stale current guidance, unsafe build parallelism, ignored retention errors, and missing stress/plugin evidence accepted by receipts. These are documentation/tooling fixes, not four new application fixes.

| Active investigation | Status |
| --- | --- |
| SUS-01: old automatic-link assertions and incomplete supplemental coverage | Open. Static map covers 22 gate suites plus ten supplemental entry points; current coverage must be traced, not assumed. |
| SUS-02: callback/private/take allocation, lock and deadline safety | Open suspicion; no fresh profiling establishes a defect or real-time safety. |
| SUS-03: audio breakup and stale active MIDI state | Open; see direct issue register. |
| SUS-04: every dynamic/native control reaches a working, accessible action | Open. 281 preliminary obligations include 90 MIDI actions and 75 typed menu IDs; all NOT RUN. |
| SUS-05: recovered/generated material and transitive model/runtime rights | Open provenance/release review. |
| SUS-06: malformed take parser admits fractional counters/too many snapshots/nested payloads | **AUD-D05 repaired and included in ra05 stage.** Exact integral bounds, pure snapshot validation and64-snapshot capture limit have relevant fresh coverage within87 performance/parser checks. Whole rules remain open. |
| SUS-07: reserved MIDI/plugin-state take event kinds may be silently ignored on replay | **AUD-D06 repaired and included in ra05 stage.** Unsupported kinds rejected; supported kinds freshly covered within87 performance/parser checks. |
| SUS-08: asynchronous take popup/event-number dialog may retain stale indices or capture-state admission | **AUD-D07 repaired and included in ra05 stage.**29 owned callback checks freshly pass; physical gestures/whole journeys remain separate. |
| SUS-09: private-preparation popup/async library completion may retarget or miss changed mode/busy admission | **AUD-D08 confirmed/repaired and staged,25 October.**47 focused callback/worker checks pass after10 reproduced failures. Private-state/request/target/lane guards preserve newer work. |
| SUS-10: take-export chooser/deferred-tempo continuation may retain a stale ordinal | **Unconfirmed source suspicion.** Requires an owned stable-target/admission reproduction and export-path trace. |

All **H01–H11** human acceptance areas remain at least partly open: dense curve editing; amount-handle feel; cues; full editing/navigation; musical arrangements; FLkey lifecycle; real audio/private routing/plugin tails; independent music references; accessibility/DPI/themes/help; genuine recovery/long listening/package/newcomer workflow; and mobile control after a prototype. Native fixtures and screenshots address parts of these areas, not the entire human journey.

Public release additionally requires resolved JUCE entitlement, ASIO terms, SoundTouch relinking and other package provenance, plus clean-environment evidence and agreed numerical/listening/performance/accessibility/manual scope. [Licensing](LICENSING.md) and [audit status](release-audit/STATUS.md) own those requirements. No release is qualified by this report.

## Follow-up order

1. Continue after staged SUS-06–SUS-09 repairs and ra06 coverage hardening at unconfirmed SUS-10 export target/admission, remaining SUS-04 controls and SUS-01 supplemental oracles. Keep safe resource preflight/minimal checks and preserve the staged candidate/evidence.
2. Resume the 572 missing imports only through a later user-controlled scan or an explicit recovery request; retain valid analysis and the seven recovered records.
3. Confirm real-output listening/private routing and FLkey startup/reconnect behavior on the repaired build. Keep observed failures and driver/load details.
4. Establish independently reviewed constant-tempo 4/4/key references and acceptance limits; do not substitute internal grid consistency for musical correctness.
5. Complete agreed editing/effect/stem/controller scope, then the final bounded regression/package checkpoint when resources permit. Full manual remains last.

## Documentation and data cleanup

The accompanying maintenance pass replaces stale root continuity/roadmap status with current summaries, preserves dated detail under `docs/history/`, corrects operating-guide contradictions, and removes only identified generated test data or proven redundant copies. The subsequent review classified all 318 remaining files: 248 expired cache artifacts removed, 16 identical renders deduplicated while preserving paths, and 54 canonical/input/dependency files retained. No files remain unclassified from that inventory; all 70 retained evidence paths and 1,679 protected files remain byte-identical. Exact plans, hashes, journals and checks are retained independently under `.local/maintenance-20261024/` and `.local/cleanup-review-20261024/`. The [cleanup report](CLEANUP-20261024.md) records cumulative counts and preservation. Application source, build, production session, original music and preserved analysis/references are outside this maintenance change.

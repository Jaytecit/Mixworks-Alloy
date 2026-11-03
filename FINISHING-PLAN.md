# Mixworks current implementation roadmap

Updated 3 November 2026. [HANDOVER](HANDOVER.md) owns candidate/activation status. The numbered requirements and open acceptance work below remain preserved; old checkpoints are dated evidence, not current task instructions.

## Deferred: custom libraries / crates — Jay, 2 November 2026

Plan a simple in-app way for users to create and maintain their own custom
collections, using Rekordbox-style crates/libraries as an interaction reference.
Jay explicitly requests **planning later, implementation later**; this does not
expand the current visuals milestone. Future planning should resolve whether
collections cover music, visual media or both; manual membership versus saved
filter rules; useful simple building tools; and how collections reference existing
files without duplicating or disrupting the main library. These are open design
questions, not agreed features or a request to copy another product's interface.

## Current priority and acceptance

Follow [Alloy visual direction](docs/VISUAL-DIRECTION.md): stronger musical movement, readable whole-image and refracting-glass compositions, selective kaleidoscopes, much more colour variation and live blends for long track joins. The 123-scene catalogue is a foundation for refinement, not artistic acceptance. Keep the retained shortlist and judge short moving previews. The earlier one-scene/100-scene stopping points are superseded.

## Other engineering work — retained backlog

The authorized [release audit](docs/release-audit/STATUS.md) remains outstanding after staged SUS-06–SUS-09 repairs and ra06 coverage hardening, at unconfirmed SUS-10 take-export target/admission, remaining SUS-04 controls and SUS-01 supplemental oracles. The named gate now requires23 suites; it has not been run. Keep essential checks and respect actual responsiveness. Historical import recovery is not permission to restart scans or alter production data. Real-output/FLkey reliability and independent constant-tempo4/4/key accuracy remain open. Then complete agreed editing/effect/stem/controller scope and final package qualification; full manual last.

Use essential batched checks under the responsiveness override. No automatic full regression/stress/capacity run. Preserve user data, protected references, independent source checkpoints and failed evidence.

## All numbered requests

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
| P32 | Music-driven visual timeline | **Owned scene lane implemented:** 123 scenes / 738 looks, image and glass materials, live blends, musical movement, colour cycling and fullscreen output. Artistic/hardware acceptance remains open. Dancer, timed video and product video export remain deferred. See [visual direction](docs/VISUAL-DIRECTION.md). |
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

## Audit and human acceptance

A01 menu/source selection and A02 required-source admission are delivered. A03 has owner clarification and track sharing, with audio/MIDI acceptance open. A04 has native musical-object access through tempo/amount widgets, with the screen-reader journey open. A05 has current-file Save, treatment summaries, compact metadata, movement scope, shortcuts and build version; final help/manual and human acceptance remain.

H01–H11 remain partly open: curves; amount handles; cues; editing/navigation; musical arrangements; FLkey; real audio/private/plugin behavior; independent music truth; accessibility/DPI/themes/help; real recovery/long listening/package/newcomer workflow; and mobile control after a prototype.

The active audit has 78 NOT RUN/16 BLOCKED rules. Its four documentation/tooling repairs and 37 passing tests/one skip do not qualify the application. [Audit status](docs/release-audit/STATUS.md) remains authoritative.

## Detailed criteria and historical decisions

[Archived finishing plan](docs/history/FINISHING-PLAN-20261024.md) preserves detailed P/A/H criteria and the historical instruction order. Its old delivery labels are superseded by the table above. [Earlier wishlist](docs/history/WISHLIST-20261024.md) preserves original requests and mappings R01–R15. [Editing parity plan](docs/DJSTUDIO-EDITING-PARITY-PLAN.md), [current mode contract](docs/ARRANGE-RECORD-PERFORM-PLAN.md) and [musical arrangement targets](docs/DJ-ARRANGEMENT-IMPLEMENTATION-20261023.md) retain specific design detail.

P40 classifies essential/useful/advanced/internal help; P41 writes the quick start; P42 completes the manual; P43 verifies every instruction/screenshot and newcomer journey against the accepted package. Preserve user-facing limits and consequences. These remain the final deliverables after the agreed application scope settles.

> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

> Dated audit/repair record. Baseline findings and pending labels below describe their original checkpoint. Use the [0.55.64 request register](REQUEST-STATUS-20261024.md) for reconciled delivery and the [active audit status](release-audit/STATUS.md) for current release qualification. Do not treat historical failures as current confirmed defects.

# Alloy editing system audit — 22 October 2026

Alloy already has most core transition and automation tools described in the DJ.Studio report. Its largest remaining editing gaps are **direct audio-range editing, reusable samples, movable effect regions and real stem editing**. Improve those workflows instead of replacing the current node editor or rebuilding the selected-transition console.

This is an audit and planning delivery. No native implementation, launch, production control, analysis reset or release is included. The companion [implementation plan](DJSTUDIO-EDITING-PARITY-PLAN.md) owns the proposed increments.

## Baseline and confidence

- **Alloy:** current working Source/CMake inputs match all **184** entries of accepted0.55.42/source5. Analysis11, session34, portable preset3; four fixed mixer lanes. The retained accepted summary contains **5,322 checks across22 suites**. Every referenced report hash was checked; these tests were not rerun for this documentation task.
- **Pixels:** inspected retained candidate captures `automation-editor-1250.png` and `audit-swap-1920.png` in `.local/na5-g/timeline-editing/`. They show the compact owner/parameter editor and wide paired ordinary Bass controls. They establish layout in those fixture states, not new gestures, musical quality or the user's current screen.
- **DJ.Studio installed evidence:** [22 October editing/stem report](research/DJSTUDIO-EDITING-STEMS-20261022.md), including its hashed frontend, partial semantic coverage and native/runtime unknowns; [18 October transition investigation](research/DJSTUDIO-TRANSITION-AUDIT.md), including bounded direct observations. Its original Alloy gap table predates numerous delivered fixes.
- **Current DJ.Studio documentation:** official editing, automation, transition and stem guides checked22 October. Studio View explicitly targets5.0; the installed report concerns the4.2 frontend. Documentation is a workflow specification, not an independently reproduced implementation or audio measurement.
- **Method:** compare each outcome with current owners/tests; distinguish implemented, partial, absent in the audited model/routes, and unverified acceptance. Consulted [skills.sh gap-analysis](https://www.skills.sh/wojons/skills/gap-analysis) for the requirements-versus-implementation method. No extra skill installed or delegated review.

Independent evidence: `.local/editing-parity-audit-20261022/audit-provenance.json` and `before-source-docs.zip`. This freezes source/document identity independently of Git without another model/runtime bundle. The checkout already contained substantial uncommitted development; this audit preserves it.

## Comparison by editing outcome

Evidence labels: **R** = supplied installed/report evidence, **D** = current official documentation, **A** = current Alloy source/tests, **V** = inspected retained Alloy pixels. Implemented means an actual code path exists; it does not imply this audit exercised it live.

| Outcome | DJ.Studio evidence | Alloy0.55.42 | Decision |
| --- | --- | --- | --- |
| Edit only the selected transition | R: pair-local controls, length and locks; D: frame/song movement | **Implemented.** Local length/preset/bands/filter, separate insertion defaults, I/O and frame handles | Preserve; finish hands-on acceptance |
| Independent Volume/Bass/Mid/Treble shapes | R/D: independent mode/amount/gap | **Implemented.** Ordinary source-owned preset nodes, sharp EQ handoffs, independent amounts and explicit conflicts | Keep one curve owner |
| Add, move, bend, precisely edit and delete automation | D: point editor and line shaping | **Implemented.** Console/footer/shortcuts/context/exact entry, Read/Touch/Latch and owner scope; A/V | Consolidate discovery and dense-curve acceptance |
| Save treatments and compare alternatives | R/D: user transition presets | **Implemented with limits.** Session/portable node templates and A/B/C rehearsal; preview uses programme transport and reset effect history | Private monitoring remains Performance work |
| Cut/copy/paste/split/reverse/loop one segment | R: sample handler branches; D: audio editing | **Implemented.** One-segment clipboard, independently cloned effects, slicing, finite loops and duplicate | Build range work on these owners |
| Select a time range and edit it directly | D: cut/copy/crop/repeat portions of audio | **Missing general workflow.** Selection identifies a segment or one node, not an editable audio interval | Highest functional priority |
| Select and move/copy several objects | R: sample multi-selection branch; runtime behavior incomplete | **Missing in audited timeline state.** One selected clip, one optional clipboard Clip, one node selection | Add explicit selection sets; exact reference gestures remain unverified |
| Close time or insert safely | D: range deletion rejoins audio | **Partial.** Global ripple-delete/close-gap and previewed before/after song insertion exist; overlapping clips block time removal | Extend to ranges; retain explicit Ripple |
| Reusable samples/overlays, extend and repeat handles | R: sample copy/repeat/extend; documented stem-to-sample use | **Partial foundation.** Ordinary clips overlay/loop, but no reusable snippet library or sample child-lane workflow | Reuse clips/media and playback |
| Move/resize an effect region as one object | R: effect-block handlers; D: region editing/favourites | **Absent first-class model.** VST chains and parameter curves exist; no owned effect-region collection/identity | Add real region and processor ownership |
| Noise, echo/reverb exits, loop/gater, brake/scratch, side-chain, pitch/key effects | R: categories; D: effects guide | **Partial at best through VSTs.** EQ/cutoff and finite loops do not implement those local effect families | Stage real DSP/rendering |
| Pair-local tempo policy | R: Off/Free/Half/Full; D: local sync/ramp options | **Partial.** Project tempo/prepared clocks exist; TransitionLink has no equivalent policy record | Resolve clock ownership first |
| Separate audio and automate transition stems | R: job/cache dispatch and2/4 subviews; D: independent layers | **Absent.** No separation job/cache or stem routing/automation owner in audited model | Jobs, aligned playback, then editing |
| Extract a stem range and mute its original counterpart | D: samples with linked original-stem mute | **Absent.** Requires ranges/snippets and aligned stems | Final integration slice |
| Scope, keyboard access and compact layout | D: changed5.0 selection/scrolling defaults | **Partial acceptance.** Native segment/frame/node access and compact/detached controls exist; V shows long selectors and many surfaces | Test complete journeys; preserve Alloy bindings |

DJ.Studio describes waveform-range editing and repeated paste in its [audio editing guide](https://help.dj.studio/en/articles/8207056-editing-audio-files-cut-copy-paste), and zoom-dependent phrase/bar/beat selection in [Studio View5.0](https://help.dj.studio/en/articles/8106005-studio-view-editing-arranging-controls). These establish a range-workflow target, without requiring its destructive Delete default or minimum edit unit.

The [automation/effect guide](https://help.dj.studio/en/articles/8213331-automation-editor-and-effect-blocks) describes movable effect regions and parameter automation. Its FAQ currently says controller automation and direct manual-volume-node copying are unsupported. Preserve Alloy's MIDI automation; node-group copy would be an Alloy enhancement rather than established parity. The [Transition Editor](https://help.dj.studio/en/articles/8106059-transition-editor) describes additional effects, stems and pair-local tempo controls.

The [Stem Separation guide](https://help.dj.studio/en/articles/9112447-stem-separation) describes independent layers, range extraction, linked original-stem mute and different editing/export qualities. Alloy needs the complete source-to-edit-to-export chain. The report's native dispatch names do not establish available models, rights, quality or performance for Alloy.

## Findings and boundaries

| ID | Priority | Evidence and consequence | Plan owner |
| --- | --- | --- | --- |
| ED01 | High | `Workstation.h`: singular selection and optional Clip clipboard; `Workstation.cpp::Timeline::mouseDown` selects one clip/node. Copying a passage requires manual splits | EP02/EP03 |
| ED02 | High | `TimelineEditing.cpp::performTimelineAction` has segment actions; `Model.cpp::closeTimeRange` requires other clips wholly outside the interval. General range splicing/protection remains unfinished | EP03 |
| ED03 | High | `Model.h` has clips, links and curves, but no effect-region owner. Parameter curves alone cannot provide a movable effect instance with duration/tails | EP05 |
| ED04 | High, large dependency | Audited model/engine/transition routes have no stem source/routing owner. EQ Bass is frequency processing, not an isolated bass stem | EP07/EP08 |
| ED05 | Medium | `trackCount=4`; overlays use ordinary lanes. Treating every stem as another mixer track would exhaust this model and affect controller assumptions | EP04/EP08: child lanes/internal voices |
| ED06 | Medium | Paste and Duplicate refuse whenever any transition is locked because they may add tempo points. This is conservative scope, not proven corruption; safe disjoint edits need clock/lock proof | EP03 |
| ED07 | Medium | TransitionLink stores bands/filter/alternatives/lock, without pair tempo policy or effect families. Inspector labels alone would misrepresent capability | EP06 |
| ED08 | Medium | Older sections of `TIMELINE-EDITING.md` describe invented endpoints and800 px/s; `TRANSITION-PRESETS.md` describes generated/manual layering and early formats. Newer preset guide/handover correctly describe ordinary nodes and1600 px/s | EP01 |
| ED09 | Medium | Native/bridge results do not prove the complete human journey, audible boundaries/tails, separation quality or physical MIDI/ASIO. Inspected captures are retained fixtures | EP01/EP09 |

This audit found structural/workflow gaps and documentation drift. It did not reproduce a new crash, corrupt save or audible defect, and does not turn historical failures into current bugs.

## Code map

- [Model.h](../Source/Model.h), [Model.cpp](../Source/Model.cpp): source clocks, curves, Project, slicing, time splicing and persistence.
- [Workstation.h](../Source/Workstation.h), [Workstation.cpp](../Source/Workstation.cpp): selection/clipboard, gestures, split/duplicate/remove and consoles.
- [TimelineEditing.cpp](../Source/TimelineEditing.cpp), [TimelineAutomation.cpp](../Source/TimelineAutomation.cpp), [AutomationUI.cpp](../Source/AutomationUI.cpp): command admission, node ownership, gestures and native access.
- [PresetNodes.h](../Source/PresetNodes.h), [PresetNodes.cpp](../Source/PresetNodes.cpp), [PresetNodeTests.cpp](../Source/PresetNodeTests.cpp): ordinary preset application, exterior preservation and PCM evidence.
- [TransitionUI.cpp](../Source/TransitionUI.cpp), [TransitionControls.cpp](../Source/TransitionControls.cpp), [TransitionStyles.cpp](../Source/TransitionStyles.cpp), [TransitionRehearsal.cpp](../Source/TransitionRehearsal.cpp): pair controls, portable treatments and audition.
- [ArrangementInsertion.cpp](../Source/ArrangementInsertion.cpp), [ArrangementInsertionUI.cpp](../Source/ArrangementInsertionUI.cpp): preview/candidate admission and later-arrangement protection.
- [Engine.h](../Source/Engine.h), [Engine.cpp](../Source/Engine.cpp), [WavExport.cpp](../Source/WavExport.cpp): real-time processing/resource ownership and export.
- [EditingTests.cpp](../Source/EditingTests.cpp), [TimelineEditingTests.cpp](../Source/TimelineEditingTests.cpp), [TrackOwnershipTests.cpp](../Source/TrackOwnershipTests.cpp): source/loop/reverse PCM, UI/model, plugin and persistence checks.

## Recommendation

Deliver **range editing/snippets first**, **effect regions/useful effects second**, then **separation plus actual stem editing**. Preserve selected-transition/node tools, source mappings, MIDI, explicit conflict handling and Undo. Functional outcomes are the target; pixel matching, proprietary code reuse and identical DSP are outside scope.

Core editing parity can be accepted before stems. Full parity within the report's scope additionally requires effect, tempo and stem milestones. Private headphone preparation, live event recording, publishing and the rest of DJ.Studio's product remain separate; the [finishing plan](../FINISHING-PLAN.md) and [Performance plan](PERFORMANCE-MODE-PLAN.md) retain those scopes.

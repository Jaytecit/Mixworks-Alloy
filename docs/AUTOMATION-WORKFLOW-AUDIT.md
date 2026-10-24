# Timeline parameter automation audit — 21 October 2026

The workflow is fragmented. Filter lacks the insertion shortcut/menu parity of EQ and volume; FX introduces another selection process; enablement and recording states are spread across menus and the Controller console. Existing node/curve improvements do not close these discovery gaps.


## P55 core implementation — 22 October 2026

The locally staged 0.55.24 consolidates automation in the existing Editing console. Its final native timeline suite passes **1,947 checks** and compact/wide/detached captures have been inspected; its fresh complete gate passes **4,055 checks across 22 suites with zero failures/errors**. The original audit below describes 0.55.20, with these newer implementation results kept separate:

| Finding | Implemented here | Remaining boundary |
|---|---|---|
| AUTO-01 | Shared guarded insertion, Filter/F parity, actual context-time seed, visible Add at playhead, retained FX target | Human workflow acceptance |
| AUTO-02 | Persistent explicit segment/lane/master and parameter selectors, deterministic coincident-curve choice, node/state feedback | A04 semantic/UIA access and human compact/detached journey |
| AUTO-03 | FX Show is view-only, including during an actual armed Touch take; first edit materialises storage with one Undo | Physical controller acceptance |
| AUTO-04 | Node Edit/Delete/Add context menu, visible node actions, Editing keyboard Delete cannot delete a segment | H01/H04 feel and discovery |
| AUTO-05 | Visible Read/manual override and Return to Read for the exact curve | A03 live-control ownership policy; live desk routing is unchanged |
| AUTO-06 | Separate Read/Touch/Latch mode, selected-owner lane/master arm, full armed-scope help and take activity | Existing Controller Record still arms all lanes/master; no new recording/routing semantics |
| AUTO-07 | Paired-filter navigation to the associated real Transitions inspector | Direct paired-filter nodes/conversion remain unimplemented |
| AUTO-08 | Separate visible, generated, saved-node and Read state; existing Clear manual nodes menu retained | Complete A04 access and wider treatment-reset journey |
| AUTO-09 | Editor bar/beat/signed project time, insertion status and exact/context entry agree | Historical drag status strings still use source-elapsed time; broader P52 cleanup remains |
| AUTO-10 | New editor/commands use node and Bass/Mid/Treble with explicit owner | Full-app P52 terminology inventory remains |

Browsing and exact-entry Cancel preserve recording. Active captured writes block node edits until explicit Read or Stop; guards revalidate lock/export/busy/changed documents before commit. No detector, saved-format, live cutoff routing or generated-filter DSP change. Native Save/parse retains exact FX identity and point values; existing reopening/regeneration suites remain part of full verification. This does not claim exhaustive non-identical treatment regeneration/reopen, human H01/H02/H04, musical listening or physical MIDI/ASIO acceptance. See HANDOVER and `.local/p55-20261022/REVIEW.md` for candidate/failure provenance.

## Evidence and limits

Source audit of 0.55.20 and inspected existing native `curve-tools-1440.png` and `paired-filter-inspector.png` from `.local/pr5-ui/timeline-editing/`. All 11 inspected Source files match the final candidate manifest. Receipt `.local/pr5-ui/receipt.json` identifies executable SHA-256 `fbd7b15de4e473da637ad838bb69b7e337e8668d07a8611d7757d039da24dc84`. These are retained fixture captures, not new gesture/listening acceptance or observations of the user's current mix.

Independent before-doc/source checkpoint and hash/capture provenance: `.local/automation-audit-20261021/`. No runtime change, launch, production control, regression run, reset or detector evaluation. The installed Impeccable native audit guidance informed this review, adapted to JUCE/Windows; mobile scoring and browser detectors do not apply. Consulted [skills.sh](https://www.skills.sh/) as required by repository preferences; no extra tools were installed and no subagents used.

## Current routes

| Task | Current route | Gap |
| --- | --- | --- |
| Choose built-in parameter | Select segment, use footer buttons/compact selector or click visible line | Footer depends on selected segment visibility and width; no persistent full target editor |
| Choose FX | Footer FX, Automation > Choose effect control, or Add Effects Parameter Marker | Separate plugin/parameter picker and setup; selection is not view-only |
| Add Volume/Bass/Mid/Treble | Timeline RMB or V/B/M/T in cursor context | Selection and insertion commands are separate; names/order differ from footer |
| Add Filter | Select Filter, then Automation > Point at playhead and Apply, or edit a neutral endpoint | No Add Filter Marker counterpart; line click/double-click does not add a node |
| Add precise node | Automation > Point at playhead; enter timeline seconds/amount, Apply | Seeds at playhead even when submenu opens at a different RMB position; bypasses Snap |
| Move node | Drag time/value; Snap applies horizontally, Alt bypasses it | Both axes move together; independent exact time/value needs dialog |
| Bend curve | Drag unequal saved segment or supported generated fade vertically | Line drag already means bend; cannot silently redefine it as insertion |
| Switch nodes | Timeline focus, Left/Right; Enter edits | Mostly explained through hover/status; no persistent previous/next controls |
| Delete node | Delete selected node, or RMB directly on saved node | RMB deletes immediately; display-only generated handles cannot be deleted |
| Enable/disable/clear | Automation submenu: Read selected curve / Clear selected curve | Hidden state; Read also names recording mode; clearing manual nodes leaves generated treatment |
| Live adjustment/recording | Controller; Record enables Touch for all lanes/master; detailed modes/arming in menus | Broad scope separate from selected curve; manual movement can disable its saved curve |
| Generated paired Filter | Lower Transitions inspector | Different owner/processor from segment LP/Off/HP nodes; not an ordinary editable node curve |

Sources: [footer/selection](../Source/TimelineAutomation.cpp), [menus/insertion](../Source/TimelineEditing.cpp), [FX/precise entry/modes](../Source/AutomationUI.cpp), [gestures](../Source/Workstation.cpp), [recording/control](../Source/Automation.cpp), [Controller](../Source/PerformanceDeck.cpp).

## Prioritised findings

### AUTO-01 — P1: inconsistent insertion, especially Filter and FX

`TimelineEditing.cpp:15–25` offers Volume/Treble/Midrange/Bass/FX markers but no Filter. `TimelineAutomation.cpp:104–116` selects Filter and directs users to a right-click/playhead command. `Workstation.cpp:1048` selects on line double-click; ordinary line drag bends. The selected Filter editor therefore has no obvious interior-node action. FX requires a separate picker.

**Required:** visible **Add node** for every supported target, **Add node here** at the actual context-click position, and explicitly named **Add at playhead**. Add Filter menu parity. Retain an existing FX insert/parameter without repicking. Define the insertion gesture visibly, preserving line selection and drag-to-bend. Share command owners across editor/menu/shortcut. Owners: **P55/P06/P12**.

### AUTO-02 — P1: selected target is distributed and sometimes disappears

`curveToolsReserve`, `clipFooterBounds` and `curveToolsBounds` hide tools when lane/segment/visible space is insufficient; buttons become a dropdown at narrow widths. Only the selected segment has these tools. The 1440 capture has no Add node button; the compact inspector capture shows the small remaining timeline. `automationCurveAt` chooses one nearest curve, preferring an already selected curve, without explicit cycling for coincident curves. Clicking segment body clears the automation target.

**Required:** a persistent automation area in the existing Editing console, showing segment/lane/master owner, parameter, selected node and state. Keep footer shortcuts; list existing FX curves and offer deterministic overlapping-curve selection. Browsing must not alter sound/transport/history. Owners: **P55/P06/A03/A04**, with **P56** freeing timeline width.

### AUTO-03 — P1: FX Show curve changes musical state

`chooseAutomationPlugin` (`AutomationUI.cpp:107–134`) sets Read mode, calls `remember()` and `ensurePluginAutomation`, then selects. `Automation.cpp:27–31` can create a stored parameter lane. Merely showing FX can therefore dirty the mix/create Undo and end recording mode, unlike built-in footer selection. The preparation path also lacks equivalent upfront transition-lock validation; reproduce before claiming a lock bypass.

**Required:** make FX selection view-only. Materialise storage only on the first admitted edit/record action, with one Undo and lock/export/busy revalidation after dialogs. Preserve an active take while browsing, or expose any necessary finish-take action explicitly. Owner: **P55**.

### AUTO-04 — P1: RMB on a saved node deletes immediately

`Workstation.cpp:877–883` erases a saved node on popup mouse-down; elsewhere RMB opens a menu. On a generated adjustment handle it instead explains that editing is required first. Delete/Enter/Left/Right already exist (`TimelineAutomation.cpp:409–449`) but depend on focus and node selection.

**Required:** node context menu with Edit/Delete/Add node here, explicit deletion, preserved Delete/Undo, and visible previous/next/time/value/Delete controls. Explain generated adjustment handles and Reset treatment versus Delete manual node. Owners: **P55/P06/A04**.

### AUTO-05 — P1: live adjustment silently disables curve playback

`Engine::setControlValueAt` ends with `if (!record && curve) curve->enabled = false` (`Automation.cpp:194`). Recording requires playback, arm, Touch/Latch and applicable time. Manual movement outside that state retains points but disables the target curve. Cutoff explicitly stays lane-owned while most other controls can route to the selected segment (`Automation.cpp:137–146`); selecting segment Filter does not make the visible live cutoff knob control that curve.

**Required:** show exact owner and **Read / manual override / recording** state beside controls, with obvious Return to automation. Specify override/return behaviour; never silently retarget lane cutoff to a segment. Cover stopped/playing/unarmed/outside-segment/active-take cases. Changed routing/record semantics require A03 agreement and full audio/ownership verification. Owners: **P55/A03/P08**.

### AUTO-06 — P1: conflicting modes and hidden arming scope

Recording **Off** means engine Read, while **Read selected curve** toggles curve enablement. FX help says **Touch/Write**, but menu says **Touch/Latch** (`AutomationUI.cpp:17–28,133`). Controller Record arms all four lanes/master in Touch (`Engine.cpp:1012–1017`). Touch release is 0.5 seconds without input (`Automation.cpp:129`), which needs explanation for hardware without release messages.

**Required:** show mode, armed scope and actual write activity together. Distinguish not recording from curve playback disabled. Use Read/Touch/Latch consistently, explain inactivity release, and offer selected-owner arming alongside explicitly labelled all-lanes/master recording. Selection/browsing must not unexpectedly stop a take. Owners: **P55/P08/P52**.

### AUTO-07 — P1: manual and paired transition Filter are different systems

Footer Filter edits segment cutoff nodes; generated transition Filter uses paired stages in Transitions. Generated curve address enumeration covers Volume/Bass/Mid/High only (`TimelineAutomation.cpp:134–145`). [Filter documentation](TRANSITION-FILTERS.md) confirms the separate processor; existing preset verification lists paired-filter cases as open. EQ success cannot establish editable paired-filter nodes.

**Required:** identify both owners and expose appropriate editing from the same automation area. Provide supported treatment editing or an explicit jump to its real inspector and limitations. Manual cutoff must not silently replace, double-apply or masquerade as a paired filter. Conversion, if chosen, must be explicit/undoable and verified for DSP/ownership/persistence. Owners: **P55/P12/P50**.

### AUTO-08 — P2: visibility, enablement and treatment ownership are conflated

Transitions Show/Only affects visibility; footer selects/reveals; Read toggles playback; Clear removes manual nodes. Empty segment curves expose neutral endpoints without storing points. Generated/manual Volume/EQ can share one effective line, and clearing adjustments leaves its base treatment. These useful distinctions are not presented together.

**Required:** separate Visible, Read enabled, generated treatment and manual adjustment indicators. Show/Only/selection remain view-only. Distinguish Clear manual nodes from Remove/reset treatment, preserving the correct owner. Owners: **P55/P06**.

### AUTO-09 — P2: time and value feedback differs across routes

`addTimelineAutomation` converts project time to source-elapsed time then reports that as “Marker added at” (`TimelineEditing.cpp:61–79`); the dialog uses signed project seconds relative to musical origin (`AutomationUI.cpp:148–150`). Moved/stretched/pre-roll segments can therefore show conflicting times. Insertion snaps, precise entry bypasses Snap, and the submenu seeds at playhead. Gain is percent, Filter LP/Off/HP Hz, FX normalised percent rather than necessarily its physical unit.

**Required:** consistent project bar/beat and signed project seconds, optional explicitly labelled source time, visible Snap/bypass rules, appropriate units and plugin display text where available. Cover move/trim/stretch/reverse/loop/pre-roll clocks. Owners: **P55/P52**.

### AUTO-10 — P2: terminology increases recall burden

HIGH/Treble, Mid/Midrange, node/point/marker and song/track/clip/lane all occur in this journey. FX errors say selected track even for segment inserts. Users must remember both location and interpretation.

**Required:** node for parameter points, distinct label/tempo/cue markers, Bass/Mid/Treble and explicit owner/plugin names. Hints teach one journey: select target → add node → edit → hear → Undo. Owners: **P55/P52/P07**.

## Foundations to retain and proposed completion

Retain native labelled footer buttons, keyboard node navigation, one-gesture Undo, neutral endpoints, exact entry, combined effective curves, gain unity detents, Alt free time, line-click selection and curve bending. Extend the existing Editing console instead of introducing another editor: **Owner → Parameter → Read/state**, with Add node, Add at playhead, previous/next, time/value and Delete, plus visibility and recording scope. FX selection retains a named insert/parameter. Paired Filter identifies its treatment and exposes/navigates to its real controls.

P55 acceptance covers all supported Volume/Bass/Mid/Treble/manual Filter/FX and segment/lane/master owners, both transition participants and all preset/creation routes already required by P06/P50. Include absent parameters, overlapping curves, compact/short/scrolled segments, dialog cancel/error, lock/export/busy guards, selection during a take, node versus segment deletion, Snap/Alt, project/source clocks, Undo/Redo, regeneration and save/reopen. View selection must preserve sound/transport/history; one admitted edit gives one Undo. Inspect wide/compact/detached captures, then obtain human H01/H02/H04 separately. The bridge can set up/capture but does not expose arbitrary node gestures. Use focused UI suites; full gate for engine/audio/ownership/timing/persistence changes under [TESTING.md](TESTING.md). No fresh tests are claimed here.

## Other requested finishing items

**P09 reopened:** raw confidence multiplies winner margin, winning-passage fraction and model agreement (`Analysis.cpp:570–586`); UI bands require 0.55/0.80 (`Workstation.cpp:2009–2041`). A small runner-up margin can suppress rating despite a correct key. This explains a possible mismatch; no new score distribution or accuracy measurement was made. Record the user's report of no observed wrong keys. Investigate calibration independently of chosen-key accuracy; if actual detector confidence cannot improve, implement the requested more positive ordinal presentation for supported valid detections, disclosing the revised policy and preserving raw evidence. Header **CONF**, numeric-only cells, explanations/manual/unknown status in tooltips/accessibility. Preserve missing/no-key distinctions without inventing high ratings for absent evidence. Do not resume protected reference investigations.

**P56:** layout, right splitter and preference reader enforce a 240-pixel minimum (`WorkspaceLayout.h:10`, `WorkspaceUI.cpp:373`, `WorkspaceLayout.cpp:107`). Dragging the timeline's right divider fully closed must hide the entire inspector, including master-output section, reclaim width and leave an accessible restore grip. Restore previous width and persist local workspace state. Keep master access via existing peak/Controller routes. The separate timeline peak strip has its own owner and is not silently included. Cover mouse/keyboard restore, compact/fullscreen, resize/detach and restart; no musical/transport/Undo change. Persistence changes follow the project's verification policy.

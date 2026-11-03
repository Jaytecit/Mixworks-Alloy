> Dated audit/repair record. Baseline findings and pending labels below describe their original checkpoint. Use the [0.55.64 request register](REQUEST-STATUS-20261024.md) for reconciled delivery and the [active audit status](release-audit/STATUS.md) for current release qualification. Do not treat historical failures as current confirmed defects.

# Alloy remediation plan — 21 October 2026

Based on [the evidence-backed findings](ALLOY_PRODUCT_AUDIT.md) and [coverage ledger](ALLOY_AUDIT_COVERAGE.md). **No repairs are implemented by this audit.** Preserve graphite/copper/hexagonal identity, original music, revision-9 analysis/corrections and compatible caches. Do not treat this as a detector update or release milestone. Sizes below are relative, not delivery commitments.

**Implementation follow-up, 21 October:** the user authorised beginning repairs. [FINISHING-PLAN.md](history/FINISHING-PLAN-20261024.md#audit-repair-priority--authorised-21-october-2026) now integrates all findings and owns their priority. Batch 1 / A01 is implemented and locally staged in **0.55.13**: unique typed menu IDs/disjoint transition commands, active-surface correction targeting and explicit context source selection. Build and **949 focused checks** pass (Alloy 883, browser 32, correction 34), with a scoped receipt and inspected candidate-native source-dialog capture. No full gate was run. OS-helper imagery did not match its isolated host; no OS-driven acceptance is claimed. [HANDOVER.md](../HANDOVER.md) records hashes, failure provenance, data preservation and remaining limits. Batch 2 / A02 is delivered in 0.55.23; its final complete loading/persistence gate passes3952 checks across22 suites, with47 supplemental bridge checks and actual native Locate picker acceptance/cancellation. The current handover owns final identity/evidence; P55 core is delivered in 0.55.24; P50/P53/P52 continue before broad P12.

## Batch 1 — Correct command identity and musical target selection

**Necessary repairs: AUD-001, AUD-002. S–M. No dependency on redesign, detector calibration or P12.**

1. Give top-level, submenu and context commands nonoverlapping typed IDs; dispatch through one table/owner with explicit allowed contexts. Fix Sections before other polish.
2. Pass source IDs into correction actions; resolve menu/shortcut targets from the active surface. Avoid implicit precedence between hidden library selection and current Playlist/timeline selection.

**Acceptance:** every Beatmatch length changes the default only, for clean/dirty/empty/playing/locked states; only Exit requests close. Library song A → Playlist song B → correction always names B. Check ordinary click, context menu, Ctrl+W, BPM/Key and the source dialog's commit/Cancel. No partial correction or lost arrangement on invalid state. Keep the existing unsaved guard.

**Verification:** focused native command-dispatch/selection checks plus the actual native reproductions, build and captures. Under TESTING.md these local UI repairs need focused suites; a full gate is required if correction ownership or shared analysis/model semantics change. Do not repeat thousands of DSP checks for a label-only follow-up.

## Batch 2 — Remove unrelated library files from project admission

**Delivered 22 October in 0.55.23.** Full catalogue references remain saved; only required audio gates opening. Native two-valid-clips plus unused-missing export, cached ten-clip/359-entry readiness apart from background work, normalized old/new bridge comparison and Locate/cancellation pass. Synthetic measurements do not establish the original real-library speed benchmark or musical/hardware acceptance. See `.local/admission-20261021/REVIEW.md`, HANDOVER and VERIFICATION for exact hashes, original failed candidates, retention and the complete gate.

**Necessary core repair: AUD-003. M. Can follow Batch 1 immediately.**

Admit referenced arrangement media as the critical path. Retain unrelated library metadata and mark unavailable entries without rejecting the musical project; hydrate/validate catalogue entries outside the critical loading transaction. Distinguish absent, corrupt and unsupported files. Add a precise required-source failure/relink path that preserves current and saved documents. Decide explicitly whether a session snapshot should carry the whole catalogue; do not silently drop its useful metadata while repairing admission.

**Acceptance:** the retained two-valid-clips + one-unused-missing-source fixture opens and exports its valid clips; missing required media is explained by path/owner, never a misleading format suggestion alone. Failed/cancelled required-source preparation retains the existing project and saved file. Save/reopen preserves corrections, transitions, effects and automation. Compare the same ten-track/359-entry project before/after and record readiness time separately from background catalogue completion. Reuse valid analysis/prepared caches.

**Verification:** fresh red reproduction, focused persistence/admission/cache checks, then **one complete gate on the final candidate** because engine/cache/persistence paths change. Include source identity changes, invalid documents, cancellation and session compatibility. Use `verification.run_checks`; keep hashes/reports/snapshots, not generated audio duplicates. No AppData reset.

## Batch 3 — Establish live control scope and output continuity

**Necessary clarification/diagnosis: AUD-005 and AUD-004; assess AUD-013. M.**

First expose existing owners clearly: clip title versus lane/all-songs, corrected volume/pan/EQ tooltips and per-group scope. Decide whether live desk level/pan/EQ should be lane-owned like cutoff; changing their routing requires explicit product agreement, not an incidental tooltip fix. Keep arrangement clip automation available through its own explicit owner.

Use the existing callback diagnostics to reproduce lock skips with the same real passage, no concurrent test suite, then controlled additional load. Identify lock holders and measure before making throughput claims. Include the observed modified-copy rehearsal clip warning in headroom/listening work; do not claim that it measured the untouched original mix.

Diagnose the existing MIDI startup mismatch on owned copies. Preserve named profiles/backups and identify the user's intended profile before restoring it. A normal restart loading `[]` is expected; the unresolved question is why the initial in-memory profile differed and how to explain/prevent that divergence.

**Acceptance:** later audio on a reused lane responds to the control whose displayed scope promises it, with an old ended clip selected. UI, mapped MIDI, Read/Touch, Undo/Redo and reopening agree. Track lock/pending states explain refusal. Counter deltas and captured/listened output establish continuity; no causal claim based on a single accumulated counter. Profile Open/Save/Learn/Clear/failure/restart outcomes match their descriptions.

**Verification:** scoped UI checks for labels; actual mouse/MIDI ownership workflow and production PCM probes if routing changes. Engine/audio/automation ownership repairs require the complete gate once on the final candidate. Physical FLkey/output acceptance remains required even with passing synthetic tests. No driver/buffer changes or profile restoration should be hidden in this work.

## Batch 4 — Make primary musical workflows accessible

**Necessary access repair: AUD-006. L. Shares explicit identity work from Batches 1/3.**

Progress through 0.55.29: native row/cell/footer, segment/transition/label and visible automation-node access; actual parent/child hierarchy and offscreen discovery, stable owner/time binding, guarded exact editors and bounded dense traversal now have native proof. Tempo/amount objects were added in 0.55.31 with actual native hierarchy, readonly values, existing precise editors, dense navigation and focus-scroll continuity proof; the complete Windows screen-reader journey remains open. See HANDOVER for exact scoped evidence; this is not full AT acceptance.

Add accessible table cells/row metadata, timeline/clip/transition/marker/automation objects and action/value interfaces; expose current selection, control scope, filename/dirty state and status feedback. Preserve keyboard alternatives and existing well-described standard buttons/dials. Implement this with JUCE/Windows UIA, not HTML ARIA or a nonexistent table text hook.

**Acceptance:** a keyboard/screen-reader user can identify two songs and their BPM/key, select/place a clip, seek, perform a basic edit, identify its owner/result, Undo, save/reopen and export. Native focus remains visible and stable across tabs/detach/dialogs. Check text/DPI/high-contrast/readability settings and hit targets appropriate to Windows. UIA dumps are supporting evidence, not the final screen-reader journey.

**Verification:** focused UIA/accessibility and native tests; hands-on assistive-technology acceptance at representative sizes. Full gate only if shared model/engine behavior changes. Avoid tests that mirror paint constants.

## Batch 5 — Simplify preservation and explain arrangement decisions

**Product corrections: AUD-007, AUD-008; layout/help proposals AUD-009, AUD-011, AUD-012. S–M per coherent change.**

- Make Save/Ctrl+S update the current file; add explicit Save As and visible document identity. Preserve transactional saves/backups and close guard.
- Replace “Custom / edited” with truthful current-treatment wording and a concise band/filter summary. Treat stored generator provenance as an optional persistence change with corresponding tests.
- Prioritise title/BPM/key together at compact sidebar widths. Preserve user column control and make confidence detail available without dominating the track identity.
- Preview suffix-move scope at hover/focus and explain Shift audio slip, retaining the approved pinned-incoming behavior.
- Add Help/About/shortcut access to existing task docs and build-derived version (P49). Reconcile stale current-version statements while keeping dated research/design history labelled. The full manual remains last, after accepted workflows.

**Acceptance:** save/edit/reopen takes the expected route; cancellation/errors retain dirty state. An untouched auto mix is not described as user-edited; summaries match actual treatment after edits/Undo/reopen. Track title and BPM can be compared without a mandatory hover/scroll at the agreed compact width. Newcomers can find the guide; experts keep their existing accelerators.

**Verification:** focused workflow checks and actual compact/wide captures. Presentation changes require build plus visual inspection, not the full gate. Shared saved-format/provenance changes require appropriate persistence tests and one full final gate.

## Optional visual policy decision

**AUD-010. S. Separate from required repairs.** The owner previously requested full automatic attract/forge activation after Auto Arrange. If that policy is revised, retain a completion celebration while respecting the chosen Follow Windows/Steady policy. Test completion, Escape, saved forge policy and actual Windows motion behavior. Do not silently replace the approved identity or remove the requested effect as an audit “fix.”

Impeccable follow-up categories: `harden`/`clarify` for Batches 1–3, `harden`/`adapt` for access, `layout`/`typeset`/`clarify` for compact information and guidance, `animate` for an agreed motion-policy change, then **`polish` last**. These commands are optional workflow aids, not substitutes for the musical acceptance criteria above.

## Shared causes and sequencing

| Shared cause | Symptoms | Small coherent response |
|---|---|---|
| Implicit command identities/context | Quit collision, wrong correction source | Typed unique IDs and explicit target/context resolution, retaining useful alternate routes |
| Library versus document ownership coupled | Slow all-library admission, unused-file failure | Separate required project readiness from catalogue hydration; preserve metadata/cache compatibility |
| Hidden clip/lane/global scope | Live control confusion, plugin target language, stale-source editor | Visible ownership plus one explicit resolver per scope; change routing deliberately |
| Paint-only musical/status surfaces | Generic rows, inaccessible clips/points, unannounced results | Native semantic object model and accessible status, reusing stable identities |
| Engineering completion versus user completion | Passing suites despite broken menu, export toast without file proof, generated label ambiguity | Retain native command-driven journeys and persisted/PCM outcomes alongside tests |

Batch 1 has the highest ratio of impact to effort. Batch 2 follows because unrelated files must not deny access to saved work. Begin bounded audio/MIDI diagnosis in parallel only where it does not disturb user playback/data; fix demonstrated audio risks before optional visual intensification. Accessibility is core functionality, not the last cosmetic pass. Keep optional redesign out of the first repair candidate.

No public binary/package release is authorised by this plan. Any later release milestone must also read LICENSING.md and resolve JUCE/ASIO/SoundTouch obligations, then run the final candidate's required gate and hardware/listening acceptance. Local technical checks do not establish distribution clearance.

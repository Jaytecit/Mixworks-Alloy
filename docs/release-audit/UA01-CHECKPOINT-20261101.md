# User-readiness audit: first execution checkpoint

1 November 2026. This begins milestone 1 of [the current plan](USER-READINESS-20261101.md).
It is a source/inventory and interaction-concept checkpoint, not a completed native
workflow audit. No whole rule has been accepted. No application source changed.

## Current evidence and triage

The former inventory refresh failed on `Source/EarToolTests.cpp`. Its classifier
also omitted the new ear/retry UI, original-file audio, visual scene and clarity
families. Explicit classifications now include these, including visual Python
runtime/verification tools. Unknown files still fail closed. Three new mandatory
rules MW-V01–V03 cover full visual planning, manual image/scene edits and output
lifecycle. Existing rules retain their identities and historical results.

[Current cases](inventory/current-cases.csv) is the current acceptance ledger:
97 rules plus UA01–UA09, with correctness and usability separately **not checked**.
Source findings do not become runtime reproductions. The old initial-status ledger
is historical; its three new visual rows are NOT RUN. Generated file/control/CLI
registries are discovery coverage, not expanded command or feature acceptance.

Priorities for the next functional checkpoint:

| Priority / case | Confirmed source finding | Missing evidence / action |
| --- | --- | --- |
| P1 UA07 | Engine.cpp:555–578 uses a global stem-solo mask; StemUI.cpp:167 clears other clips' solo state globally. | Reproduce with two isolated lanes, then scope UI, render and output consistently to the chosen lane. |
| P1 UA08 | TransitionUI.cpp:9/21/314 uses canInsertArrangement, which rejects playback. Existing preset edits use a different guard; focused transition navigation seeks. | Reproduce Add vs Apply vs Navigate separately. Do not remove every transport guard indiscriminately. Verify target, cursor continuity, Undo and output. |
| P1 UA06 | Model.h holds both lane and source-owned curves. New exclusive lane behaviour is a scope change, not a paint fix. | Agree concrete overlap/short/move outcomes; cover audio, performance capture, MIDI and export together. |
| P1 UA03 | Retained grid/placement failures and two crashes remain open. | Independent source/project/pixel/output oracles; no fresh detector truth or musical acceptance yet. |
| P2 UA04 | Workstation.cpp:1342 already adds sortable KEY CHECK; :2221 sorts key evidence. Dedicated GRID CHECK is absent there. | Preserve existing useful key review; distinguish detector evidence from user confirmation, then add separate grid review. |
| P2 UA01/02 | VisualArrangement.cpp:95 restricts to one family and :113 only penalises immediate repetition; :128 converts empty windows to cuts. | Exact 44-track failure remains unreproduced. Check full schedule, boundary treatments, reopen and later pixels separately from speed. |
| P2 UA05/09 | Current source workflow mapped below; alignment guide concept prepared. | Native gestures, actual appearance and first-use observations still outstanding. |

[Retained failures](inventory/retained-failures.csv) imports 227 assertion/error/
incomplete-process records from gate-09 and final-12. These include duplicates and
are **not 227 distinct defects**. Each keeps its original binary hash and report
hash; 25 match retained baseline evidence. Earlier reproduction does not establish
obsolete expectations or harmlessness. One record contains a proved obsolete
version conjunct: EarToolTests.cpp:81 demands 42, Model.h:19 declares 45, and
Model.cpp:1177 writes projectVersion. Its remaining pitch/roundtrip conjuncts are
unverified. No test assertion was changed and no failed report was discarded.
The other records remain unresolved pending fixture/path classification; this is
an initial triage, not completed classification of the broad failures.

## Core journey and proposed simpler route

This is a source trace, not a native walkthrough or newcomer usability result.

| User step | Current owners / routes | Preview direction / acceptance boundary |
| --- | --- | --- |
| Find music | Workstation library table/filter; LibraryWorkflows import/retry; LibraryPreview | Library remains the single source list; separate key/grid review beside metadata. No scan started. |
| Review analysis | GridCorrectionUI; KeyEarUI/HarmonyEarUI; RetryReview | Named source with separate detected/reviewed states. Source-save requires disk re-read. |
| Place and align | TimelineEditing; ArrangementInsertionUI; TimelineHeader | Selected lane/track scope visible; guide independent from transport. Source anchors are a separate concept. |
| Edit and fade | EditSelection; TimelineAutomation; AutomationUI; model curves | One visible chosen lane parameter; handles available immediately; local trim geometry stays identifiable. |
| Add transition | TransitionUI; PlaylistTransitions; TransitionStyles | One target pair and start shown; stopped/playing action consistent. Existing Add/Apply differences must be reconciled. |
| Add visuals | SceneLane; VisualScenes; VisualArrangement | Full schedule and deliberate handovers; no speed claim or invented audiovisual export. |
| Save/export | Model persistence; WavExportUI/WavExport; Playlist | Reopen exact saved state and independently inspect exported contents; no native acceptance in this checkpoint. |

## Editing concept and unresolved musical choices

The inline concept is preserved as `.local/ua01/alloy-editing-preview.html` with a
standalone wrapper at `.local/ua01/editing-preview.html`. It contains simulated
transport, lane/parameter selection, node editing, Undo, guide snap/free/hide,
lane-B stem-solo feedback and a visual crossfade placement. It has no audio engine,
real music, save/export integration or native controls. Node times stay fixed in
this concept; dragging edits their value. It is not a substitute for Alloy tests.

- Full tracks: start, start+8 bars, end−8 bars, end. Neutral Volume/EQ = 0 dB;
  Filter = Off. Plugin neutral values need each parameter's own default.
- Adjacent/overlapping tracks: coincident handles merge; both tracks read the same
  lane value in an overlap. The examples expose this consequence rather than
  presenting independent per-track control as a lane curve.
- Short tracks: inner handles meet at the midpoint; a 12-bar clip has three distinct
  nodes. This is a proposal for review, not a settled interpretation.
- Proposed continuation: split keeps the existing lane curve; move changes the
  audio's position under it; user-edited nodes survive delete/move; generated
  untouched handles can be reconciled without overwriting edits. Inserting a track
  into an already edited lane must inherit its existing curve, not reset to neutral.
  Gaps hold the curve; tempo changes preserve bar-relative handle intent. These
  edit/persistence rules are not implemented by this concept.
- A single shared volume curve cannot independently crossfade two overlapping
  tracks on the same lane. The concept shows a transition between A and B. Resolve
  same-lane transitions explicitly before changing automation ownership; do not
  silently remove an existing capability or add another unexplained envelope.

JavaScript syntax passed. **Visual/browser interaction verification is pending:**
browser security rejected the local `file:` URL. No alternate browser, local web
server or raw browser-command workaround was attempted. The concept is a draft
for direction, not a visually accepted preview. The final response exposes the
normal inline concept for Jay's observations.

## Identity, verification and continuation

Independent exact-byte before/completion archives, manifests, dirty diffs and
command outputs are in `.local/ua01/`. All 231 Source files remain unchanged and
match the retained build13 manifest; the prepared executable still hashes to
`c51561ef9f95c220324746f49ded6a9d54bda3a217dbf123977bca2965e28c95`.
No native app/build/test, GPU renderer, scan, benchmark or launcher activation ran.
Original music, user data and running applications were not controlled. No owned
application/browser/server job remains.

Use the retained check outputs for exact inventory/project totals. Relevant checks:
inventory regeneration/check, classifier/rule/ledger integrity, project document
links/CMake registration and preview JavaScript syntax. These are tooling/document
checks only. The current plan remains partly unchecked; no complete milestone or
release claim is made.

Next: inspect `Source/TransitionUI.cpp` lines 1–50 and
`Source/ArrangementInsertionUI.cpp` lines 1–14, then prepare one focused native
Add/Apply/navigation reproduction with two owned tracks and a moving transport.
Inspect the chosen test's complete call chain for timing/load work before running.
Do not run the whole SceneLane suite: SceneLaneTests.cpp:387 includes an explicit
minimum-FPS assertion. Coordinate physical input separately. Keep the 44-track
schedule test distinct from output rendering and benchmark work. Continue exact
failure/oracle classification alongside this bounded workflow.

Method discovery consulted [skills.sh](https://www.skills.sh/); the native repository
audit and source-to-output tracing remain the applicable method. No skill or tool
was installed and no subagent was used.

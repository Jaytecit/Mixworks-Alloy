> Dated audit/repair record. Baseline findings and pending labels below describe their original checkpoint. Use the [0.55.64 request register](REQUEST-STATUS-20261024.md) for reconciled delivery and the [active audit status](release-audit/STATUS.md) for current release qualification. Do not treat historical failures as current confirmed defects.

# Mixworks Alloy product audit — 21 October 2026

Method: dual-agent Impeccable assessment (A: `/root/design_critique`; B: `/root/technical_audit`), supplemented by primary-agent source tracing and operation of the native application.

**The first repair batch should fix command dispatch, correction targeting and project admission.** Choosing a transition length initiates Quit; the source-correction command can open a hidden, previously selected library song; and a missing library song that is not used in the arrangement can prevent the entire arrangement opening. The audit also found substantial native accessibility gaps and misleading clip/lane ownership labels.

This is an audit and remediation plan, **not a release acceptance or an exhaustive certification**. No application code, detector, build configuration or executable was changed. See [coverage and limitations](ALLOY_AUDIT_COVERAGE.md) and [implementation batches](ALLOY_REMEDIATION_PLAN.md). Evidence labels below distinguish runtime observations, source findings and subjective recommendations.

## Scope and provenance

- Inspected the user's running **0.55.12**, initially PID 188856, executable SHA-256 `f1646688edac36d0d0ac699d4eea899586b04859a96f7dc095d946cf35682565`. All 154 frozen Source/CMake inputs matched the accepted candidate. Analysis revision **9**, session format **27**.
- Began with the user's unaltered, generated **10-track / 38:55.496** arrangement. Saved a diagnostic baseline, then used disposable copies. Later experiments are identified separately; they must not be mistaken for the unaltered arrangement.
- Operated native controls using the installed Windows Computer Use `@oai/sky` runtime: playlist selection, Editing, gain, lock/Undo, detach/attach, playback, source review/audition, saving, process exit/reopen, missing media, F11 resizing, A/B rehearsal, master EQ/Undo, keyboard display, timeline fit/split/Undo and WAV export.
- Ran the existing production-component `fusion-workflows` and `export` suites through `verification.run_checks`: **38 + 69 = 107 checks, zero failures/errors**. These are fresh results on the exact executable, not a new full gate. The existing **2,608-check / 22-suite** candidate gate is historical evidence, explicitly distinguished from this run.
- Independent source checkpoint, reports, native UI Automation trees, screenshots, hashes and assessment files are in [the local evidence directory](../.local/product-audit-20261021/). [Identity](../.local/product-audit-20261021/identity.json), [fresh suite results](../.local/product-audit-20261021/focused-results.json) and [control-source index](../.local/product-audit-20261021/control-source-index.csv) support reproducibility independently of Git.

## Intended product and current boundaries

Alloy is a native Windows JUCE/C++ timeline mixer with four real audio lanes, SoundTouch stretching, local Beat This! analysis, source grid/key corrections, linked transitions, automation, a mouse/MIDI performance desk, VST3 hosting, master processing, project persistence/recovery and stereo WAV export. The current reliable-detection priority is constant-tempo **4/4**; a pulse estimate is not proof of a downbeat. Bar alignment requires a source downbeat anchor and correct project mapping.

The live desk shares the arrangement engine. Record captures **control automation**, not an audio take or MIDI-note performance. Clip effects/automation and lane instrument/effects paths coexist. Display/workspace preferences have a separate owner from musical session Undo. These distinctions explain several findings below.

Full range/ripple editing, variable-tempo source maps and other meters, stems, note recording, a built-in synth, themes, MP3/FLAC export, richer Auto Arrange explanations and the final user manual remain planned in FINISHING-PLAN.md. Their absence is not classified as a broken shipped control. Two explicitly disabled Spare pads are reserved placeholders, with explanatory tooltips. Nonzero-latency VSTs are deliberately blocked rather than silently played out of alignment; full delay compensation remains unfinished.

## Findings

Severity: **P1** significant core-workflow failure or major access/ownership problem; **P2** consequential confusion, inefficiency or bounded diagnostic concern. No P0 data-loss event was demonstrated. Confidence concerns the stated evidence, not an unproven root cause. Effort is relative: S roughly a focused change, M several related changes, L a shared architecture/accessibility effort.

There are **13 findings: 5 P1 and 8 P2**, including explicitly labelled recommendations and verification concerns; this is not a claim of 13 confirmed software bugs.

### AUD-001 — Section-length menu command initiates Quit

**P1 · runtime-confirmed and code-confirmed · high confidence · S**

**Surface/consequence:** Beatmatch > Transition length > Sections 8–64 bars invokes application closure. An unsaved mix gets an unexpected Save/Discard/Cancel prompt; a clean mix can exit. This is harmful duplicate command identity, not useful alternate access.

**Reproduction:** Open a mix, make an undoable edit so it is dirty, open Beatmatch and click Sections 8–64 bars. **Expected:** select a new-track transition default. **Actual:** the Unsaved mix close guard opens. Cancel safely retained the test state.

**Source:** `Source/Workstation.cpp:1324–1329` defines six lengths. `Source/WorkspaceUI.cpp:25` assigns IDs 40–45; line 74 handles 45 as Quit before length routing at line 105. File > Exit legitimately uses the same 45 at line 12.

**Evidence:** [menu before selection](../.local/product-audit-20261021/1792538164992-sections-command-before-0.png), [unintended close prompt](../.local/product-audit-20261021/1792538178410-sections-quit-modal-0.png).

**Correction/acceptance:** Give commands unique typed identities and share dispatch across routes. Exercise every length through the actual menu, including on clean/dirty mixes. No length command may request close, mutate musical placement immediately or launch another command. Exit must retain its unsaved guard. Suggested Impeccable follow-up: `harden`.

### AUD-002 — Source corrections can target a hidden previous library selection

**P1 · runtime-confirmed and code-confirmed · high confidence · S–M**

**Surface/consequence:** Beatmatch > Track BPM / key can edit a different song from the selected Playlist/timeline track. Applying an unnoticed correction could alter the wrong source's analysis/corrections. No correction was applied during this audit.

**Reproduction:** Select **90s Hammer** in Library; select **Blade Runnerz** on the timeline; switch to Playlist, where Blade Runnerz remains selected; invoke Beatmatch > Track BPM / key. **Expected:** Blade Runnerz, or an explicit target choice. **Actual:** Source Beat Grid / **90s Hammer** opens while Editing and the VST rack name Blade Runnerz.

**Source:** `Source/Workstation.cpp:1101–1105` keeps separate library/playlist identities; ordinary timeline selection at 848 does not replace `selectedLibraryId`; `editAnalysis` at 2045–2055 prefers `selectedLibraryId` regardless of the current browser page. `WorkspaceUI.cpp:121` routes command 25 to that resolver. Right-clicking a clip sets its library identity first (843), so routes can disagree. Ctrl+W shares the resolver in source; that shortcut's result was not independently established at runtime.

**Evidence:** [selected Blade Runnerz](../.local/product-audit-20261021/1792538085248-playlist-retains-timeline-selection-0.png), [wrong correction source](../.local/product-audit-20261021/1792538112120-grid-wrong-source-from-playlist-0.png).

**Correction/acceptance:** Pass an explicit source identity from context actions and resolve global commands from the active surface/selection. Test library → playlist, playlist → library, ordinary click, right-click and shortcut paths with two different songs; every route must show the intended title before commit. Retain one correction owner and existing source-wide semantics. `harden` / `clarify`.

### AUD-003 — Unused library dependencies gate an otherwise playable mix

**P1 · runtime-confirmed and code-confirmed · high confidence · M**

**Surface/consequence:** Saved projects embed the library, and opening requires all embedded entries, including unused ones, to decode successfully. Moving/removing an unrelated song can prevent editing/exporting a mix whose actual sources are available. It also puts whole-library checking on the critical loading path.

**Reproduction:** The saved ten-track mix contains **359 media entries**. A separate fixture has two valid, 12-second clips from one available source plus one additional media entry referenced by **zero** clips, pointing to a deliberately nonexistent file. Launch that session. **Expected:** open the playable arrangement, reporting the unrelated library entry as unavailable. **Actual:** opening is rejected: “Cannot decode: never-existed-unused-audio.mp3. Try WAV, AIFF, FLAC, Ogg or MP3. The current session was kept.” The blank launching instance remains responsive. No real media was moved/deleted.

**Source:** `Source/Engine.cpp:655–676` computes used media but loops over every parsed media reference and returns on any failed `readMedia`. `Workstation.cpp:2148–2160` commits transactionally and merges previous library entries back into the loaded library. Thus availability of unrelated catalogue material controls project admission.

**Evidence:** [unused-missing failure](../.local/product-audit-20261021/1792538631999-missing-unused-blocks-entire-mix-0.png), generator/provenance in the evidence directory. On the real ten-track reopen, [238/359 entries at 0:32 elapsed](../.local/product-audit-20261021/1792538361308-reopen-progress-or-complete-0.png) preceded a usable mix. This is one observed run, not a repeatable warm-load benchmark; no new claim about detector throughput follows.

**Correction/acceptance:** Admit used arrangement sources first; hydrate/validate unused catalogue entries separately and preserve their unavailable metadata. Distinguish missing, unsupported and corrupt files; offer a useful path/relink route for required missing sources. An unavailable unused song must not block project admission. Cancellation or required-source failure must keep the current project. Preserve analysis revision 9, corrections and compatible caches. `harden` / `optimize`.

### AUD-004 — Silent callback skips occurred during real playback; cause unresolved

**P2 diagnostic finding · runtime-confirmed counter change · high confidence in observation, low in cause · M investigation**

**Reproduction/state:** Windows Audio, Realtek speakers, 48 kHz, **480-sample / 10 ms** blocks; ten-track arrangement, no inserted VSTs, first song playing, generated attract/forge appearance active. Audio/MIDI diagnostics advanced from **66,931 callbacks / 124 lock skips** to **67,940 / 125**, with zero over-budget/size/export-silence counts, worst callback 2.43 ms, driver xruns unavailable. A separate native export suite was using CPU concurrently. Earlier accumulated skips include stopped time and cannot be attributed to playback.

**Expected/actual:** Playback should not lose a callback to avoidable shared ownership. At least one lock-skip increment occurred within this playback interval. `Engine.cpp:489–503` clears output then returns if `ScopedTryLock` fails, so the skipped block is silent by implementation. No physical listening, loopback capture or causal attribution to visual effects, settings, hardware or the concurrent suite was established.

**Evidence:** [diagnostics before](../.local/product-audit-20261021/1792537989853-audio-midi-diagnostics-0.png), [later counter tree](../.local/product-audit-20261021/1792537999138-diagnostics-later.json).

**Correction/acceptance:** Reproduce a defined passage with isolated load and counter deltas, then identify the contending lock holder; compare visual policies without changing audio conditions. Retain the nonblocking callback, but shorten/move non-audio critical sections. Acceptance requires measured silence-skip/over-budget deltas plus a captured/listened output, not a faster screenshot or zero historical test failures. `optimize`.

### AUD-005 — Lane-labelled controls mix clip and lane ownership

**P1 usability/functional consistency · code-confirmed; runtime labels inspected · high confidence · M**

**State/problem:** With a clip selected, lane-strip volume/pan and desk EQ/level/pan can target that clip. Cutoff, mute and solo are lane-owned. The strip volume tooltip nevertheless says “Lane volume”; pan says “Position this lane.” The desk's generic `/ CLIP` suffix does not identify each pot/fader's scope or the selected song. Selecting an ended song on a reused lane can make a purported live volume/EQ move affect silent material.

**Source:** `Engine.cpp:113–122` (`controlClip`, `controlSettings`, `pluginChain`); `Automation.cpp:134–144` resolves non-cutoff empty addresses to the selected clip. `Workstation.cpp:1167–1177` mixes these owners in one strip. `PerformanceDeck.cpp:141–170` follows the selected-clip exception for all but cutoff. The current cutoff fix is present; this finding does not allege that it regressed.

**Expected/actual:** Scope should be visible and consistent with the operation promised. In the inspected state the lane strip carries a song name while the common lane controls have different owners. No fresh end-to-end loudness comparison for the old-selected/later-playing volume/EQ case was performed; source routing establishes the mismatch, not a listening test.

**Correction/acceptance:** Display `Clip • song` versus `Lane • all songs` at the affected control group, correct tooltips and explain the selected-clip override. Decide whether live desk controls should consistently stay lane-owned before changing routing. Acceptance must cover two songs reusing a lane, with an ended clip selected, mouse/MIDI parity, Read/Touch, Undo and save/reopen. `clarify` / `harden`.

### AUD-006 — Primary musical content and status lack native accessibility semantics

**P1 · runtime-confirmed UIA gaps and code-confirmed · high confidence · L**

**Reproduction:** Read the native accessibility tree while browsing Library/Playlist, editing the timeline and receiving footer feedback. **Expected:** meaningful titles, metadata, selected objects, values/actions and outcome/dirty announcements. **Actual:** rows expose generic “Row 1” names and unnamed painted cells; descriptions sometimes expose file paths, but not complete comparable column data. Clips, transition frames, source waveform objects and automation nodes are largely painted custom surfaces rather than accessible child objects. General footer status/UNSAVED is absent from the tree.

**Source:** `Workstation.cpp:981–1105` paints table content; `PlaylistTransitions.cpp:6–8` returns no cell component except the playlist transition picker; pinned JUCE `juce_TableListBox.cpp:129–134` supplies anonymous placeholders. `Timeline` and `TimelineHeader` paint musical objects without a custom object accessibility model. `Workstation.cpp:1538` repaints a status string; `WorkspaceUI.cpp:206–207` paints it and UNSAVED. Do not prescribe a nonexistent JUCE `getCellText` override.

**Evidence:** [initial UIA tree](../.local/product-audit-20261021/001-initial-ten-track.json), later explicit window captures and [Assessment B](../.local/product-audit-20261021/assessment-b.md). Standard buttons/dials, resizers, modal text and many tooltips already expose useful roles/help; those strengths should remain.

**Correction/acceptance:** Provide accessible cells/row metadata and a navigable timeline object model using JUCE/Windows UIA; expose selected scope, point values, dirty state and rate-limited action feedback. Complete import → selection → basic edit → save/export using keyboard plus a Windows screen reader. Tree inspection alone is not screen-reader acceptance. `harden` / `adapt`.

### AUD-007 — Save always behaves like Save As

**P2 · runtime-confirmed and code-confirmed · high confidence · S**

**Reproduction:** Save a mix, edit it, press the visible Save button again. **Expected:** save to its known destination, with a separate Save As route. **Actual:** the picker opens again; choosing the same file adds another overwrite decision. The current document filename is not persistently identified in the main title/header. Ctrl+S routes identically in source.

**Source:** `Workstation.cpp:1364,2097–2111,2309`; `chooseSave` defaults `useSessionFile=false`. The unsaved close guard uses `true`, so saving on close already has a different, more direct path.

**Evidence:** [repeat Save picker](../.local/product-audit-20261021/1792538203743-save-reopens-picker-for-named-session-0.png). Both initial and edited copies saved successfully, and the edited copy reopened with ten entries and −6 dB retained.

**Correction/acceptance:** Use the current file for Save/Ctrl+S and expose Save As separately, sharing one atomic save owner. Keep first-save/cancel/failure semantics and backups. Show current filename and dirty state. `clarify` / `harden`.

### AUD-008 — Generated treatments are described as “Custom / edited”

**P2 design recommendation · runtime-confirmed presentation and code-confirmed classifier · high confidence · S–M**

**Reproduction:** View Playlist before modifying the supplied generated ten-track arrangement. **Expected:** users can inspect what the generator did without inferring personal edits. **Actual:** all nine incoming transitions read “Custom / edited.” The panel reveals individual band/filter settings, but offers no concise generated-treatment explanation. This does not prove their audio is wrong.

**Source:** `PlaylistTransitions.cpp:25–41` recognises only limited preset/style matches, otherwise supplies that label; `AutoArrangement.cpp:42–85` generates compound treatments. Existing richer rationale work is planned under P22.

**Evidence:** [unaltered generated Playlist](../.local/product-audit-20261021/1792537802192-playlist-0.png), [selected treatment](../.local/product-audit-20261021/1792537831161-transitions-selected-0.png).

**Correction/acceptance:** Start with neutral “Custom treatment” plus a summary of the actual current bands/filter/length. Add generated provenance only if it is retained truthfully through edits, Undo and persistence. Do not label a result “edited” solely because it is not a built-in preset. `clarify`.

### AUD-009 — Compact browsing hides core mixing metadata

**P2 design recommendation · runtime-confirmed layout · high confidence · M**

At 1920×1080 with the existing ~305-pixel sidebar, Library shows ART, a narrow title, key and a large confidence column; **BPM is offscreen**. Playlist similarly privileges a 130-pixel incoming-treatment picker while titles and timing are truncated. A user can resize/scroll/hover, so this is inefficient comparison rather than missing data.

**Source/state:** `Workstation.cpp:983–992` fixes initial column widths/order and disables stretch-to-fit; `WorkspaceUI.cpp:271–277` supplies the sidebar area. Reproduce by returning to the inspected sidebar widths. **Expected:** title and BPM visible together in an everyday mixing layout. **Actual:** both title identification and tempo comparison require extra interaction.

**Correction/acceptance:** Preserve resizable columns, but prioritise title/BPM/key in a compact row and move detailed confidence/time to secondary information. Check Library and Playlist at compact and wide widths without relying on hover. [Workspace evidence](../.local/product-audit-20261021/001-initial-ten-track-0.png). `layout` / `typeset`.

### AUD-010 — Auto Arrange forces animation policy

**P2 optional policy recommendation · code-confirmed · high confidence · S**

**Source/reproduction:** Configure Follow Windows or Steady, then finish Auto Arrange. `AutoArrangementUI.cpp:156–159` sets attract motion Animated, enables attract/forge and selects Animated forge motion through command 83; `WorkspaceUI.cpp:92–95` persists forge motion. **Expected recommendation:** preserve an explicit user's motion policy while still celebrating completion. **Actual implementation:** it selects animation explicitly.

This full automatic lighting activation was **previously requested and documented**, so it is not presented as an accidental regression or unauthorised style change. Revising that policy is an optional product decision. OS reduced-motion acceptance was not run; do not claim an axe or WCAG failure from this source path.

**Acceptance if changed:** preserve Follow Windows/Steady across generation; keep the requested completion effect within that policy; Escape still exits attract mode. `animate` / `adapt`.

### AUD-011 — Detailed help exists but has no obvious app entry; version context drifts

**P2 design recommendation / code-confirmed documentation mismatch · high confidence · S**

**Reproduction:** Look for a guide/shortcut list from the six application menus. **Expected recommendation:** a discoverable entry into task guidance and an identifiable app version. **Actual:** File/Edit/Mix/Beatmatch/View/Tools contain settings/hints but no Help/guide route (`WorkspaceUI.cpp:6–61`). Popup hints help individual controls, not a full mix journey. `docs/README.md:3` still identifies 0.55.8 while the accepted executable/README is 0.55.12; DESIGN.md also retains historical eight-lane and earlier placement descriptions.

**Consequence:** Users must find repository documents externally and distinguish current guidance from history. **Correction/acceptance:** add a small Help/About/shortcuts entry into existing task docs, show build-derived version (already planned P49), and distinguish current design guidance from dated history. Do not accelerate the final full manual ahead of accepted functionality. `clarify` / `onboard`.

### AUD-012 — The suffix-move handle does not communicate its large edit scope

**P2 design recommendation · visual/source evidence · medium-high confidence · S–M**

The small header hexagon with a horizontal arrow can move the selected box **and following playlist tracks**, pinning the incoming transition; Shift changes it to audio-only slip. Its glyph does not communicate this suffix ownership, and the controls are small at whole-mix zoom. See `AudioSlipUI.cpp` and the move/slip documentation. The independently approved movement semantics are not being rejected.

**Reproduction/expected:** inspect the ten-track whole-mix view and identify the result before dragging. **Actual:** understanding the scope depends on tooltip/documentation and modifier recall. [Initial arrangement](../.local/product-audit-20261021/001-initial-ten-track-0.png).

**Correction/acceptance:** retain the handle and fast gesture; add an explicit hover/focus scope preview or action caption, including affected following tracks and Shift mode. Provide a discoverable keyboard/menu equivalent if absent. Validate earliest/middle/last clips and locked/pending rejection without moving earlier audio. `clarify` / `polish`.

### AUD-013 — Initial active MIDI state disagreed with startup persistence

**P2 verification finding · runtime-confirmed mismatch; suspected cause · high confidence in state, low in attribution · S diagnosis**

The initial app showed **FLKEY 49 1 / 50 active mappings**, while the startup `midi-mappings.json` was already **4 bytes / `[]`**. Its last-write timestamp was **20 October, 19:13:36**, before this audit and before its fresh suites. A normal reopened instance correctly read that empty snapshot and showed **Default / no mappings / 0**. Named profile files remain available. The audit did not clear/reload/import/open/save controller mappings or reprogram the FLkey.

**Evidence:** initial [Audio/MIDI profile state](../.local/product-audit-20261021/1792537989853-audio-midi-diagnostics-0.png), [observed snapshot](../.local/product-audit-20261021/startup-mappings-observed.json), [metadata](../.local/product-audit-20261021/startup-mappings-metadata.json), [reopened indicator](../.local/product-audit-20261021/1792538841018-keyboard-and-inserts-0.png). `Engine.cpp:59–64` loads the startup snapshot; `MidiConnection.cpp:92–118` implements profile admission. The evidence does not identify the writer that previously emptied it, or establish a current profile-save bug.

**Expected/correction:** make active-versus-startup mismatch understandable and establish which saved profile the user wants before restoring anything. Diagnose on owned copies: Open profile → restart, Learn autosave → restart, Clear → restart and failure paths must match stated semantics. Preserve named copies and backups. No automatic restoration/cleanup is recommended based only on this observation. `harden` / `clarify`.

## Musical outcomes and useful strengths

- Ten entries and a selected clip's −6 dB input-gain edit survived save, process exit and reopen. Comparison of the two saved documents showed only that clip's `gainDb` changed; arrangement positions, tempo, transitions and other fields matched.
- Transition Lock disabled affected timing/band/cutoff controls; Undo restored editability. Editing detached into the same native controls with −6 dB intact, and native close reattached it.
- Playback advanced with real output-meter activity. Source audition navigated to the source middle and advanced independently; Cancel returned without committing corrections.
- Store A → Hear A prepared a production rehearsal and started comparison playback; End restored the previous cursor paused. The rehearsal showed a **+0.8 dB right-channel peak/clipping warning** in the disposable copy with Blade Runnerz input gain changed to −6 dB. This is an observed warning, **not** a measurement of the unaltered original mix or proof of audible clipping. No long musical listening judgment follows.
- The 20-second real-source-window fixture exported **960,000 frames, stereo, 48 kHz, 24-bit PCM**, peak 0.719386, RMS 0.205212, zero full-scale clipped samples. [Measured file result](../.local/product-audit-20261021/export-measurement.json) confirms the file, not just a toast. Its two overlapping windows are a disposable test arrangement, not the original auto mix.
- Master low-shelf +3 dB followed by Undo reopened at 0 dB. Split inside the first short segment produced two contiguous pieces; Undo restored the original two-clip project. These UI observations do not independently prove sample-identical split audio; the prior native gate supplies separate production PCM coverage.
- Existing workers, transactional candidates, lock validation, source protections, progress/Cancel, private export renderers and backup/temporary commits are strong foundations. The fresh export suite exercised actual test VST3 tails, cancellation, retry and close/admission ownership, not a mocked success notification.

## Impeccable critique and native audit

The graphite/copper/hexagonal instrument identity is specific and coherent. Preserve it. The opportunity is clearer musical scope, readable comparison information and trustworthy outcomes, rather than a new visual theme. Expert hardware density can be appropriate; a mechanical four-choice limit is unsuitable for a mixer. Current attract-lit screenshots are not the normal unlit baseline.

Independent Assessment A scored Nielsen heuristics **25/40**: status 3; real-world match 2; control/freedom 3; consistency 2; error prevention 3; recognition 2; efficiency 3; minimalism 2; recovery 3; help 2. These are design judgments on its bounded evidence, not functional pass rates. Expert risks: hidden BPM and repeated saving; newcomer risks: clip/lane ownership and Record meaning; access risks: painted musical objects, tiny labels and hover dependence. The emotional weak point is losing confidence in what an operation will change or preserve.

Assessment B's adapted Windows technical score was **11/20**: accessibility 1, performance 2, theming 3, platform conformance 2, adaptivity 3. Windows UIA/resizing/motion replaced the native reference's iOS/Android-specific criteria; mobile gestures/safe areas were not imported as requirements. Performance and DPI/hardware bounds remain explicit.

The installed Impeccable context loader ran once. No PRODUCT.md existed, but scoped audit could proceed using README, HANDOVER and existing DESIGN.md. Its detector attempt produced `[]`, exit 0: **C++/JUCE is unsupported by that markup scan; this is not a clean quality result.** A and B did not see each other's assessment; B sent interim messages to the parent before A finished, so strict parent-level detector blinding was not maintained. The independent A assessment itself remained unanchored by B. No duplicate review was run.

Browser visibility/injection/overlays/live server: not applicable to `JUCE_WEB_BROWSER=0`. Playwright, axe-core and Knip would not inspect this native application; none was installed as a substitute. The required [skills.sh directory](https://skills.sh/) was consulted; it did not establish a better compatible method than the installed native tools. Recommended Impeccable commands are follow-up categories, not changes already made. Questions skipped: user requested an autonomous audit and concrete handoff.

## Boundaries that remain

No fresh physical FLkey mapping/reconnect, ASIO acceptance, multi-monitor unplug, Windows DPI/text/high-contrast setting changes, screen-reader journey, real third-party instrument performance, disk-full/unwritable destination test, forced-crash recovery, 60/90-minute listening, independently annotated detector/downbeat accuracy, or complete exported musical listening review was completed. Existing evidence covers portions of these; it does not convert them to runtime passes here. Text injection/chord automation was unreliable in some JUCE editors, and F11 recreated the native peer; targeting was refreshed rather than treating helper failures as app bugs. Early saved modal captures 002–004 reused the initial workspace state and are excluded from modal evidence; later explicit-state captures are used above.

No original music, production analysis/corrections, cached audio, library snapshots or MIDI profiles were reset. Normal app saves/closes and workspace inspection can update their own owned preferences/recovery/catalogue state; this report does not claim an immutable AppData tree. Alloy was allowed to close under the user's current instruction, superseding earlier leave-running requests. No public package, commit, installation or licence acceptance occurred.

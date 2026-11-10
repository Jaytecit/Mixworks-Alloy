> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Guided pulse, downbeat and grid correction — ua11

**Update:** [ua13](UA13-FOCUSED-VERIFICATION-20261101.md) successfully builds this
implementation and passes61 focused alignment checks; all five grid captures
were inspected. The memory-stop/pending history below is superseded for this
scope. Physical/listening/novice acceptance and full readiness remain open.

Jay requested a method ordinary users can understand, with advanced tools in an
Advanced tab. The priority is constant-tempo4/4. This work improves manual repair;
it does not claim improved automatic detector accuracy.

## Method

Separate the three questions: Are the beats the right distance apart (tempo)?
Do the clicks land on the music (pulse position)? Which of those beats is ONE
(bar/downbeat phase)? A known ONE anywhere in a track establishes both pulse and
bar phase. A second correctly counted later ONE measures precise tempo. Check
several passages after aligning them; two matching peaks alone prove neither
the count nor that the musical accent is beat one.

The editor now starts with three guided pages: Tempo, Beat one, Check drift.
Only the controls for the selected task are shown. A shared waveform, original
source transport, click/audio monitor, level, zoom, passage navigation, Undo/Redo
and save actions stay accessible. Advanced keeps exact seconds, A/B modes, fine
BPM changes, manual keys, retry results and diagnostics. Page changes preserve
the same draft and create no Undo. The graphical identity remains graphite/copper.

Beat 1 back/forward shifts the downbeat by exactly one beat while preserving
spacing and pulse phase. Bounds reject a shift outside the source rather than
clamping a partial beat. Existing nudge, two-anchor calculation, drag history,
source save and captured timeline-clock behavior are reused.

Save for this track keeps the current mix unchanged. Save + rebuild mix explicitly
confirms its existing consequence: rebuilding Beatmatch for the whole mix can
move tracks and replace manual transitions. Declining keeps the editor draft.
Undo restores the mix, not durable source corrections. The underlying save and
rebuild policy has not been redesigned into a selected-track-only operation.

## Verification record

Before/candidate source checkpoints and the guarded serial build are retained
under `.local/ua11/`. The bounded `--grid-alignment-only` checks now cover the
guided pages, hidden advanced controls, unchanged draft on navigation, independent
128BPM/two-anchor oracle, bar-accent phase invariance, Undo and confirmation
cancel/accept, plus existing save/reopen and failure preservation. Planned native
captures show all three guided pages and the Advanced editor.

The single-worker, below-normal-priority build was stopped by its owned-process
guard when available memory fell to1497MiB (1500MiB stop threshold). It did not
complete; no native checks or new screenshots ran. Do not use the pre-existing
build executable as this candidate. After the stop, static review added clearer
empty-tempo and missing/out-of-range check-beat guidance and reduced the popup to
980x710; these completion-source edits also need compilation. Whitespace/source
checks pass, which does not establish runtime or visual acceptance. No retry was
attempted under the same resource conditions. No owned jobs remain.

The
existing ua09 destination and ua10 label changes are also present in this source
tree and retain their separate verification obligations. No automatic scan,
cache reset, production restart, detector run, benchmark or full gate is part of
this checkpoint.

Resume when memory allows: one guarded build from completion source, then the
bounded grid-alignment, review-sorting, destination-safety and save-export-workflow
selectors serially through `verification.run_checks`; inspect their captures.
The normal launcher remains ua08 `9770639eb18c`. This is an implementation awaiting
verification, not a preview ready for judgement. Physical gestures, musical
listening and novice success remain to be demonstrated after native acceptance.

## Automatic-analysis next step

Do not equate clearer controls with more accurate detection. Separate BPM,
pulse-offset and bar-phase errors on a small independently reviewed constant-4/4
set before modifying detection. First resolve the memory-limited analysis path
from ua06 so a one-source evaluation can complete safely. Compare source facts,
source-to-project conversion and actual clicks at multiple positions; do not move
waveform graphics to hide a timing error. A later rescan requires demonstrated
improvement and coordination with Jay. No ground truth or improvement is claimed
here, and variable-tempo mapping remains outside the current scope.

Method research consulted [skills.sh](https://skills.sh/) and the official
[Rekordbox manual](https://cdn.rekordbox.com/files/20210301182245/rekordbox6.5.1_manual_EN.pdf),
whose grid controls separate BPM, position and first-beat adjustment. The
Impeccable distill workflow informed progressive disclosure using existing
native controls. The prior native grid-editor capture was inspected against
current source; it confirmed the dense all-controls-at-once presentation.

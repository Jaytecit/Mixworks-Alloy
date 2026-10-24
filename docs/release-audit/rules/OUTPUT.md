# Intelligent arrangement, rehearsal and output

Inherit RULES; F-ARRANGE/F-AUDIO/F-PLUGIN; scopes arrange/export, Model/Beatmatch/Engine and data. All mandatory, NOT RUN.

### MW-O01 — Eligibility and constraints determine safe plans
- Contract/source: AutoArrangement eligibility/options, Model keyConfirmed and source grid, max3h plan horizon.
- Cases/procedure: zero/one/enough library tracks, unknown/manual/low-confidence beat/key, missing sources, insufficient duration, count/duration endpoints, starting ID absent, all seven styles/four energy policies, genre soft preference; cancelled planning.
- Pass/oracle: only eligible sources selected, no repeat unless explicit contract, bounds/time/links/phase valid, finite plan and clear stop reason for insufficient supply; independently enumerate small libraries≤6 tracks to verify feasible/invalid constraint decisions. No hidden mutation on failure.
- Evidence/mode/severity: eligible-ID/constraint oracle, seed/options/plan+reason and transactional state; automated; P1.

### MW-O02 — Advertised arrangement policies affect output for the stated reason
- Contract/source: AutoArrangement energy/genre/styles and docs/AUTO-ARRANGEMENT.md.
- Cases/procedure: controlled contrasting BPM/key/energy/activity/genre library, seeds held constant across options, ties/permutations/missing profile, very short songs; seven styles and all energy policies, current style and varied deterministic seed.
- Pass/oracle: options reach scoring/transition construction and change a deliberately distinguishing fixture as intended; genre stays soft, musical activity not confused with RMS. Reproducibility required only for same seed/source/config, UI random choice need not repeat. Independent ranking/constraint expectations and logged decisions explain result.
- Evidence/mode/severity: options→decision→plan/PCM comparison table and seed, source feature truth; automated/static; P1 for inert advertised settings, otherwise P2.

### MW-O03 — Section-based alignment respects measured changes and frame rules
- Contract/source: EnergySections/Beatmatch/AutoArrangement and latest HANDOVER; fixed older8–64 default is not a universal explicit-frame limit.
- Cases/procedure: paired section candidates, outgoing-only/incoming-only/no hints, short/late incompatible source, two/four-bar changes, preferred overlap exceeded by valid match, bass swap common change and later preset application.
- Pass/oracle: measured source changes map to a shared musical boundary, whole-bar default frame and bass swap use intended common point; explicit frame geometry T07 remains independent and may exceed64bars. Fallback/rejection explained with safe alternative; no truncation of full source merely to satisfy old cap. Independent authored changes and beat formulas decide validity.
- Evidence/mode/severity: source change→placement→frame/swap table, PCM/visual and listening; automated + listening; P1.

### MW-O04 — Replace/extend and ordered-timeline arrange commit atomically
- Contract/source: AutoArrangementUI and prepareOrderedTimeline; Preserve current mix/extend choices, protections and current ordinary-independent insertion contract.
- Cases/procedure: replace/extend current mix with manual nodes/links/locks/labels/tempo, existing end no eligible continuation, pending source hydration/cancel/stale project; ordered timeline arrange versus Library selection.
- Pass/oracle: preview/options name actual scope, cancellation/error exact state/Undo preservation; success one Undo and intended existing-prefix/manual data preserved where promised. Ordered explicit arranger links/transforms only its named scope; ordinary add remains independent. Unsupported preservation rejects clearly, not silently rebuilds.
- Evidence/mode/severity: pre/plan/post semantic/ownership/PCM diffs and Undo/reopen, actual native dialog handlers; automated + manual; P1.

### MW-O05 — Rehearsal and presets are independent and musically reviewable
- Contract/source: TransitionRehearsal/TransitionStyles/RehearsalContext; Arrange matched-RMS A/B/C versus private independent renderer.
- Cases/procedure: store/prepare/audition/apply A/B/C, lead-in/out extremes, named preset save/import/export/delete, malformed/cancel, locked/stale transition, plugin latency and missing source; compare route/level and Undo/reopen.
- Pass/oracle: preparation/audition/browse do not edit live musical/plugin/meter state, Apply changes named transition exactly once, Arrange comparison normalization follows matched-RMS contract, private uses its declared renderer. Listening rubric verifies continuity, phrase handover and unintended bass/level clash; tests do not decide musical taste.
- Evidence/mode/severity: state/PCM/preset identities, normalization measurements and listening notes/actual UI; automated + manual/listening; P1.

### MW-X01 — WAV plan and decoded contents match the selected ending
- Contract/source: WavExport plan/write; current integer output rate, stereo PCM24, last-clip versus chosen bounded tail, RF64 support.
- Cases/procedure: empty/nonzero playbackStart, fractional end, one-frame/odd final block, rates44.1/48/96k, tail0/positive/max legal and take elapsed duration. Independently decode complete produced file; header inspection alone insufficient.
- Pass/oracle: exact frame count `ceil((end-start+tail)*rate-1e-6)` justified by half-open frame coverage; independently compute start/end from authored fixture rather than plan function. Rate/channels/PCM24 encoding/data size correct, first/last samples and duration match. RF64 capacity separate R08; no MP3/FLAC export promise.
- Evidence/mode/severity: independent decoder/version and frame/encoding/sample measurements, options/plan/report hashes; automated; P1.

### MW-X02 — Live, offline and take output agree only where contracted
- Contract/source: shared Engine renderer, WavExport clone, monitor-only controls, take replay and finite effects.
- Cases/procedure: deterministic gain/delay/plugin/automation/tempo/trim/transition/stem fixtures, same prepared clock/rate and continuous start history, last-clip/include-tail, master processing. Compare device-callback programme before mute, offline decode, take supported replay and live master WAV.
- Pass/oracle: same-history deterministic float PCM max error≤2e-6; PCM24 adds≤1 signed quantization step, exact frames/channels. Private/monitor mute excluded from file, live instrument input absent from arrangement export as documented. Nondeterministic plugins/history differences use predeclared spectral/listening criteria REQ-03, not unsuitable byte-null demand.
- Evidence/mode/severity: aligned independent null report with max/RMS/location, stream/clock/config identities and declared differences; automated + listening; P1.

### MW-X03 — Output failure/cancel never destroys the existing destination
- Contract/source: validateWriteDestination, WavExport temporary/backup/interprocess lock and WavExportUI.
- Cases/procedure: early/mid/final cancellation, source/hardlink alias, existing destination+backup, locked/unwritable/disk-full/flush failure, nonfinite render, plugin latency changing before commit and simultaneous owned exports.
- Pass/oracle: existing destination exactly retained unless successful atomic replacement, additive backup on success, temporary partial file not reported complete and cleaned by ownership policy; output error/cancel truthful, valid retry works, no leaked renderer/thread/callback. Live master WAV partial-preservation contract P11 is deliberately different.
- Evidence/mode/severity: before/after output/backup/source hashes, injected stage logs and final independent decode/retry; automated + UI; P0 data damage, otherwise P1.

### MW-X04 — Export remains cancellable and independent of programme processor state
- Contract/source: createExportRenderer clone/admission, Workstation ExportTask and message-thread native plugin constraints.
- Cases/procedure: slow fixture processor, progress/cancel/window close, exclusive edit/device admission, clone failure/cancel and final native latency query pending; before/after plugin state and meters.
- Pass/oracle: UI can process progress/cancel, operation doesn't hold programme lock for whole render, cancelled final admission disarmed before worker destruction. Programme snapshots/native plugin state aren't altered by offline rendering; no successful export after cancel request accepted before commit. Responsiveness absolute bound awaits REQ-05.
- Evidence/mode/severity: event-loop/progress timeline, lock/thread ownership review and clone/state hashes; static + automated/native UI; P1.

### MW-X05 — Playlist/tracklist/label exchange retains paths and musical order
- Contract/source: Playlist parse/text/Engine exportPlaylist, M3U/M3U8 import and text export menus.
- Cases/procedure: UTF8/BOM/Unicode/spaces/relative supported entries, missing/duplicate/non-audio/nested playlist, reordered clips/labels, empty list, cancel/permission/conflicting destination; import then save/reopen.
- Pass/oracle: independent expected source/path/order list matches actual import/output; nested playlists rejected, missing sources explained without silent success or unintended recursion. Text timings/labels use project clock and documented rounding, files don't overwrite source audio/session. No unsupported playlist format silently claimed.
- Evidence/mode/severity: authored input/expected lists and raw output bytes, parsed results/state and real picker; automated/manual; P2.

### MW-X06 — Musical listening acceptance covers actual output
- Contract/source: intended DJ mixing use; limitations in rhythm/stem/rehearsal docs and current handover.
- Cases/procedure: F-CORPUS J01/J04/J06/J08 output on declared listening chain; matched level source/reference versus mix at joins, cue starts, pauses, EQ/swap/filter, stretched transients, tails, stem sum, begin/end and full representative set. Blind/alternating order where feasible; capture exact listening intervals.
- Pass/oracle: agreed REQ-03 rubric: no unintended dropout/click/channel swap/obvious beat flam, no unreported destructive truncation and transitions implement selected intent; subjective taste observations separated from reproducible defects. Absence of true stems or reference mix disclosed. Without listening evidence this rule cannot PASS numerically.
- Evidence/mode/severity: candidate/output hashes, listener/device/level/rubric, intervals/observations/reproductions and unresolved judgments; listening/manual; P1/P2 by defect.

# Musical time, editing and transition ownership

Inherit RULES; fixtures F-TIME/F-AUDIO; source group time plus Model/Engine/ElasticAudio and downstream export/performance. All mandatory, NOT RUN. Mathematical checks use the independent analytic oracle in RUNBOOK, not TempoMap output as expected truth.

### MW-T01 — Tempo map is monotone and invertible
- Contract/source: TempoMap in Model.*, constant/ramped project BPM; current workflow supports project ramps despite constant-source detection scope.
- Cases/procedure: constant map, ascending/descending ramps, adjacent/equal-time markers, all legal BPM endpoints, negative pre-roll/zero/exact marker/late horizon; 10,000 deterministic time/beat points plus repeated edit/Undo. Reject nonfinite/illegal map input before commit.
- Pass/oracle: beats integrate the specified BPM function, time is its monotone inverse, round-trip error ≤1e-7 seconds for ordinary 0–3h fixtures and ≤0.5 output sample at extreme legal points; no NaN/negative time reversal. Analytic integration/root solve sets expected values.
- Evidence/mode/severity: table of worst error/location and inputs, native tempo checks plus independent oracle comparison; automated; P1.

### MW-T02 — Source samples, display grid and rendered attacks share a clock
- Contract/source: Clip source/time methods, TimelinePlacement/TempoStrip, Beatmatch and Engine::render.
- Cases/procedure: tagged impulses with known source offset/downbeat late in file; trimmed/slipped/reversed/looped/stretched clips, ramp regions, zoom/scroll extremes, negative-grid extension; inspect start/middle/end and bridge timing versus independent audio_probe/decoded output.
- Pass/oracle: nonstretched impulse placement ≤1 sample from independent transform; stretched attack error separately meets N02/T03 criteria; ruler/display ≤1 physical pixel rounding from the independently computed coordinate. Bar phase is modulo4 from any reliable anchor, not first kick. Probe mapping alone cannot pass.
- Evidence/mode/severity: source sample table, captures/coordinate calculations and independently located output attacks; automated + visual; P1.

### MW-T03 — Elastic preparation and seek preserve attack timing
- Contract/source: ElasticAudio, Beatmatch/TempoTests; existing attack bounds 25ms moved/seek, 50ms ramp fixture are technical regression limits, not a new accuracy promise.
- Cases/procedure: F-TIME pulse/tone through constant stretch both directions, ramp, repeated seeks, cache seams, overlapping voices and odd blocks; cold/warm/rebuild, cancelled preparation then retry.
- Pass/oracle: attacks stay within the declared fixture-specific 25/50ms ceilings against authored sample events, no accumulated end drift or audible seams; worst error reported. Exact tolerances require REQ-02 musical acceptance review; do not silently treat 50ms as general acceptable DJ alignment. No current-output-derived expected attacks.
- Evidence/mode/severity: independent onset/phase and seam-RMS reports, preparation identity plus listening; automated + listening, provisional threshold block where general claim unresolved; P1.

### MW-T04 — Ordinary placement never invents a transition
- Contract/source: latest independent-timeline contract in HANDOVER, ArrangementInsertion/TimelinePlacement/Transitions; historical automatic-link defaults are superseded.
- Cases/procedure: Library/Playlist add, drop at all lanes/times, before/after insertion, move, split, trim and duplicate; touch/overlap/no-overlap with zero/one/multiple selections; explicit Add Transition separately, ordered Auto Arrange separately.
- Pass/oracle: ordinary commands create independent geometry at requested valid destination and zero new links; explicit link creation requires eligible overlapping pair and one Undo. Clamp/rejection at time0 is visible and transactional. Existing unrelated links/owners remain unchanged.
- Evidence/mode/severity: link/geometry/source-clock snapshots, native actual handlers, PCM exterior spans and gestures; automated/manual; P1.

### MW-T05 — Split, slip, reverse, loop and trim conserve intended audio
- Contract/source: TimelineEditing, AudioSlipUI, Model Clip geometry and docs/TIMELINE-EDITING.md.
- Cases/procedure: each endpoint/interior split, one-sample/short/long clip, exact source boundaries, positive/negative slip, reverse+loop, repeated edits; snap off/on/Alt, locks; cancel drag, Undo/Redo and reopen.
- Pass/oracle: sources never read out of bounds; split pieces reconstruct pre-split PCM excluding only intentional local fades; audio slip moves source under a fixed box, geometry edits follow displayed intent; locks reject without mutation. Independent source-index formula and sample tags verify conservation.
- Evidence/mode/severity: before/after geometry+PCM, source identities and round trip; automated/native gestures; P1.

### MW-T06 — Range, multi-selection and clipboard edits preserve ownership
- Contract/source: EditSelection, ArrangementInsertion and range/global ripple docs; user requires current workflow, no legacy compatibility.
- Cases/procedure: all/single-lane ranges, disjoint clips, partial/exact linked frame intersections; Copy/Cut/Delete/Paste/Repeat/Crop/Replace/Unlink, ripple delete/close-gap across lanes; paste consecutive times, empty clipboard, locked crossing and insufficient media; cancel dialogs.
- Pass/oracle: ordinary Cut leaves gap; ripple shifts exactly the named later scope, preserving sample/time/tempo/label/automation relationships or rejects unsupported intersection explicitly. Shared original effects remain shared where intended; separate placement remains independent; one Undo restores exact musical state.
- Evidence/mode/severity: declarative edit oracle/source-tag stream, pre/post JSON, Undo/Redo/reopen and native shortcuts/gestures; automated + manual; P1.

### MW-T07 — Explicit transition frames and outside muting are independent of songs
- Contract/source: Transitions/TransitionControls/Bands/Filters, current frame contract/session41 superseding old anchored/capped frames.
- Cases/procedure: link two overlapping tracks, move/resize frame independently, full four-lane overlap, typed >64bars, shared-audio limits, mute-outside on/off, unlink with/without hard-cut preservation; lock and invalid pairs. Move header/suffix with incoming frame pinned.
- Pass/oracle: frame edits do not move songs or hidden fade anchors; bounds stay within shared audio; muting cuts incoming before start/outgoing at or after end only when enabled. One in/out link per chain owner; independent PCM oracle includes exterior audio and exact half-open edges.
- Evidence/mode/severity: native frame handler/state/PCM edge probes, save/Undo/gestures and captures; automated/manual; P1.

### MW-T08 — Transition bands, swaps and filters implement their selected treatment
- Contract/source: Model transition enums, TransitionBands/Filters/SwapHandles, docs/TRANSITION-PRESETS.md and TRANSITION-FILTERS.md.
- Cases/procedure: enumerate all band modes × four bands, all strengths/gaps/positions, both filter directions/depths and NONE, pairing receivers both ways, zero-length invalid and near edges, manual node interaction and preset replacement; lock/cancel/Undo.
- Pass/oracle: known tone/impulse envelopes match mathematically declared gain/filter shape and selected scope, no hidden endpoint/cut, complementary pairing preserves intended sharp swap; independent frequency/time measurements, not copied generated nodes, establish the result. Measurable continuity and musical preference are separate.
- Evidence/mode/severity: enum-case matrix, analytical envelope/spectral report and listening/captures; automated + listening/manual; P1.

### MW-T09 — Automation nodes edit the addressed owner and parameter
- Contract/source: AutomationAddress/Curve, PresetNodes, TimelineAutomation/AutomationUI; selected-value and no invented endpoints contracts.
- Cases/procedure: every built-in/plugin/stem address; Add/Edit/Delete/Clear/Read/bend/exponential/gain-space, neutral empty curve, same-time nodes, max/min normalized/physical values, click-line reveal, keyboard next/Enter/Delete, cancelled edit; split pieces/gaps, another placement and removed target.
- Pass/oracle: only named address changes, Add creates one requested node, no invented neutral guards/duplicates; selected caption only; interpolation follows independent piecewise formula at boundaries/interiors, one Undo and identical roundtrip. NaN/out-of-order invalid nodes reject safely.
- Evidence/mode/severity: independent interpolation samples, owner/node state, native control and gestures/captures; automated/manual; P1.

### MW-T10 — Quantized navigation and musical labels retain phase
- Contract/source: BarNavigation, TimelineHeader/TempoStrip/TransitionNavigation and docs/BAR-NAVIGATION.md.
- Cases/procedure: beat/half/quarter snap, disabled snap and Alt bypass, bars/labels previous/next, marker move/edit/delete, source manual offset, seek0/end, long zoom/scroll and playback follow versus manual pan.
- Pass/oracle: navigation reaches independent integrated beat/bar positions, visual playhead and audible clock agree; moving a manually corrected clip retains correction unless Alt explicitly changes it. Labels/tempo targets follow defined ownership; no cumulative rounding across repeated forward/back moves.
- Evidence/mode/severity: deterministic navigation sweeps, PCM attacks, ruler captures and Undo/reopen; automated + gestures; P1.

### MW-T11 — Hotcues and source sections are source-owned evidence
- Contract/source: Hotcues/HotcueTimeline/SourceSectionStore/UI and DownbeatReferences; docs/HOTCUES.md/SECTION-FLAGS.md.
- Cases/procedure: HC1–HC9 default/create/edit/delete, S versus HC/downbeat, section add/rename/move/delete, two placements/split, source-only save versus rebuild, stale dialog, missing source/cache write failure; test source bounds and preserved BPM/key overrides.
- Pass/oracle: source seconds stay fixed when placement/tempo changes; S/HC are not rewritten by downbeat save; automatic suggestions stay distinguishable from manual annotations. Successful source save is independently re-read; append-only reviewed-choice snapshot alone cannot prove source-save success.
- Evidence/mode/severity: source identities, before/after store/arrangement/PCM mapping and protected review journal; native/manual; P1.

### MW-T12 — Live addition preserves established programme clocks
- Contract/source: Beatmatch/TimelinePlacement live-add horizon and tempo-target handover; launch clock source ownership.
- Cases/procedure: add third/fourth source BPM up/down while playing, prior future manual markers, no future tail, pending preparation cancellation and source changes; compare established voices before/after and save/reopen.
- Pass/oracle: protected existing-audio/manual-marker horizon stays identical; only new eligible tail reaches incoming source BPM, no retime/redecode of existing voice/cache, no late stale commit. Independent pre/post programme PCM/time maps and source hashes prove preservation.
- Evidence/mode/severity: protected horizon calculation, before/after pulse samples/ownership and actual asynchronous add path; automated/native + listening; P1.

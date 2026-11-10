# Core workflow completion record - 4 November 2026

## W31 - source BPM/key library refresh - complete locally

Source BPM/grid and key saves now have current demonstrated library refresh,
including distinct catalogue/playback objects. A BPM-only save preserves key;
a key-only save preserves BPM, downbeat, segment pitch and placement. Combined
saves refresh text searches and review styling. Failed durable writes roll back;
cancelled drafts do not publish. Valid manual facts resolve their own review field.

The existing source-save implementation was reused. Current source/library
acceptance was added to `Source/AnalysisRefreshTests.cpp`; no rescan or reset is
required. This closes W31, not the detector/music quality requirements of W11.

Source checkpoint: `.local/fw04/candidate-05-source.zip` and its per-file SHA256
manifest; before-source archive `.local/fw04/before.zip`. Build passed in
`.local/fw04/build-05.log`. Executable SHA256:
`6ebefce17f7ad85b366c85f38b9dd3c61f6ba5a8f41d7c2c048222b95d2a15bd`.

Serial focused verification through `verification.run_checks`, stress off:
`.local/fw04/focused-05.json`, library suite 39 checks / 0 failures. The complete
core batch passed 177 checks across seven focused suites, with no infrastructure
or retention errors. This is scoped evidence, not a full regression receipt.

Actual desktop: fresh bridge `.local/fa4/w`, verified owned PID17336, isolated
stores, physical MIDI disabled and monitor muted. Simple A/B/C pinned authored
source times0/8/16s, confirmed four bars in each interval and saved120 BPM.
Advanced BPM-only +0.01 step saved120.01; the library row changed immediately.
Key by ear chose C major/8B and saved the source key; the row changed immediately,
while BPM120.01 and arrangement positions0..20/10..30 stayed intact. Closing the
parent grid editor with Cancel retained the separately committed key. Save/reopen
preserved both facts. Inspected captures: `library-bpm-key-committed.png` and
`library-bpm-key-reopened.png`; semantic evidence `source-save-desktop.json` and
`saved-session-facts.json` beneath that workspace's `evidence/` directory.

The choice route was exercised without judging its musical correctness. Native
checks cover cancellation, failed writes and immediate search/filter refresh.
Automated keyboard input did not change focused JUCE controls in this desktop
session; shortcut usability is not established by the mouse evidence. The owned
app saved and closed gracefully; its processes exited before retention. Generated
WAVs and arrangements were removed under `.local/fa4` with removal hashes;
reports, JSON, source checkpoints and screenshots survive independently of Git.

## W32 - visible and audible track-edge trims - complete locally

Original requirement and acceptance (retained from the outstanding queue):

| W32 — implemented, acceptance in progress | **Drag track edges to trim visible and audible content.** Give the placed track/segment's left and right outer edges clear trim handles, distinct from transition markers and fades. Dragging an edge inward hides the excluded waveform and removes that interval from playback/export. Keep the original file intact so dragging outward can restore available content. Trim this placement without implicitly moving other tracks or changing source alignment. | Exercise both edges by ordinary mouse drag, with snap/free placement, Undo/Redo, save/reopen and rendered audio. Transition markers remain independently available and are not silently moved/deleted with the track edge, including when outside the retained waveform. If trimming removes audio a transition needs, show the conflict and its consequences rather than silently restoring excluded sound. Distinguish this from dragging a transition frame or applying an inaudible visual crop. Reuse existing trim/source-range support where it meets the outcome; explain any unavailable state under W04. |

Six-pixel outer copper handles are separate from fades, source anchors and
transition corners. Trimming changes the placement's source range; outward drags
restore available original content without moving neighbours or source alignment.
Shared effects keep an independent routing clock, durable in session format48.
Transition frames/nodes retain their own timing even outside trimmed audio, with a
visible conflict explanation instead of silently restoring excluded sound.

Source checkpoints: candidate05 source ZIP/hash manifest in `.local/fw04`, build05
and executable identity above; candidate06 ZIP/hash manifest/build06 and identity
in `.local/fw04/focused-06.json`. The candidate05 edge suite passed36 checks with
0 failures through `verification.run_checks`, stress off. It covers raw, reverse,
loop, prepared clip-local and elastic PCM, shared routing, both mouse event paths,
snap/free, Undo/Redo, real save/reopen/export and restoration after reopening.
Initial failed fixtures and the real shared-return routing failure remain in
native02/native03 reports; their repairs are not inferred from old passes.

Actual ordinary desktop gestures in isolated `.local/fa4/w`: both snapped edges
0..20 ->2..16; both outward restorations after reopening; mouse Undo/Redo. Export
probes: omitted1s/17s silent; retained3s stereo samples exactly equal the restored
clip. Captures and `trim-oracle.json` retained. In `.local/fa5/w`, Snap off and an
ordinary right-edge drag ended audio at53.538767395626245s while the transition
frame32..60 remained visibly independent outside it. Export lane0 was silent56s;
Undo restored audible56s; retained40s PCM was exact. Save/reopen kept the fractional
edge and independent frame. See `free-trim-oracle.json`,
`free-trim-external-transition.png`, `free-trim-reopened.png`, and
`saved-session-facts.json`. The native suite verifies source file hashes unchanged.

Unavailable states are explicit: extending clip-local prepared audio beyond its
prepared PCM requires preparing the restored range; old paired bass ownership
must be replaced with the current ordinary-node preset before trimming. These
refusals preserve the mix. This is not universal plugin-tail, physical-device,
listening, detector or full-product qualification. Both owned desktop roots saved,
closed and exited before hashed removal of generated audio/sessions; no production
cache, original media or protected references were touched.

## Supporting core work still under acceptance

W02/W03/W05-W08/W11 source changes are implemented in candidate05, with
focused results recorded above. They remain in the roadmap where wider acceptance
is incomplete. Candidate06 fixed the observed scene picker hiding live output;
70 focused checks passed, and actual Prism-to-Neon active GPU replacement kept
Live mix visible through choosing/replacing. Candidate07 fixes a subsequently
observed Crossfade summary and protects individual band/filter manual-node
replacement. W01 has no specific reproducer from Jay and remains open.

The user subsequently requested avoiding the computer-use skill as much as
possible. Continue with source, the development bridge and focused checks;
reserve unavoidable physical/desktop acceptance honestly. No production control,
cache reset, launcher change, public distribution or external agreement occurred.


## W03/W17 - partial linked range editing - local implementation complete

Cut/Delete/Crop and Replace can now slice selected transition participants.
They reuse the existing audible unlink/materialisation operation before slicing:
affected frames become ordinary source curves; surviving audio and its unselected
partner keep their heard treatment and optional outside-bounds cuts. Cut/Crop
menu text discloses unlinking; Replace previews the number of affected frames
and offers Cancel. Locks, unavailable sources and stale state still refuse
atomically. One Undo/Redo restores the complete relationship and audio edit.

This completes this implementation slice of W17, not its remaining protected
ranges, automation groups or changing-tempo group retiming, or W03 newcomer
gesture acceptance. Independent constant-tempo authored fixtures support the
result; arbitrary plugins, changing source maps and listening are not qualified.

Source: `.local/fw04/candidate-10-source.zip` with per-file SHA256 manifest;
build10.log. Executable SHA256:
`1813afe90dcfe07e3c51a3ca62515c9ab0791cf3baa571d1de6df8913a51a0f9`.
`focused-10.json`:68/0 through verification.run_checks, stress off, zero
infrastructure/retention errors. Captured output around both boundaries and
interior/exterior positions matches the retained/unselected-source PCM oracle
exactly; exclusions are zero. Actual source-backed save/reopen and offline
export renderer agree. Undo/Redo, replacement preview and Cancel passed.
Inspected native captures: partial-linked-cut.png (visible gap and editing
console), partial-linked-replace-question.png (readable consequences), and
editing-export-unavailable.png (actual disabled automation controls/status).
No computer-use skill, production session/data/cache/scan/launcher change.
Generated native audio/sessions removed through shared retention; reports,
source snapshots, measurements and pictures retained independently of Git.


## W03 urgent Snap/IN/OUT/pair locks and W02 removal - local implementation complete

Jay's corrected contract: OUT changes within its stationary outgoing track;
incoming audio moves so IN remains aligned. IN chooses another incoming source
point beneath the previous fixed OUT, keeping the join fixed (explicitly
confirmed). Append to a last explicit OUT aligns the next IN and records the
pair identity. Snap destinations use only the project timeline lattice; prior
free offsets and neighbouring track pulses cannot become snap references.
Track movement aligns its musical pulse to that grid; nodes and markers use
project time. Alt/free editing remains available without mass-quantising a mix.

LINK badges are replaced by actual accessible pair movement buttons. Unlocked
headers move independently; locked pair closures move together. Unlock an active
pair to change its independent IN/OUT alignment. Previous edit protection remains
separate under advanced Protect edits. Session49 stores movementLocked per pair;
analysis revision is unchanged and no reset/rescan follows.

Visible Remove transition offers Keep current mix, Clear treatment, and Cancel.
Only the selected frame/pair lock is removed; adjacent frames remain. Keeping
the mix materialises heard treatments and outside cuts. Clearing also replaces
Volume/EQ/Filter nodes in the frame, including custom/shared nodes, and removes
that pair's outside-bounds mute. The question explains this. Undo restores it.

Candidate11 built, SHA b13a95859a74382657368da0699405a95605f64f50107760ca3fdb7bc2a9f08f,
native43/2 retained. Actual marker gesture checks exposed waveform range selection
intercepting IN/OUT before their handler. Candidate12 gives visible marker,
transition, swap and audio-move handles their manipulation priority. Its219/0
checks (pair45, live70, direct68, trim36) passed serially via run_checks, stress
off, zero infrastructure/retention errors. SHA256:
`7fcc502932bcebb921c5ca5e1907a628062be4be141c4b0448d5234d09866d4e`.
Tests include actual IN/OUT and source-node mouse handlers, free-then-Snap,
off-grid-neighbour exclusion, locked closures, PCM, visible removal/Cancel/Undo
and actual source-backed reopen. Source11/12 ZIP/hash manifests and failed/native
reports remain independently in `.local/fw04`.

Candidate13 changes only the title reserve beside72px pair buttons, fixing
observed first-letter overlap. Build13 passed, SHA256:
`6b21201004d5f0acf9b2b88de23ee736d8968e566d0d1a148c9a522dc5000c55`.
Fresh bridge `.local/uq13/w`, owned PID26668, verified protocol/root/capabilities,
physical MIDI disabled. Three authored12s sources, private corrections120BPM and
0.213s downbeat. Pair A/B locked, B/C unlocked. Free moving B to5.537 moved A/B
by1.25s while C stayed8.287. Actual Undo restored it. Snap moving B to5.42 placed
B5.287/A1.287/C8.287; the source0.213s downbeat is exactly timeline5.5s/beat11.
Save/reopen retained geometry and pair flags. Actual app export-renderer probes
before/after translation:128 stereo frames at48kHz, peak0.32769775, max PCM error0.
Inspected real Workstation pixels show readable full titles, Locked/Unlocked
states, visible Remove and correct grid/transition placement. Details, source
hashes, semantic session snapshots and captures are in that root's evidence.

The initial isolated fixture correctly refused incompatible captured-clock
metadata, then a second fixture lacked mandatory filter/alternatives. Both
failures and unchanged app states were retained; fixture3 corrected those fields
and exact opened positions/status were asserted. The bridge does not expose
monitor mute, so all13 geometry/PCM checks kept transport stopped. No listening,
physical desktop gestures, MIDI/ASIO or full regression/performance/stress
qualification is claimed. Native handler/callback evidence has its stated scope.
The owned app saved/gracefully closed and PID exited before retention; generated
audio/sessions were removed with hashes while JSON, reports and pictures remain.
No computer-use skill, production control, original-media/protected-reference,
analysis/cache/scan or selected-launcher change. W03 newcomer/accessibility and
library preview remain open; W01 intermittent zoom breakup remains unconfirmed.


## W03 — library title preview strip

Source checkpoint `.local/fw04/candidate-15-source.zip` and per-file SHA256 JSON;
bridge/client/docs checkpoint `candidate-14-tooling.zip/json`. Executable SHA256
`7082f66d62a48401b68b0333e59b85b6d3577cfd31c7a0eb155efc735a8e40bd`,
UI0.55.68/session49; analysis unchanged. No production activation/reset.

Readable title above a seekable waveform/line; click/drag auditions the selected
source passage, same-source seeks reuse its open reader/route, asynchronous loads
honour the latest position. Artwork retains play/stop. Left/Right5s, Shift30s,
Home/End, Space/Enter and Escape are available. Review warnings do not disable
preview. Recycled rows and lost focus cancel stale gestures. Existing Arrange
master route pauses programme without rewinding; Record/Perform uses configured
private output. Arrangement, Undo and offline audio remain unchanged.

Candidate14 built `9ebe02d9c80ee69b74c3e3dda4b08e6df7bd9f2f1b3b8b061ff7eb1e385d0035`;
new seek26/1 retained. Its only failure was native accessibility assertion: JUCE
requires a window peer, absent from the hidden test fixture. Candidate15 creates
a hidden peer; no visible production window or physical test output is needed.
It also adds focus-loss cancellation, callback copy and filename/artist hover.

Candidate15: seek27/0, existing preview27/0, library review/sort39/0 =93 focused
checks, serial through `verification.run_checks`, stress off, zero infrastructure
or retention errors. Real decoded8s PCM oracle: early.12, late.36; selected private
late passage.18. Actual callbacks, pointer/key handlers, EOF, async replacement,
missing file/device failures, unchanged programme cursor/document/history and
export exclusion checked. Wide/compact captures inspected. The source14 failure
and source15 repair/evidence remain in `.local/fw04` independently of Git.

Bridge adds owned-source `preview` with explicit stop/state and optional
`view.monitorMuted` through the real control. Python harness17 tests OK,1skip
(observed result journalled). Required `scripts/verify-bridge.py`: native31/0 and
actual desktop27/0, `.local/qb15/summary.json`; decoded audition starts within the
file, same-source seek keeps programme cursor fixed, Stop idles, save/reopen and
graceful close pass. Screenshot inspected. Isolated app restarted only within its
owned workspace and exited; ownership retention manifests have zero errors.

Jay requested faster builds while no other processes were running: session-local
normal build priority, still one compiler worker. Candidate15 used incremental
compilation of two implementation files. Test priority remains below normal.
No full gate/stress, physical mouse/keyboard, headphones/driver or musical-listening
qualification. Broader W03 newcomer, gestures and accessibility acceptance remain
open. [Usage](../LIBRARY-PREVIEW.md).


## W02 and W05 — live preset replacement and whole-mix timeline cues

Local implementation/functional acceptance completed on candidate22. Source/UI
0.55.68/session49, analysis unchanged. SHA256
`357f8c561ffdf04f07754ac7e3cc39e1d76e01d77e4e805777f0cca5d7777d81`;
`.local/fw04/candidate-22-source.zip/json` and tooling ZIP/hash are independent
checkpoints. Earlier candidate/source/failure evidence is retained alongside them.

Actual app inspection exposed whole Crossfade retaining previous Bass swap curves.
Its private compiler cleared inactive Clip.controls while effectControls held the
active source automation. The whole-preset compiler now clears both private stores
and reads/writes captured bass ramps through the actual owner. Interior treatment
is replaced; exterior and mixer-lane curves remain. Fresh/replaced Crossfade PCM
agrees, repeated application cannot multiply a fade, manual replacement previews
its range/targets and combination, Cancel and Undo preserve sound. Two overlapping
pairs explicitly name the shared12–16s span before replacement; the other partner
and exterior middle-track PCM remain unchanged. Record/Perform keeps chronology
and later fader changes through structural Undo/Redo; actual callback and take
replay PCM agree. Read/Touch/Latch ownership guards remain explicit.

Further investigation found real ordinary-EQ sharp-cut delay: exact-cut subdivision
remained in the piece processor after ordinary source EQ moved to its shared
original-track owner. That actual processor now splits and evaluates at the exact
sample instead of waiting for a32-sample tick. Preset-node fixture maximum PCM error
was.2532635331 on blocks1/31/173/512; it is now exactly0 on all four, with the
original strict equality retained. Ordinary source curves were also missing from
the timeline inventory because it read inactive clip storage. The inventory reads
actual effect controls; shared ordinary source lines are deduplicated per lane,
while piece-specific generators remain separate. Actual two-song cut handles are
visible and editable, with exact Undo, save/reopen and node curves.

W05's selected-segment HC1–9 pads, Alt+1–9 and typed/virtually learned MIDI action
all jump the whole mix immediately, stopped or playing. Quantised pause settings
do not delay cue navigation. Other audible lanes follow the new programme time;
the viewport reveals it. Out-of-placement cues explain their refusal. During an
editable take the seek is captured and replayed with matching actual callback PCM.
Two stale stopped-only/source-only hints have been corrected. Physical MIDI remains
W12/W28 qualification; source-only internal performance voices do not back these
pads.

Candidate22 serial focused: preset172/0 + live76/0 + shared-lane49/0 =297/0.
Required bridge native36/0 + actual desktop37/0 =73/0; Python harness20 tests OK,
1skip. Stress off, below-normal tests, zero infrastructure/ownership retention
errors. Build normal priority for this session at Jay's request, still one worker.
Actual app checks real mode menu, Arrange publication during playback, Record/Perform
replacement question/confirmation while capturing, cue seeks, Stop/finalise,
performed project snapshot, save/reopen and graceful owned close/restart/close.
Monitor muted throughout audition/live fixture checks. Actual/native captures
inspected. `.local/qb22/summary.json`, reports/evidence and `.local/fw04/focused-22.json`.

Failures remain independently retained:16 app11/2 encountered the expected question
which the driver had not handled.17 native18+74/0 and initial app36+35/0 did not
prove applied curves. Stronger17b/19 app14/2 initially inspected the restored
pre-take mix after Stop, an incorrect oracle. Corrected oracle inspects the actual
performed take project event: unchanged17c fails14/1 with retained old bass cuts;
19b passes37/0 with zero bass throughout the frame.17c closes clean/stopped owned
app gracefully after the assertion. Earlier failures needed owned-child fallback
termination, recorded without claiming graceful completion. Before/after curves:
`.local/fw04/whole-preset-before-after.json`.

Preset19's168/45 also occurred identically on unchanged17. Repairing assertions and
fixtures to inspect the actual source owner reduced20 to169/7; no thresholds were
weakened. Other fixtures had assumed normal Add automatically overlapped/linked
tracks, superseded by Jay's independent placement contract. They now explicitly
request automatic transition placement.21 reached171/2 with exact-cut and inventory
repairs;22 corrects that last fixture and passes172/0. Candidate18's new manual
volume fixture put points on a disabled curve, correctly refused Read-state
replacement;19 enables the intended active curve and retains diagnostics.

No claim of resolving W01's intermittent zoom/audio report from these bounded
results. No full gate/stress, physical gestures/devices, long playback or musical
listening acceptance. Broader real-workflow qualification remains W28. No production
session/media, analysis/cache, protected references, scan or launcher activation
changed; no agreements, purchases or public distribution.

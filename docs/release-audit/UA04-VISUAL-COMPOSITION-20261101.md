# ua04: visual composition, review sorting and alignment guide

1 November 2026. Continues the whole user-readiness plan. Jay accepted shared
neutral lane curves plus separate per-track transition fades. New visual direction:
use section markers, more scenes more often, and faster quiet-to-loud transitions.

## Implemented and checked

- Visual scene selection balances whole-show usage across eligible styles instead
  of restricting the whole show to one family. Image shows retain verified image
  compatibility. Five supported blends get deliberate variety; musical filter
  handovers retain direction. Missing music windows get short visual-only blends.
- Source section markers map through offset/rate/slip. Retained RMS before/after
  a boundary identifies rises of at least6dB: a half-beat blend lands on the loud
  section; ordinary changes use four beats. Musical and section windows fit
  together without three active scenes. Forward non-looped sources currently get
  section cues; reverse/loop placements keep song-level changes. Labels do not
  overpaint in overlaps; manual contiguous cuts have a visible Cut marker.
- Independent GRID CHECK sorting: Unknown BPM, Review BPM/pulse/bar, Detected,
  Manual bar/grid. KEY CHECK independently groups unknown/review/detected/manual.
  High detector scores are explicitly not human confirmation. This is review UI,
  not a detector-accuracy improvement. Narrow library layouts need horizontal
  scrolling or a wider library to see the review columns.
- Cyan dashed alignment guide with visible Guide toggle, Shift-click placement,
  drag, project Snap and Alt-free placement. It is a view-only reference, separate
  from the playhead and music document. Hiding/dragging/double-clicking it does not
  seek or create Undo. It is not saved as musical session content.
- Timeline context-menu Add Transition now agrees with the live panel route.

Final candidate `323a800991d0f3c5ed9f45ad20fdadd538b5cdee2e6afa8c3d0e946c2d38bcb8`.
Final focused native checks via verification.run_checks: visual24/0, review12/0,
guide6/0. Earlier candidate01 context-menu/live-transition30/0 (that implementation
unchanged since); ua03 lane Solo40/0 remains separate exact-candidate evidence.
Native pixels inspected directly. No broad gate, benchmark, stress, physical
input/device/listening, full rescan, user-data change or launcher activation.

Visual proof covers44-track planning, every handover,12 eligible image looks,
five transition kinds, source-timing section cues, quiet/loud speed, >88 scenes
with markers, final end, saved visual-document reopen and native editable timeline
pixels beyond track40. It does not establish actual rendered late-scene GPU output,
complete source-backed44-track playback, or musical/physical acceptance.

Three serial guarded builds. Candidate02 visual24/1 preserved: long music windows
excluded section markers. Production scheduling was fixed by fitting the combined
windows; candidate03 visual24/0. Initial review screenshot did not expose columns;
fixture layout resized and new pixels inspected. All reports, source archives,
logs and independent candidates are in `.local/ua04/`. Generated fixture cleanup
had zero errors. No owned process remains at this checkpoint.

## Continue

Implement accepted lane automation and default8-bar handles coherently across UI,
controls/recording, persistence and audio/export. Preserve per-track transition
fades; short tracks use shared midpoint handles, overlaps merge coincident handles,
and existing authored lane curves must not be reset by insertion. Actual editing
preview and musical/user acceptance remain open. Six existing music candidates
are listed in `.local/ua04/evaluation-subset-proposed.json`; no annotation/analysis
has occurred and no independent ground truth is claimed. Jay had no specific
misanalysed names. Broader failure classification, detector improvement, newcomer
trial, hardware and later release qualification remain unfinished.

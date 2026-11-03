# Editing workflow — 1 November 2026

Jay's review of ua05 rejected the Editing tab's hierarchy and purpose. The primary
workflow is select arbitrary audio, split a track, copy/cut/paste a passage, move
tracks and adjust fades. An arbitrary drag must not silently use a fixed length
or global Snap. Keep the approved graphite/copper identity and existing audio
semantics; reorganise around tasks rather than add another flat toolbar.

## Reference findings

- [DJ.Studio editing](https://help.dj.studio/en/articles/8207056-editing-audio-files-cut-copy-paste):
  waveform selection precedes contextual Cut/Copy/Paste; Shift-drag selects audio.
  Its documented one-beat minimum is not adopted: Jay explicitly wants arbitrary
  manual selection. Alloy retains explicit beat snapping as an option.
- [Official MixMeister Control guide](https://www.numark.com/images/product_downloads/mixmeister_control_quickstart_guide_v1.1.pdf):
  selected-region editing groups Cut/Copy/Paste, Split at cursor and Undo together.
  This is workflow evidence from the official controller guide, not a claim of
  inspecting the current desktop application. Official Windows CHM and Mac PDF
  manual links were found; the PDF attachment fetch failed.
- [skills.sh](https://skills.sh/) was consulted for method discovery. The existing
  Impeccable Operate guidance fits this task; no additional skills installed.

## Implemented workflow

- Audio edits opens with a selection summary and Move tracks / Select audio.
  Free drag is default; separate beat-drag and fixed musical presets are explicit.
- Click waveform to place the cut/paste cursor; drag waveform for a passage.
  Shift-drag temporarily selects in Move mode, and track headers remain movement
  handles. Selection dragging does not change Snap, audio, transport, saved state
  or Undo. A visible duration and highlighted bounds identify the audio range.
- Core Split at playhead, Cut, Copy, Paste and Delete stay on the main page.
  More edits groups selection, selected-audio operations and explicitly all-lane
  time-closing operations. Stale native menu targets are refused.
- Fades & timing shows only per-track fields. Automation shows only curve tools
  with lane/track owner selection. Choosing a curve disables range interception.
  Entering per-track settings clears the transient range so its fields are usable.
- Source/snapshot retention in `.local/ua07/`. Focused native workflow verification
  and actual pixel inspection recorded below.

## Retained first pass

Candidate `0b9567bbf13dff6fc03bf40a8d39a5c1820b3c698921ad9e7bd5706ae91d4fe9`
completed29 checks with one failure: the primary actions still scrolled out of
view at1250x800. Native screenshots confirmed the ordinary minimum lane height
consumed the Editing dock allocation. Four pixel captures were inspected in one
round. The correction reserves the complete editing dock before applying ordinary
lane minimums, keeps all task pages compact, and completes waveform click-to-cursor
behaviour. The initial executable/report/source remain in `.local/ua07/`.

The second candidate `f5a711c7bd58e69604c2516f23d1d4c43d8fdde023327c48cf89047288eb772f`
passed workflow30/0 and lane-automation49/0. Four native captures confirmed every
Editing page fits, but the smaller lanes at1250x800 hid Mute/Solo/level controls.
This functional accessibility regression required one further compact-strip fix,
not another aesthetic polish pass. Compact strips now share a row for those three
controls; the focused workflow adds checks for every lane at both window sizes.

## Final evidence and acceptance

Candidate `9850cb441e06a6a0e2900749619572695b8805f995e4d81c5971504a15ff4328`:
**workflow32/0**, no process/retention errors. Tests exercise production mouse
handlers and button actions, free/reversed/beat selection, click positioning,
cut/copy/delete/paste/split, Undo/Redo, actual arrangement write/parse and independent
original-passage PCM comparison through the export renderer. At both tested sizes,
primary actions and all four lanes' Mute/Solo/level remain visible. Final1250x800
pixels inspected after the compact-strip repair. Other pages were inspected on the
preceding candidate; only compact strip layout changed afterward.

Jay's review of the final preview: **"Clearer; keep this direction"**. Record this
as accepted direction, not final usability or physical mouse/keyboard acceptance.
More-edits menus, arbitrary DPI/multi-monitor layouts and real-music listening
still need wider workflow qualification. No broad regression result is implied.

Three serial guarded single-worker builds. Receipts and hashes retained for all
runs; source checkpoints precede each build. Final source/bundle/candidate and
screenshots are independent of Git in `.local/ua07/`. Generated fixture audio and
arrangements were removed through verification.run_checks with zero errors.

No detector modification, full gate, scan, stress, benchmark, production launch or
activation. Current audio/session ownership from ua05 remains the contract.

# Destination safety and error/retry — ua09

**Update:** [ua13](UA13-FOCUSED-VERIFICATION-20261101.md) successfully builds this
repair and passes111 destination checks plus42 save/export checks on one candidate.
Confirmation/error pixels inspected. The pending history below is superseded for
that focused scope; wider qualification remains open.

Jay requested the next destination/error/retry checkpoint and explicitly kept
full readiness open. No activation or public release was requested here.

## Finding and repair

Source inspection found a concrete overwrite-confirmation gap. JUCE's non-native
chooser checks the typed path before calling Alloy. Alloy then adds/replaces the
extension. For example, entering `mix` can target an existing `mix.wav` that the
chooser never asked to replace. Save As, WAV, playlist and track-list paths share
this pattern. The relevant JUCE implementation is
`.local/deps/JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooserDialogBox.cpp`
(`okButtonPressed`) and `juce_FileBrowserComponent.cpp` (`getSelectedFile`).

The repair confirms the final destination when extension correction changes the
path to an existing file. The prompt names that final path and offers Replace or
Cancel. Existing same-path chooser confirmation remains. Save As keeps its
chooser ownership until confirmation closes; cancelled/failed saves do not run
the saved continuation. Save/export admission and stable take identity are checked
again after the new asynchronous boundary. Backend source protection, writers,
backup policy, audio rendering and saved formats are unchanged.

## Verification status: pending

New `--destination-safety-only` native checks use the existing 0.2-second stereo
fixture with two placements, returning before broader take tests. Coverage written:

- Real chooser button submission for all four corrected destination paths;
  cancel keeps existing bytes and creates no backup; acceptance preserves backup.
- Same-path chooser overwrite cancellation and original-source refusal.
- Arrangement-only sources and Windows hardlinks across all four writer routes.
- Missing WAV folder and directory refusal.
- Windows-held destination failures for session, WAV and text output; error
  details, ownership release, dirty/session preservation, then successful retry.
- Cancellation after the first 128 rendered frames, followed by renderer retry;
  original source bytes and complete mix state checked after the journey.
- Two intended captures: final-path confirmation and export commit-error details.

No build or native test has started: memory preflights refused at 1635,1779 and
2482 MiB available (2500 MiB start threshold). A separate observation briefly
exceeded the threshold, but memory had dropped before the next guarded start.
No automatic memory cleanup or process closure was attempted. The source review
caught a missing UI-message include and an unavailable JUCE message getter before
compilation; neither is a compiler/test result. No passing claim is made for the
new code, and no screenshots exist yet.

Before/candidate source archives, manifests and working-tree patches are retained
independently of Git in `.local/ua09/`. Resume with the guarded one-worker build
only when memory is available, then run the destination selector and existing
save/export journey serially through `verification.run_checks`. Inspect both new
dialog captures. Retain any failed build/run rather than overwriting evidence.

## Limits and continuation

This is an unverified source repair, not a ready preview. The normal launcher
remains ua08 `9770639eb18c`; running applications, production data, originals and
analysis are untouched. No scan, stress, benchmark or full regression was run.
Full readiness remains open, including wider functional audit, musical/physical/
newcomer acceptance, analysis ground truth and final release qualification.

Even a future focused pass will not establish disk-full, network/removable-media,
power-loss/crash durability, malicious concurrent path substitution, symlink
variants, long-file capacity or physical file-picker browsing. Source protection
currently concerns sources known to the engine; do not describe it as protection
for arbitrary music elsewhere on disk.

[skills.sh](https://skills.sh/) was consulted for method discovery; the repository's
native workflow and retention harness remained the appropriate method. No skill
or dependency was installed.

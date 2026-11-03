# Launcher activation and save/export journey — ua08

Jay explicitly requested updating the launcher to the latest build and continuing
the readiness plan. The accepted ua07 editing candidate was staged first. The
continuation added a bounded save/export audit; only `PerformanceTakeTests.cpp`
and the `SelfTest.cpp` selector changed. Production code is unchanged from ua07.

Final launcher: `out/releases/0.55.64-9770639eb18c/Mixworks.exe`, SHA256
`9770639eb18c76e8f205dc6d191ba795ba2ed236d2668729f841c1544ea7504a`.
The desktop shortcut resolves through `Mixworks.cmd` to `out/Launch.cmd`.
Both activation steps verified source archives, exact executable/model/runtime,
all 33 staged assets including the paired visual runtime, and native report and
retention identities. Launcher backups and receipts are in `.local/ua08/` and
`out/`; previous candidates remain intact. No normal application was launched or
restarted and no production music, analysis or session data was changed.

This is an explicitly authorised **local development preview**, not a complete
regression receipt or public release. The existing broader failures remain open.
The normal complete/scoped deployment gate was not relabelled or weakened; this
uses the established explicit local-preview staging path.

## Bounded native evidence

`--save-export-workflow-only`: **42 checks, zero failures**, using
`verification.run_checks`, below-normal priority, stress disabled. The branch
returns before recorded-take tests or the long-clock export suite. One 0.2-second
stereo fixture feeds two trimmed placements on separate lanes with silent gaps.

- Actual Save As cancellation preserves dirty state and does not invoke the saved
  continuation. Successful Save As commits its chosen file without Undo; Save
  reuses it, and another Save As keeps both files.
- Actual asynchronous source-backed reopen restores placement IDs, source offset
  and timeline position.
- Ordinary mix export writes 2,000 frames of 8 kHz, 24-bit stereo WAV. Every sample
  agrees with an independent constant-signal/gap oracle. Ending-prompt cancellation
  writes no file and releases export state.
- Actual source-playlist and track-list choosers write their outputs. The M3U8
  contains the repeated original source, and text explains that edits remain in
  the session. Both cancellation routes write no destination.
- Exports preserve session identity, dirty state, Undo and original fixture bytes.

Four native dialog captures were inspected: labels, paths, filenames and actions
are visible. Chooser captures occur immediately after injected folder selection,
before asynchronous directory listing refresh; these do not qualify browsing
behaviour. Actual destination files and their content were checked separately.
No physical mouse/keyboard, musical listening, plugin-tail, disk-failure, long-file,
full-format, crash-recovery or whole-rule acceptance follows from this fixture.

The first build failed on a test writer pointer type; corrected before native
execution. Both serial single-worker build logs and source checkpoints survive.
No memory-guard stop. One native run; retention removed five generated audio/session
files (84,693 bytes), with zero retention errors. Text outputs, reports, hashes,
screenshots, source archives and final binary survive independently of Git.
An initial staging source-membership assertion stopped before mutation because
the archive intentionally omits the third-party LICENSE file; source checking was
restricted to archived source inputs while packaged licences remained hash checked.

## Continuation

MW-D01/D03 and MW-X01/X03/X05 now have partial current native evidence; none is
marked wholly verified. The core save/export route works in this bounded journey;
no application repair was needed. Next: current error/retry and destination safety
cases (existing output, protected source, invalid destination), then remaining
functional audit gaps. Keep tests bounded; `--export-only` contains a 90-minute
callback simulation and must not be selected wholesale under the current policy.
Real-music ground truth and analysis remain deferred after ua06's memory stop.
No broad gate, scan, stress, load or performance work was run.

[skills.sh](https://skills.sh/) was consulted for method discovery. Its general
catalogue did not replace the repository's native workflow/retention harness;
no additional skill was installed.

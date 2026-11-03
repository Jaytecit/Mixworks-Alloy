# Recovery and source relinking — ua15, 1 November 2026

Final candidate `c23eed125222979242d2cdb1441bfd77f4f031bc263772d32c0d25218696e9e6` builds and passes **67/0** bounded `--recovery-only` checks through `verification.run_checks`, below-normal priority, stress disabled. No infrastructure or retention errors. Three final native captures inspected. This is scoped evidence, not whole-rule or full-readiness acceptance.

Source-confirmed defect: Locate changed only the media path, bypassing path-keyed saved source bindings and leaving take snapshots at the old location. `relinkProjectSource` now relocates matching current-format media and bindings, retaining hashes/dimensions, including take initial/project-event snapshots. Wrong audio of identical dimensions refuses transactionally; exact moved audio succeeds. Failed parsed input and exported-take destinations remain unchanged. No format/detector change.

Native decision Cancel, chooser Cancel, wrong-file failure, exact-file retry, explicit Save As and independent file reopen preserve the current document as applicable. Independently prepared relocated take snapshots render the authored stereo ±0.125 PCM oracle (8 kHz, 1,600 source frames, 64 inspected output frames). This is snapshot preparation/render evidence, not complete continuous take replay/export acceptance.

Recovery checks include separate-process live leases and owned abnormal exit, corrupt-current fallback, stale-fingerprint refusal, unrelated-file preservation, dirty paused snapshot, Later/Restore, durable successor, preparation Cancel and missing audio. Fixtures are isolated; no device/model, scan, load/stress/performance work or production control.

Three serial single-worker below-normal builds succeeded. An earlier wrapper launch failed before compilation because Windows PowerShell lacked pwsh under PSHOME; the same wrapper was launched with pwsh. First native run52/8 retained: added chooser checks were after the existing stopDispatchLoop, and JUCE refuses subsequent callback posts. Moving them before shutdown produced67/0. A final message-only clarity repair followed pixel review and passed67/0 on the final candidate. Every attempt remains separate. Final retention removed4 generated files/98,302 bytes without errors.

Exact source archives/manifests/diffs, unique executables, bundle identities, reports, logs, journals and capture hashes survive independently of Git in `.local/ua15/`. Launcher remains ua14/ua13 `3a8286068928`; running production PID27916 was observed and untouched. No activation or user data changes.

MW-D01/D04/D05 remain open for remaining rule cases, physical browsing/input, plugins, multiple missing files, takes-only source failures, shutdown interleavings and broader current-candidate regressions. Full regression is deferred under the iterative minimal-check instruction; persistence changes make that deferral material. Hardware/listening/novice/detector accuracy and release qualification remain open.

Next steering: Jay supplied a consistent source waveform/grid offset screenshot. Investigate shared sample-time/drawing coordinates before changing detector results, corrections or production data. No new rescan/reset is authorised by this audit.

Method discovery consulted [skills.sh](https://www.skills.sh/); repository-native transactional workflow/independent PCM checks remained appropriate. No plugin installation or subagent.

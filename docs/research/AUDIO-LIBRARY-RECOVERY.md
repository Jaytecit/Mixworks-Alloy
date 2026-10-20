> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# Music-folder and timeline loading repair — v0.23.0

The audio files were present. Two separate defects explained the reported behavior:

- Every changed analysis record created another numbered backup beside the music. Records also allowed an older detector to replace newer analysis; preserved histories contained alternating detector revisions.
- A fresh timeline insertion compared current file bytes against the catalogue's old fingerprint and rejected a changed file. Background scanning also rejected changed catalogue identities instead of supplying a fresh entry for subsequent drops.

70 of 359 active analysis records no longer matched their source files. For Ode To Omar, the prior reference copy and current MP3 have identical bytes after the ID3v2 metadata block; its metadata changed. This does not establish what edited it, or that all other changes were metadata-only.

## Result

Fresh catalogue drops load the current file and duration. Manual corrections survive only when source identity matches. Adoption replaces the pending source or library entry; existing loaded clip/Undo objects retain their audio. Actual loading failures now name the file and reason.

Analysis storage is `%LOCALAPPDATA%/Mixworks/Analysis-v1`, with a path-hashed filename and recorded source path. Updates are atomic and serialized per record, identical saves do nothing, older detectors cannot downgrade records, and only one previous-version backup is retained. Unidentified files/backups are preserved. Legacy files remain readable and migrate after fingerprint verification. Music files are never edited by this storage workflow.

## Recovery performed

After the user saved and closed the old application:

- Migrated 289 matching active records, preserving manual corrections.
- Preserved 70 stale records for recovery; normal loading/idle scanning rebuilds their analysis from current audio.
- Moved 1,791 analysis backups and all 359 active legacy records out of Music. Every moved file's SHA-256 matched before and after.
- Verified all 645 original audio and MixMeister `.mxm` files against the pre-recovery byte counts and SHA-256 values: unchanged.
- Confirmed zero `.mixworks-analysis.json*` files remained in Music after reopening the new app. At the last check, 320 of 359 tracks had central records; remaining tracks rebuild when scanned or loaded.

Recovery archive: `C:/Users/jayte/AppData/Local/Mixworks/LegacyAnalysisArchive/2026-10-17-6aac1a983543`. Its `manifest.json` records original/destination paths and digests; `moves.jsonl` records verified moves. Nothing in this archive was deleted. Restore individual files only with the app closed, checking the manifest and refusing to overwrite an existing destination.

## Verification

Red results: `.local/v023-red/` reproduced out-of-folder storage, detector-downgrade and changed-catalogue insertion failures before the production changes.

Passing regression suites total **464 checks**: rhythm 27, beat grid 25, main 279, playback cache 53, library 42, placement 38. Results are under `.local/verify-rhythm/v023-first/`, with the corrected library restart suite at `.local/verify-rhythm/v023-final/library/results.txt`. The initial library failures came from restart subprocesses using a different isolated analysis store; they now share their parent's test store. A production CLI migration fixture also verified unchanged original bytes and retained manual BPM/downbeat/key values.

**35 real-track checks** passed in `.local/v023-real-tracks/results.txt` using Better Off Alone and Ode To Omar: production decoding, automatic downbeat availability, changing-tempo grid/anchor alignment, transition rendering, and isolated stem rendering. These checks do not establish human listening quality or new MixMeister detection parity. `13 Guns` also decoded successfully through the read-only CLI; its uncertain/variable automatic tempo and unknown downbeat remain separate detector limitations. The v0.23 native UI opened successfully, but user input resumed during attempted drops; no completed native drop or listening pass is claimed.

Recovery evidence is in `.local/v023-recovery/`: original-file audit, migration report, archive plan/result, post-recovery source verification and final state. The launched release is `out/releases/0.23.0-04b4985fdbe8/Mixworks.exe`, SHA-256 `04B4985FDBE87855562E6534E97C012D6D2A4F6576229BF3E24A9FC692D118FA`; `Mixworks.cmd` now targets it. Prior releases and the launcher backup remain available.

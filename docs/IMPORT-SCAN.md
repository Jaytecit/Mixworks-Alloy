# Music scan failures and reports

The Music scanner includes subfolders and accepts WAV, AIFF, FLAC, Ogg and MP3. **Tools / Choose Music scan folder...** selects one folder tree. **Use Windows Music folder** restores the default. Previously saved tracks remain in the Library when the scan folder changes.

Alloy opens the saved Library, checks remembered tracks in the background, and looks for new files in the chosen tree. Automatic scanning runs when the workstation is idle. After the initial pass, Windows folder notifications trigger further checks. A periodic pass also reconciles the tree. Unchanged tracks reuse their saved data from file stamps. Changed files or analysis records are checked before reuse.

**Checked** counts supported files seen during the pass, including unchanged tracks and tracks already queued for the startup source check. **Added** and **refreshed** count live Library updates. **Saved** counts Library rows committed to disk. Hover over the notice for analysis reuse, audio processing, failures, and pending source checks. The scanner saves in small batches, including completed tracks from a stopped pass. Library additions do not require saving the mix.

The first launch with the new store imports `library.json` into `library.sqlite3` in the same local app-data folder. The original catalogue stays on disk. The new database writes changed tracks independently and keeps the path index separate from each track's snapshot. The 128 MiB guard still applies to reading the original JSON file. The active Library has no 128 MiB total limit.

From 0.55.63, the notice separates **failed imports** from **cache warnings**. A failure means no library entry was added/refreshed. A cache warning means audio loaded but an analysis or decoded-audio cache operation reported a problem. Failed refreshes keep the existing entry. Source music is never deleted.

**Tools / Music scan reports...** opens the latest report from this process, or reveals the reports folder after reopening. Reports are timestamped local text files under `%LOCALAPPDATA%/Mixworks/ScanReports`; each issue includes its source path and exact reason. Bridge/isolated scanner roots use their own report directory. Report-write failure remains visible in the scan notice's details.

**Tools / Scan Music folder now** retries failed files. Valid unchanged analysed entries are skipped. Automatic passes remember unchanged failures for the process lifetime to avoid repeatedly decoding the same failing file every minute. Changing a source/analysis stamp or relaunching also permits a retry.

## 24 October MP3 repair

JUCE's reader estimated the duration of MP3s without a Xing/Info frame count by dividing file size by the first frame size. At 320 kbps / 44.1 kHz, frames alternate between 1044 and 1045 bytes. The estimate could request samples beyond the audio ending and reject an otherwise valid file.

The reproduced Tipper file has 14,028 complete frames, but the old reader advertised 14,040. Its original full read failed; the same reader with the measured length decoded successfully. A complete independent FFmpeg decode also succeeded. A read-only catalogue inventory found 579 absent supported files, all MP3s of this affected header/bitrate/rate configuration. All 517 FLACs in the Music folder were catalogued.

The repository-owned `Source/JuceMp3Length.cmake` / `.inc` patch counts complete frames for files lacking the header count, excludes standard trailing ID3v1/APEv2 tags and zero padding, and rejects incomplete/broken frame structure. Existing Xing/Info handling is retained. It does not ignore a decoder failure, replace the ending with silence or apply an arbitrary trim.

No analysis revision, session format or PCM cache-key revision changes are needed. No production analysis, corrections, audio cache or music is cleared. Valid cached data is reused; a genuinely different decoded source length still requires a correctly sized playback source.

Evidence is retained under `.local/scan-warning-20261024/` and `.local/import-recovery-20261024/`. Focused verification reads representative short MP3/FLAC fixtures and the reproduced real file; it does not decode or analyse the full library as a test. The user-controlled next launch of the staged update retries the missing imports through the normal scan.

## Recovery checkpoint, 0.55.64

A read-only audit initially found all 579 absent from the saved snapshots and no production scan reports. A missing-list-only recovery then imported **7 MP3s with 0 failures**, including full fresh analysis and normal cache writes. Fresh analysis took tens of seconds per source. The user explicitly chose **"Checkpoint and stage for later"**; recovery stopped cooperatively between sources, committed the seven results, and left **572** for a later user-controlled launch. This confirms progress, not recovery of all 579.

All 918 original snapshots are exactly unchanged; there are now 925 snapshots. The raw file list went from 926 to 933 paths (it already included eight paths without snapshots). All 961 protected baseline analysis/section/downbeat-reference JSON files are unchanged. No production data was cleared or running session controlled.

The normal scanner can continue on the next launch, or with **Scan Music folder now** if automatic scanning is off. Valid unchanged entries, including the seven completed imports, are skipped. The CLI `--recover-imports` supports an explicit missing-only manifest, per-file evidence and cooperative stop; it is not a test selector. Do not resume bulk recovery automatically after this checkpoint. Retained reports, remaining-path manifest, original catalogue and preservation audit: `.local/arp-20261024/`.

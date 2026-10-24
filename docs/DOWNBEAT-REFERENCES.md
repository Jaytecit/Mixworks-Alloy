# Saved downbeats and comparison references

Select a segment and use **↓1** in its Automation toolbar. The native accessible name and tooltip identify **Save downbeat to track**. At very narrow widths the same command is in the curve selector. Review an exact source-second position, optionally use the current playhead or IN transition marker, then **Save to track**. An IN marker can sit on any beat: use it as a downbeat only when you know it is beat one. Moving IN alone never changes the source grid. Cancel preserves the saved anchor.

Save replaces the source track's old manual downbeat, retaining BPM/key corrections. It does not move S or HC points. A reliable downbeat can lie anywhere in a constant-tempo 4/4 source; earlier/later bars extend that phase. Prepared audio retains its captured clock until Beatmatch/rebuild. Invalid positions, locks, stale dialogs, preparation and recording guard edits.

Automatic detection remains separate from the manual override. Each changed downbeat through this button, the source grid editor or waveform context command also appends an independent local **reviewed-choice** JSON snapshot in `%LOCALAPPDATA%/Mixworks/DownbeatReferences-v1`. It records verified audio SHA256/facts/path, automatic BPM/beat/downbeat/confidence/method and detector revision, chosen BPM/downbeat/key overrides, prior manual anchor, application version and UTC time. Every review is retained; later edits or reanalysis never replace earlier snapshots. Source audio and waveform/cache data are not copied or modified. Keep this reference folder when clearing disposable detector analysis.

The snapshot represents the user's reviewed choice. It is written before replacing the active source correction. If the subsequent analysis-record save fails, the active correction rolls back and the status explains that the reviewed choice remains archived. If comparison storage itself fails, the source overwrite is refused. Cancel/no-op/BPM-only edits do not create observations.

For a reference set of about20 tracks, save each corrected downbeat normally. References include repeated reviews, so compare by audio SHA256 and date rather than treating every file as a distinct song. Source anchor seconds alone are not an alignment error: anchors separated by whole bars may express the same phase. Unknown automatic downbeats are stored as -1. The records preserve your judgments as evidence, without automatically treating every choice as verified detector ground truth.

Export all observations to a new CSV:

```powershell
python scripts/downbeat-references.py --reference-dir "$env:LOCALAPPDATA/Mixworks/DownbeatReferences-v1" --csv .local/downbeat-comparisons.csv
```

Existing manual corrections can also be snapshotted using `--analysis-dir "$env:LOCALAPPDATA/Mixworks/Analysis-v1"`. Add `--include-uncorrected` to preserve automatic baselines. Capture reads analysis only and never changes it; duplicate identical captures are skipped, newer/malformed records are left alone and exports never overwrite an existing CSV. Capture dates are explicitly different from the unknown original edit dates.

See [section flags](SECTION-FLAGS.md), [testing](TESTING.md) and [handover](../HANDOVER.md).

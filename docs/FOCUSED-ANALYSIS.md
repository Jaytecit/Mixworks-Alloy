# Batch and focused analysis

In Library or Playlist, Ctrl-click toggles individual rows, Shift-click selects a
range, and Ctrl+A selects all visible rows. Right-click a selected row to keep the
selection and open its batch menu. Right-click an unselected row to target that
row alone. Search and sorting use source identities; repeated Playlist segments
of the same source are analysed once.

- **Retry weak BPM / key** runs a different, conservative secondary pass on
  selected sources with missing or weak automatic facts. Manual BPM, key and
  downbeat overrides are protected. A manual downbeat also protects its pulse
  and BPM. Results with strong evidence are skipped.
- **Re-analyse selected tracks** explicitly reruns the normal detector. It can
  give exactly the same answer. It preserves manual corrections and clears the
  completed secondary-attempt marker, allowing a later focused retry.
- **Select tracks needing analysis** selects the existing broader review set,
  including outdated analysis and unknown downbeats. A focused retry targets
  weak BPM/key fields within that set, including a missing bar anchor while
  protecting an accepted stable pulse.

Batch processing runs in the background with progress and Stop. It requires
stopped playback and no pending timeline/tempo preparation or export. Completed
results survive cancellation; incomplete work is not published. The completion
message distinguishes improved, unchanged, skipped and failed sources. A better
evidence score is not proof of a correct musical answer. Uncertain tracks still
need review. Hover a library cell to see the last focused outcome.

## Best guesses and retry revision 3 (0.55.64)

Fresh analysis now also retains the strongest tentative tonal candidate and a
4/4 phase when acceptance thresholds abstain. Timeline/library display can use
these best guesses without promoting their confidence. Manual key/BPM/downbeat
choices remain authoritative. Silence or a lone pitch can still lack a useful
key estimate. Compatible existing analysis remains intact; explicitly analysing
or retrying a selected source adds new candidates. Older completed focused
retries may run once again under revision 3. An unchanged accepted estimate can
still gain a tentative candidate; the completion counts describe accepted
estimates, not confidence in the musical result.

## Secondary method

The default detector remains analysis revision 11. The secondary pass is explicit
and source-bound, with optional independent revision/outcome fields in the
existing analysis record. It does not invalidate other tracks, delete caches,
change the session format, or automatically reanalyse the library on startup.
This selective retry does not apply the global detector-development reset.
Reviewed downbeat reference files remain independent and untouched.

Tracks over 30 seconds use three passages of up to 20 seconds centred at
22%, 50% and 78% of the source, shifting positions relative to the normal pass.
Shorter sources retain the normal bounded passage schedule. For uncertain BPM, both low-band
and mid-band percussive proposals are considered when the low-band fit is weak.
The existing neural/physical consistency checks arbitrate the result; a previous
model-unavailable failure selects the physical path instead of requesting the
same unavailable model again. SoundTouch BPMDetect adds a bounded independent
comparison on these passages, retaining raw estimates and direct/half-double/
disagreement evidence. No integer-BPM rounding, label lookup or forced half/double-tempo choice
is used. BPM is adopted only with a non-variable coherent grid, evidence at
least .35 and .05 above the previous score, residual at most 20 ms, and no loss
of an existing bar anchor. Its pulse and downbeat evidence move together.

For key, five-frame time medians and 17-bin frequency medians form a harmonic
spectral mask before the existing tuning-aware chroma/profile analysis. This
suppresses short broadband percussion without generating stems. The technique
is based on [FitzGerald's median-filter separation method](https://www.audiolabs-erlangen.de/resources/aps-w23/papers/2010_FitzGerald_HarmonicPercussiveSep_DAFx.pdf);
the implementation operates directly on spectra and does not resynthesise audio.
Numerical residue below a frame-relative floor is excluded and quiet-register
normalisation is bounded. Otherwise masking can turn negligible FFT residue
into narrow peaks with disproportionate chroma votes. On multi-passage sources,
at least two independent passages with good tonal evidence may support a key
when they carry at least 75% of the eligible weighted votes. Stereo mid/side
observations of one passage count together, not as independent agreement.
Key retries target unknown keys or scores below **.55**, matching the library's
review flag. Adoption requires at least .15 for unknown/same keys, .30 for a
different previously identified key, and improvement of at least .05. These
thresholds are conservative engineering guards, not calibrated probabilities.
Silence, noise, a single pitch, or conflicting evidence may remain unchanged.

Each completed field attempt is remembered for that source identity and method
revision. Running the same retry again skips it rather than spending time on a
deterministic repeat. Changed audio cannot reuse the attempt. A later method
revision can retry again. Failed/cancelled work does not become a completed
attempt. BPM and key decisions are independent: useful key evidence survives a
failed tempo retry. Source-bound JSON and `AnalysisRetries` text reports retain
before/candidate/result values, method/passages, prior failure and per-field
reasons. Strong-but-wrong automatic results cannot be identified from confidence
alone: use the source BPM/key editor for those cases.

Existing mix positions, captured tempo maps, prepared audio and Undo/Redo clocks
are retained. If an automatic BPM improves, Beatmatch explicitly applies it to
the existing mix; fresh additions can use the new facts. Original music is never
rewritten. Verification results and real-music/listening limitations are recorded
in HANDOVER.md and the independent local evidence directory.

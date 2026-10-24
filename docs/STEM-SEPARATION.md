# Local stem separation

Right-click a song in Library or Playlist and choose **Split track…**. The
separate progress window offers **Cancel** and, when validation finishes,
**Open stem folder**. Closing the window leaves the job running; choosing Split
again shows the active job. One job runs per application. **Split track on CPU…**
selects CPU explicitly; the normal command uses CUDA when available and falls
back to CPU for a CUDA allocation/device failure. Quitting cancels and reaps the
owned worker.

The four float WAVs are drums, bass, other and vocals. They have the original
decoded source's sample rate, channel count and frame count, with source sample
zero at output sample zero. Mono stays mono. No individual peak normalisation,
trim, BPM correction or leading-silence removal is applied. Inputs are limited
to mono/stereo, 8–192 kHz, ten minutes and 512 MB of decoded float PCM.

After separation, choose **Use on timeline** to attach the validated set to the
placed instances of that source. The stem sum replaces the original audio before
the parent track's effects and transition processing. **Play original track**
provides an A/B switch. Original files, analysis and reviewed downbeats are kept.

Adding that song from Library (or dropping it onto the timeline) again now checks
the completed stem cache in the background and restores a matching validated set.
This works after restarting the app and does not need the separation runtime or
run extraction again. New segments start with neutral stem controls; existing
segments keep their mix settings. Missing, changed or damaged assets are not
attached; the original track remains available. Saved mixes still use their
explicit asset references and are not silently replaced from the cache.

Click **Stems** inside a segment to expand/collapse its four rows. Each row has a
**Solo** button: it silences other timeline sections, including overlapping
sections on the same lane. Clicking another stem's Solo switches exclusively to
that stem; clicking the active Solo again restores the mix. Enabling Solo also
enables stem playback and unmutes that stem. Each change has one Undo. The row
menu offers the same **Solo only this stem (whole timeline)** action.

Stem Solo takes precedence over lane Solo; parent levels, automation, effects
and lane Mute still apply. It is a saved mix control, so exports also follow it.
Outside the soloed segment's time span, the other sections stay silent. Switching
to **Play original track** bypasses that segment's stem controls, including Solo;
switching back restores them. Separation quality and the stem files are unchanged.

Click a row's label for level/pan, mute, or volume/pan automation. Automation uses the
existing timeline node editor (including precise entry, curve Read and Undo).
The Editing console's owner selector also lists each stem. The selected stem's
curve uses the parent lane's editing area. Only volume and pan are independent
stem effects in this version; the parent EQ/filter/VST chain processes the sum.
Stem curves use authored timeline nodes; live Touch/Latch recording remains
available for parent controls only.

Stems share the parent's sample-zero clock, trim, reverse, loop, audio slip and
tempo map. Splits slice stem automation with the parent; moves and tempo changes
retain source timing. Stems never undergo independent beat/downbeat detection.
Session 38 saves asset identity and stem mix/automation, reading older sessions
as original-only playback. Reopening validates original and stem identities;
missing or changed assets reject the open and keep the current mix. Keep the
external stem folder with sessions that use it.

The lower console starts with tabs only. Click a tab to open its panel, and click
the active tab again to collapse it. **Mix timeline** mixes the placed songs in
chronological order, trying section-based treatment and falling back to editable
crossfades for short/unusable section transitions. **Auto Arrange** still chooses
songs from the library. **Beat grids** contains source review and grid settings.

## Runtime and storage

The optional runtime is installed with `python scripts/setup-stems.py` from the
repository, using Python 3.12 on Windows. It downloads public dependencies and
one model; music never leaves the computer. Installation is explicit and
separation itself is offline. The app reports a missing or changed runtime
instead of downloading inside an audio operation. Setup backs up an existing
runtime configuration and writes only `StemRuntime-v1.json` in the app data
folder. It does not clear analysis or change sessions.

Pinned backend: maintainer Demucs 4.1.0, commit
`2883f3db65617d6d178c6ed10d869dc14e44e59b`, Torch 2.6.0 CUDA 12.4, four-source
HTDemucs `955717e8` in safetensors format, upstream model revision
`cbc8a9b1a87023b7fd74e7b3412e6321c0eab003`. Setup verifies the full model SHA-256
against that revision's Git-LFS identity, retains package versions and copies
the worker by its SHA-256. Each job checks the model, worker and runtime versions.
Settings: one deterministic shift (seed1729), 7.8-second segments, 25% overlap;
Julius0.2.7 resamples to44.1kHz and back. A stereo-aware normalisation avoids
erasing antiphase input, and the source DC mean is shared across four outputs.

Completed sets live in `%LOCALAPPDATA%/Mixworks/Stems-v1`, independently of
`Analysis-v1`, corrections, PCM preparation, library snapshots, recovery and
`DownbeatReferences-v1`. Development bridge and self-tests inherit their
isolated analysis parent instead. Keys bind the original content hash, exact
decoded PCM hash, runtime/model and settings. The manifest records dimensions,
offset0, per-stem hashes, device and elapsed inference time. Renaming a file does
not invalidate identical content. Changing its content, decoder or separation
settings does.

Every cache reuse validates all four hashes, sample dimensions and finite PCM.
An incomplete or damaged set refuses reuse. There is no automatic eviction of
completed sets or cleanup of user data. Free-space guards reserve space for
the input, four outputs and working headroom. Cancel/failure removes only that
invocation's UUID working folder after the child exits; a crashed application
may leave an incomplete folder below `Stems-v1/jobs`, which is never Ready.

Before publication the worker also compares the four-stem sum with the decoded
source and rejects relative RMS error above15% (or residual above1e-7 for near
silence). It does not force the sum to match by hiding residual audio in a stem.
This guards gross failures, not individual stem leakage. Sparse synthetic
impulses and a two-tone antiphase fixture failed this bound in qualification and
are deliberately refused. Ordinary music, quiet music, mono and silence have
separate measurements; unsupported results never become Ready.

Publication is a same-volume directory move after worker validation and a
second original-content hash check. A source change, failed worker, low disk,
invalid model or concurrent publication leaves the current mix unchanged.
Assets remain external source-time files. Analysis revision11 remains compatible;
reviewed downbeats stay authoritative in their existing stores. Fresh analysis
also retains tentative key/bar candidates when stronger acceptance criteria
abstain. These are displayed as best guesses without raising their confidence.
Existing analysis is not invalidated or erased; an explicit selected-track
analysis/retry can add new candidates. Focused retry revision2 allows older
completed retries to run once under this policy. Silence and non-tonal material
can still have no meaningful key candidate.

## Qualification

[Benchmark driver](../scripts/benchmark-stems.py) declares exact sample-clock,
one-sample summed alignment and15% relative recombination-RMS bounds before
measurement. Reports include cold wall time, inference time, observed process
RSS, allocated CUDA peak, disk size, stem levels and recombination residual.
These metrics do not establish vocal isolation or absence of cymbal/bass
artifacts. Real music without independently recorded reference stems cannot
provide reference SDR. User listening acceptance remains a separate judgement.

See [the direction and qualification record](research/STEM-DIRECTION-20261023.md)
for measurements and limitations, and [licensing](LICENSING.md) before any
distribution. This local runtime installation does not clear public-release
prerequisites.

# Mixworks Alloy

Current development follows the [handover](HANDOVER.md), [outstanding roadmap](FINISHING-PLAN.md) and [documentation index](docs/README.md).

**Work-in-progress beta.** Official project: [Jaytecit/Mixworks-Alloy](https://github.com/Jaytecit/Mixworks-Alloy). Free for personal, non-commercial testing under the [Personal Beta Licence](LICENSE); commercial use, redistribution and rebranding require written permission. Original application rights are reserved; third-party licences remain applicable. **Public binary distribution is pending dependency clearance** described in [licensing](docs/LICENSING.md).

A local Windows mixing workstation built with JUCE, SoundTouch and local Beat This! analysis. **Alloy 0.55.133 beta includes Visual Studio 0.1.33 and 1,000 bundled Studio GLSL scenes.** It supports source-grid correction, editable audio transitions and a music-driven visual timeline. Exact candidate identity and focused verification are in the [handover](HANDOVER.md).

Run **Mixworks.cmd**, or the **Mixworks Alloy** desktop shortcut. They select the current immutable local bundle through `out/Launch.cmd`; [handover](HANDOVER.md) records its exact version/hash. The desktop shortcut uses the [Alloy launcher icon](design/alloy/alloy-launcher.ico). Keep adjacent runtime, model, notices and licences with each candidate. Updating the launcher does not restart an existing session. This local development preview is not a public binary release or complete qualification.

## Making a mix

1. Import audio or enable background Music-folder scanning. Library search, artwork, sorting, note/Camelot keys and confidence labels describe stored evidence, not accuracy probabilities. Tools / Music scan reports separates per-file import errors from cache warnings. Valid source-bound analysis and prepared audio are reused. See [loading and analysis](docs/LOADING-AND-ANALYSIS.md) and [import reports](docs/IMPORT-SCAN.md).
2. Check BPM, pulse timing and bar phase before relying on alignment. Constant-tempo 4/4 music is the detection priority; a kick alone does not identify beat one. Source review offers audition, fine BPM/key/downbeat correction, source-only save and explicit save/rebuild. Focused retries protect accepted/manual fields and retain SoundTouch evidence. See [timing limits](docs/RHYTHM-TIMING-REVIEW.md), [focused analysis](docs/FOCUSED-ANALYSIS.md) and [protected downbeat references](docs/DOWNBEAT-REFERENCES.md).
3. Add tracks as independent timeline placements. Ordinary add/drop creates no transition. Use **Add Transition** for an eligible overlapping pair, or an explicit arranging command. Frames move/resize independently within shared audio, with optional outside muting. Audio lanes support range/multiple selection, split, slip, reverse, loop, clipboard and ripple operations. See [timeline editing](docs/TIMELINE-EDITING.md), [range editing](docs/RANGE-EDITING.md) and [bar navigation](docs/BAR-NAVIGATION.md).
4. Select a transition for Volume/Bass/Mid/High treatments, swap gaps, filters, lock, A/B/C rehearsal and named presets. Section hints are measured suggestions, not semantic verse/chorus recognition. Add creates one automation node without invented empty-curve endpoints; only the selected node shows its value caption. Precise entry retains physical units. See [preset nodes](docs/PRESET-NODES.md), [transition presets](docs/TRANSITION-PRESETS.md), [filters](docs/TRANSITION-FILTERS.md) and [keyboard controls](docs/TRANSITION-KEYBOARD.md).
5. Use Editing, lane controls, the Controller desk or mapped MIDI. Original-track controls and VST curves share one owner across split pieces and gaps; separate placements remain independent. Piece geometry/fades stay local. Read/Touch/Write and arming determine automation capture; Record/Perform captures a distinct take. See [live desk](docs/LIVE-DESK.md) and [mode workflow](docs/ARRANGE-RECORD-PERFORM-PLAN.md).
6. Save a `.mixworks` session and export stereo PCM24 WAV with the selected ending/tail. Master processing, labels/tracklists, recovery and monitor mute are available. Monitor mute affects device monitoring, not the recorded/exported programme. See [mastering](docs/MASTERING.md), [export](docs/EXPORTING.md) and [recovery](docs/RECOVERY-AND-MONITORING.md).

## Arrange and Record/Perform

**Arrange** provides editing, save, playback and offline arrangement WAV. **Record/Perform** Play starts an independent editable take; Pause records held source time with silence/effect tails, and Stop finalises it. The original arrangement/plugin state remains independently available. Takes / preview offers audition, cancellable seek reconstruction, event editing/removal, discard and take WAV export.

Private library/track/grid/whole-transition preparation owns independent transport and DSP. Apply or quantized Launch explicitly commits the named target; stale targets reject. Select a separate device or independent stereo pair while stopped. Audience channels 1–2 cannot overlap private output. Missing/failing headphones never fall back to audience output. Headphones are optional for recording a take. See [preview routing](docs/LIBRARY-PREVIEW.md).

Optional live master WAV captures programme after master processing and before monitor mute, excluding private audio. FIFO/write/flush failures preserve a labelled partial file while programme and editable capture continue. Live instrument notes, opaque plugin state/topology changes and nondeterministic plugins need WAV for an exact audible record; editable note recording is not implemented. Limits are 32 takes, 65,536 events and 64 structural snapshots. Long RF64 recording has not been qualified. [Implementation contract and acceptance limits](docs/ARRANGE-RECORD-PERFORM-PLAN.md).

## Auto Arrange

Auto Arrange creates or extends a mix using eligible source grids, known keys, adjacent harmonic compatibility, an 8% BPM bound, valid placement and no repeats. Choose a starting song for a new arrangement, count or approximate duration up to 180 minutes, and optional genre/energy preferences. With an existing mix the dialog defaults to **Extend current arrangement**; enter additional songs/minutes. Create new arrangement replaces the mix. Preparation/cancellation/failure keeps the current mix until a successful commit; one Undo restores it.

Seven blend styles are available: **Balanced**, **Smooth**, **Bold**, **Use my settings**, **Constant power**, **Hard cuts only** and **DJ set (varied)**. Generated treatments are editable nodes. Balanced, Bold and DJ set can use measured bass and section changes to coordinate faders, EQ and filtering. Smooth uses a plain volume fade. Use **Blend timeline tracks** to apply the shared Balanced pair planner while retaining track order.

**Energy direction** offers Unrestricted, Keep pace, Build energy and Wind down. Rhythm, rhythmic bass, transient dynamics and tempo guide soft preferences, allowing plateaus and necessary reversals. Recording RMS is not the activity score. Genre is also a soft preference; missing evidence and departures are reported. There is no automatic gain change, vocal/drop classifier or calibrated danceability claim. The planner is a bounded heuristic, not an exhaustive best-order search. Listen through results. See [Auto Arrange](docs/AUTO-ARRANGEMENT.md).

## Visual Studio

Open **View → Open Visual Studio** or use the button on the visual timeline.
With empty lanes, Studio reacts to the live master and offers **Random scene**
and **Random MilkDrop**. Add Background, MilkDrop, GLSL and Sprite clips to
schedule a show. **Match Mix** creates an editable schedule from track handovers,
section changes and measured energy or activity. Right-click a GLSL or MilkDrop
clip to replace it at its existing time. Press **F11** to toggle fullscreen.
[Visual timeline guide](docs/VISUAL-SCENES.md) · [Studio controls and authoring](visual-studio/README.md)

## Stems

Right-click a Library/Playlist song and choose **Split track…** for optional local four-stem separation, progress and Cancel. Runtime installation is explicit; separation is offline. **Use on timeline** attaches validated drums/bass/other/vocals WAVs. Expanded stem rows provide level/pan/mute, whole-timeline stem Solo and authored volume/pan automation. The parent EQ/filter/VST chain processes the sum. No independent stem EQ/VST or live stem automation writing is offered.

Stems share the parent source clock, edits and tempo map. New placements can reuse a matching validated stem cache without the runtime; saved sessions retain explicit external asset references. Keep the stem folder with sessions. Missing/changed saved assets reject opening while retaining the current mix. Numerical recombination checks do not establish perceived isolation or musical quality. See [setup, controls and limits](docs/STEM-SEPARATION.md).

## Workspace, audio and controllers

Choose two, three or four visible audio lanes in the timeline toolbar. Hidden audio lanes still play. Overall timeline resizing divides the height evenly across visible lanes. Individual lane edges adjust the current space and survive width-only changes. Editing/Controller/Transitions panels dock, collapse or detach; workspace preferences remain outside musical Undo. See [layout](docs/TIMELINE-LAYOUT-049.md) and [named workspaces](docs/WORKSPACE-LAYOUTS.md).

The toolbar BPM follows the arrangement map at the playhead. Later tracks added during playback get future source-BPM targets after the protected existing-audio/manual-marker horizon. Source review saves do not silently rebuild existing prepared clocks. Hotcues jump source time while the programme clock continues. See [hotcues](docs/HOTCUES.md).

Audio / MIDI distinguishes detected, connected and recently receiving inputs. Reload saved mappings validates active assignments; Learn/Cancel is shared across settings and the desk. No profile/hardware remapping is installed automatically. Audio settings expose installed Windows backends including compiled ASIO; physical driver/MIDI/headphone acceptance remains unverified. Mixworks installs no driver. Callback-work/silent-skip counters and supported driver xruns aid diagnosis; the earlier reported breakup has no established root cause.

VST3 hosting is in-process with stereo main buses. Active inserts reporting nonzero latency are blocked; full delay compensation, crash isolation, sidechains, multi-class hosting and VST2/32-bit bridging are not offered. External plugins are trusted native code. Live instruments accept MIDI notes; exact audible recording requires live master WAV. See [latency limits](docs/PLUGIN-LATENCY.md).

View offers saved Popup hints, grid brightness, Attract lighting and Molten sparks / forge playhead. Forge motion defaults to Follow Windows, with Animated/Steady choices; Escape exits attract lighting. The stereo PEAK panel has independent peak hold/clipping reset and master gain. See [popup hints](docs/POPUP-HINTS.md), [mastering](docs/MASTERING.md) and [design](docs/DESIGN.md).

## Qualification and limits

The [handover](HANDOVER.md) identifies the current launcher and links evidence for the Studio preview, visual timeline and recent audio workflows. The [13 November functional audit](docs/AUDIT-20261113.md) records wider application findings. Auditing does not resume bulk recovery, reset analysed data, overwrite reviewed references, or restart a running session. Historical 60/90-minute fixtures and prior full receipts do not qualify this candidate. See [capacity](docs/SOURCE-CAPACITY.md) and the [audit status](docs/release-audit/STATUS.md).

General real-music beat/downbeat/key accuracy and performance/fidelity thresholds remain unresolved. Listening, physical FLkey/ASIO/private routing and clean-environment package acceptance need separate evidence. Variable-tempo source detection, other meters, compressed export, editable MIDI-note recording, sequencing, built-in synth, phone controller and audiovisual integrations are outside current implemented scope. Outstanding work is tracked in [the roadmap](FINISHING-PLAN.md).

## Development and verification

Use Visual Studio 2022 C++ tools, Windows SDK, CMake, Git and Python 3.11+. `scripts/build.ps1 -TestPlugin` builds the app and Gain/Delay/Latency VST3 fixtures with one build worker, MSVC `/MP1` and below-normal priority, then restores caller settings. It fetches pinned dependencies without installing toolchains or changing the launcher. Keep builds/tests sequential; stop owned work if actual responsiveness worsens. No numeric free-memory threshold blocks builds. Read [testing policy](docs/TESTING.md) first.

JUCE is pinned to `ea77444750de45e036e0279bb0291144bf8e3688` (8.0.16-security), SoundTouch 2.4.0 to `d994965fbbcf0f6ceeed0e72516968130c2912f0`; model/runtime pins are in `scripts/analysis-dependencies.json`. Exact compiled dependencies and the guarded JUCE MP3 patch are separate audit inputs.

```powershell
python scripts/check-project.py
python docs/release-audit/refresh_inventory.py --check
```

Choose focused checks for changed behaviour; native checks use `scripts/verification.py:run_checks` for below-normal execution, isolated roots and generated-fixture retention. No automatic broad regression/stress/capacity run during iterative repairs. At the explicitly authorized final checkpoint, freeze source/bundle/dependencies, then run `scripts/verify-analysis.py --exe <frozen-exe> --output <fresh-evidence-dir> --receipt <receipt.json> --include-stress` under the explicit verification policy. All 23 named suites and required supplemental workflows need evidence. A native receipt does not establish listening, hardware, clean-package or distribution acceptance.

Deployment requires the exact receipt through `scripts/deploy.ps1 -VerificationManifest <receipt.json>`; `-VerifyOnly` validates without staging. Scoped receipts cover only their declared change. Staging does not restart the app or grant permission to publish. [Verification and source recovery](VERIFICATION.md) owns receipt requirements; [development bridge](docs/DEVELOPMENT-BRIDGE.md) describes isolated workflow control.

`Mixworks.exe --analyse "<absolute-audio-file>" ... --report "<absolute-report.json>"` produces local analysis evidence. An absolute `.mixworks` startup argument opens a session; absolute audio arguments import tracks. Audit/maintenance commands must use owned isolated stores.

## Project map

| Location | Purpose |
| --- | --- |
| [HANDOVER.md](HANDOVER.md) | Current continuity, preservation and evidence |
| [docs/README.md](docs/README.md) | Current guide index and dated research/history |
| [docs/release-audit/README.md](docs/release-audit/README.md) | Rules, coverage, execution and blockers |
| [FINISHING-PLAN.md](FINISHING-PLAN.md), [WISHLIST.md](WISHLIST.md) | Acceptance questions and future requests |
| `Source/`, `tests/`, `scripts/` | App, test plugins and verification tooling |
| `design/`, `.impeccable/`, `licenses/` | Design references and third-party notices |
| `.local/` | Ignored dependencies, independent source archives and local evidence |
| `build/`, `out/` | Ignored build outputs and staged releases |

Use this folder's Mixworks-Alloy Git repository, not parent REA. Preserve source/evidence independently of Git and keep music, sessions, caches and generated bundles out of commits. Historical plans are context, not instructions to resume old operations.

New mix saves and mix-related exports default to **Desktop / Mixworks**. Change or reset this in **File → Default save folder**; Ctrl+S continues saving the current file. See [save/export defaults](docs/EXPORTING.md#default-save-folder).

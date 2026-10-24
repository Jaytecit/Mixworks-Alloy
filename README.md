**0.55.47 stems:** right-click a Library or Playlist song and choose **Split track…** for local four-stem separation with progress, Cancel and validated reusable WAVs. [Setup, limits and workflow](docs/STEM-SEPARATION.md); [measured speed/alignment and listening limits](docs/research/STEM-DIRECTION-20261023.md). Existing analysis and reviewed downbeats are preserved. Timeline stem mixing remains later work.

**0.55.46 interface:** compact Editing, short curve labels and plain feedback. See [popup hints and guide](docs/POPUP-HINTS.md).

# Mixworks Alloy

**0.55.43 editing:** [Range tool and multi-selection](docs/RANGE-EDITING.md) add direct audio intervals and scoped Copy/Cut/Delete/Paste. Analysis11/session34 and saved downbeat references are retained. Group drag and partial linked-transition deletion remain later work.

**Work-in-progress beta.** Official project: [Jaytecit/Mixworks-Alloy](https://github.com/Jaytecit/Mixworks-Alloy). Free for personal, non-commercial testing under the [Personal Beta Licence](LICENSE); commercial use, redistribution and rebranding require written permission. Original application rights are reserved; third-party licences remain applicable. **Public binary distribution is pending dependency clearance** described in [licensing](docs/LICENSING.md).

A local Windows mixing workstation built with JUCE, SoundTouch and local Beat This! analysis. **Local release: 0.55.46 beta, staged for the next normal launch. Analysis revision: 11. Session format: 34.** Four real mixer lanes support audio, inserts, automation, pan, DJ cutoff, mute/solo and MIDI. Older builds cannot open newly saved format34 sessions.

Run **Mixworks.cmd**. The launcher selects `out/releases/0.55.46-e764e0d1c94f/Mixworks.exe`. Keep the adjacent runtime, model, notices and licences with the application. Previous releases remain available for rollback; newer sessions may require the newer app.

The node/preset audit removes invented empty-curve endpoints and duplicate neutral guards; Add creates one requested node. Selected swaps expose vertical amount controls on both songs, and RMB pairing retains sharp cuts. The Library panel collapses like the inspector; RMB menus open at the cursor; new-track NONE is explicit; BPM/key sorting can use the highlighted library source as a similarity reference. See [preset nodes](docs/PRESET-NODES.md), [editing](docs/TIMELINE-EDITING.md) and the [DJ.Studio editing/stem investigation](docs/research/DJSTUDIO-EDITING-STEMS-20261022.md). Stem separation remains future Alloy work.

Playing hotcues now queue source jumps for one segment while the programme clock continues. The Library shows track totals and persistent recursive Music scan progress/results. See [hotcues](docs/HOTCUES.md) and [library scanning](docs/LOADING-AND-ANALYSIS.md). Private Performance routing and discrete event recording remain unfinished.

Editing now brings automation owner, parameter, node actions and Read/recording state together. Filter and FX share explicit insertion routes; browsing leaves the mix unchanged and right-click offers node actions. See [automation in Editing](docs/TIMELINE-EDITING.md#automation-in-editing-05524).

Automatic Sections now aligns paired measured changes and puts Bass swap at their common boundary, using a complete-bar frame. A genuine match can extend beyond the preferred overlap limit. See [automatic section transitions](docs/TIMELINE-EDITING.md#automatic-shared-section-transitions-05522). These energy hints remain suggestions for listening review.

Attack timing now follows the beginning of distinct kick envelopes. Snapped movement retains an existing manual alignment correction; hold Alt to change that alignment freely. See [movement and manual alignment](docs/TIMELINE-EDITING.md#preserve-manual-alignment-when-moving-05521).

Beatmatch now has clear source-only save and save/rebuild actions, durable save-error handling, +/-0.01 BPM fine tuning, Library/Playlist right-click access, beat/half/quarter snapping and twice the maximum timeline zoom. See [Beatmatch corrections](docs/TIMELINE-EDITING.md#beatmatch-corrections-and-fine-snapping-05514). Rebuilding can replace manual transition placement; source-only saving retains the prepared timeline clock.

The header handle now moves the selected box and following tracks with its incoming transition fixed; Shift-drag retains audio-only slip. Live cutoff knobs consistently control their lane even when an older clip is selected.

Clip controls now live in the lower detachable **Editing** console. VST3 inserts stay on the right with **Keyboard** and **Audio / MIDI...** access; see [Editing](docs/TIMELINE-EDITING.md#editing-console-05511).

## Making a mix

1. Import audio or enable background Music-folder scanning. The remembered library supports search, sorting, artwork and note/Camelot keys. Notes use compact labels: `Gb` means Gb major, `Gbm` means Gb minor; hover over a key for the full name. The adjacent **CONFIDENCE** column shows **1 No key**, **2 Low**, **3 Good**, **4 Excellent**, or **Manual**; these describe detector evidence rather than accuracy probabilities. Key labels have no question-mark suffix. Search accepts compact or full key names; switching key display updates the key sort immediately, with unknowns last. Existing valid analysis and prepared audio are reused. See [loading and analysis](docs/LOADING-AND-ANALYSIS.md).
2. Check the BPM, beat pulse and bar phase before relying on automatic alignment. Use source-grid audition and manual correction when needed. Constant-tempo 4/4 music is the current supported detection priority; a kick alone does not identify beat one. See [detection findings and limits](docs/RHYTHM-TIMING-REVIEW.md).
3. Add tracks to the timeline. The default Crossfade uses **Sections 8–64 bars**, preferring aligned energy-section changes on both tracks, then one; missing hints fall back to an eight-bar grid overlap. Edit the mix tempo map, position tracks, slip audio within a fixed clip, and use linked IN/OUT alignment and transition frames. The ruler and overview remain fixed while navigating. See [timeline editing](docs/TIMELINE-EDITING.md), [layout and resizing](docs/TIMELINE-LAYOUT-049.md) and [bar navigation](docs/BAR-NAVIGATION.md).
4. Select a transition to adjust length, Volume/Bass/Mid/High treatments, swap gaps and filter sweeps. Lock a pair to protect it, compare A/B/C rehearsals, and save named presets. **Pair bass swap** in the timeline right-click menu gives the clicked receiver the opposite manual bass ramp from its partner. See [transition presets](docs/TRANSITION-PRESETS.md), [paired handles](docs/SWAP-HANDLES.md), [filters](docs/TRANSITION-FILTERS.md) and [keyboard controls](docs/TRANSITION-KEYBOARD.md).
5. Use the Controller desk or lane headers to mix. Record captures control automation; Mix / Automation and the timeline right-click menu contain curve commands. Advanced modes and arming are in Recording options. See [live desk and FLkey guide](docs/LIVE-DESK.md).
6. Save a `.mixworks` session and export WAV. Master processing, spectrum analysis, labels/tracklists, recovery snapshots, monitor mute and bounded effect-tail export are available. See [mastering](docs/MASTERING.md), [export](docs/EXPORTING.md) and [recovery](docs/RECOVERY-AND-MONITORING.md).

Popup help is enabled by default. Use **View > Popup hints** to save it on or off; see [popup hints](docs/POPUP-HINTS.md).

Audio / MIDI now shows callback-work and silent-skip counters plus driver xruns when supported; see [playback diagnostics](docs/RECOVERY-AND-MONITORING.md#playback-diagnostics-05510). The earlier breakup remains unconfirmed.

## Workspace and audio

The slider beside Snap dims the timeline grid and remembers its brightness independently of the mix. Double-click restores the subtle default. Wheel over the timeline to zoom; View / Fit whole mix and overview navigation remain available.

All four mixer lanes stay visible with their complete lane controls. The stretch grip beneath the tempo footer is always available, including while Controller or Transitions is docked. Docking limits expansion to preserve panel space. Drag individual lane boundaries to share the existing area; lanes cannot collapse away their controls or cause vertical scrolling. Home/double-click resets the timeline height. Layout preferences remain separate from musical sessions and Undo.

The toolbar BPM follows the actual arrangement tempo at the playhead during playback, including ramps. Later tracks added during playback receive their own future source-BPM targets after the protected existing-audio/caller-marker horizon; existing voices retain their timing. Stopped marker editing and in-progress text/drag edits remain protected.

The transition header keeps its step counter and adjacent bar count. Click an automation line to reveal its nodes; selected nodes have larger grab targets, Left/Right navigation, precise Enter editing and undoable Delete. The selected-value caption stays clear of editable dots and handles; cutoff nodes show LP/HP frequency or Off. Precise entry keeps physical units. See [direct curve selection](docs/TIMELINE-EDITING.md#direct-curve-selection-0555).

Snap, mute and solo switches capture one Undo step through the same engine boundary for mapped MIDI and desk controls. Toolbar/menu Snap shares that path. Held/repeated button messages do not add extra steps; mapped Undo/Redo works with Snap without changing controller assignments.

**The controller/settings button** in the toolbar distinguishes detected, connected and recently receiving inputs, with full input names in its tooltip. When no input is detected it reads Audio / MIDI. Click it for settings, where **Reload saved mappings** validates and restores the saved assignments without rewriting the profile or changing the mix. The live desk header shows the active mapping title (or Unnamed mapping / Default); the settings list retains the active profile path/count. Settings guided setup and Map live desk share the current layout and Learn state; Cancel in either window ends pending learning everywhere. See [controller status](docs/LIVE-DESK.md).

**Audio / MIDI** exposes Windows audio types and **ASIO**. Choose an installed driver and its channels/buffer there. ASIO4ALL v2 and FL Studio ASIO were enumerated during 0.53 verification; physical ASIO playback and latency have not been verified. No driver is installed by Mixworks.

Resizing the overall timeline divides the height evenly across all four lanes, within one pixel. Individual lane-edge adjustments apply until the next overall resize; width-only changes retain them.

The desk's paired EQ/cutoff lane selectors address all four lanes, excluding their partner. Its level/pan faders, reset pads and lower mute/solo pads keep their existing lane 1–4 assignments. Cutoff is centre Off, left low-pass and right high-pass; double-click/Home resets it. Old low-pass MIDI action IDs load as aliases; the user explicitly cleared its assignments for diagnosis. Clear mappings in Audio/MIDI backs up and empties the active/saved profile; no remapping is installed automatically. Live VST instruments accept MIDI notes; automation Record is distinct from audio/MIDI-note recording.

**View / Molten sparks / forge playhead** enables optional meter sparks and the molten overlap seam. **View / Forge motion** defaults to Follow Windows; choose Animated explicitly to see the effect when Windows animations are disabled. The responsive stereo PEAK panel has independent L/R peak hold, clipping reset and master gain. All enabled dials, sliders and meter fills heat toward maximum. See [metering and motion controls](docs/MASTERING.md).

**View / Attract lighting**, or **Ctrl+Shift+A**, animates workspace lighting; Escape exits. Display/window controls and the approved visual language are described in [design](docs/DESIGN.md).

## Auto Arrange

The highlighted **Auto Arrange** button between zoom and **Audio / MIDI** creates a new arrangement. Choose a random or specific starting track, then a track count or target length in minutes, up to **180 minutes**. The available compatible library tracks also limit the result; tracks never repeat. The duration target is approximate and keeps whole tracks.

Tracks need current analysis or corrections with usable BPM, downbeat and a known analysed/manual key. A low key evidence score does not exclude an otherwise usable track; that score is not a probability. Each next track must match the preceding key on the existing Camelot matcher and stay within 8% BPM. Choose a **Blend style**:

- **Balanced** (default): bass handovers and gentle filter sweeps, guided by changes in the two tracks' measured energy.
- **Smooth**: simple volume fades.
- **Bold**: tighter bass handovers, stronger filters and extra tone fades.
- **Constant power**: a volume blend that avoids the usual halfway power dip for similarly loud, unrelated sources. It does not normalise recordings or guarantee equal perceived loudness.
- **Hard cuts only**: switch songs at the handover midpoint with a short click-prevention ramp; no full-frame crossfade, bass swap or filter sweep.
- **Use my settings**: the Transitions panel's current preset and length.

Auto Arrange keeps blends within **32 bars**, including when using your settings. Automatic styles find suitable energy boundaries within 8–32 bars. Balanced/Bold bass and filter treatments **replace** the default volume crossfade. Smooth, or missing/unsuitable energy evidence, uses a plain volume fade. These are energy-based choices, not vocal or verse/chorus recognition; listen through the result. Existing saved mixes keep their treatments when reopened; regenerate a mix to use the corrected replacement behaviour.

Choose **Energy direction** separately: **Unrestricted** keeps ordinary BPM/key matching; **Keep pace** rejects source-energy or BPM drops; **Build energy** requires at least a 1 dB increase without a BPM drop; **Wind down** requires at least a 1 dB decrease without a BPM rise. The source RMS power in each half of the actual placed overlap must qualify. Missing/silent energy evidence cannot establish a rating, and a constrained mix may finish early with a reason. These are measurements of the source audio, not perceived loudness or a calibrated dance-energy score. Existing mixer gains, effects and mastering still affect the audible result.

Constant-power curves are saved explicitly in **session format 26**. This version opens earlier supported sessions, including format 25, and retains analysis revision 9. Older apps reject format 26 rather than silently changing its curves; save an edited mix as a separate file if you need the older original.

A completed Auto Arrange enables animated attract lighting and forge sparks. **Escape** exits attract lighting; the **View** menu controls both effects. Menus, dialogs and file pickers share the workspace's panel colours and text styling.

Pause playback and finish pending preparation first. The button reads **Arranging...** and the progress panel stays visible while choosing tracks, loading sources and preparing the mix. A completion dialog shows the placed/requested counts and length, with an explicit stop reason when the target cannot be reached. The last report also stays on the button's tooltip. Generation offers cancellation and replaces the arrangement only after successful preparation, keeps mixer/plugin settings, and clears old arrangement automation and labels. **Undo** restores the previous mix. Save the result as a separate session. A missing or changed source, cancellation or preparation failure keeps the current mix. Existing library analysis, manual corrections, caches and saved files are preserved.

## Supported scope and open work

- Analysis runs locally. Constant source tempo, pulse alignment and bar phase are separate facts; an available downbeat prediction does not prove musical correctness. Variable-tempo source maps, other meters, commercial key parity and independent beat-one truth remain open. Eight reserved recordings remain untouched.
- The 60/90-minute numerical render/reopen and long seek/insertion checks are historical accepted evidence; listening, real driver underruns, current public dialogs and full physical FLkey acceptance still need review. See [capacity](docs/SOURCE-CAPACITY.md) and [long navigation](docs/LONG-NAVIGATION.md).
- Large sources use temporary mapped PCM; persistent analysis/preparation caches are reused where compatible. The source/decode capacity and separate preparation allocation limits are documented in the capacity guide. Dedicated playback prefetch remains future work.
- VST3 hosting is in-process with stereo main buses. Active inserts reporting nonzero latency are restricted; full delay compensation, plugin isolation, sidechains, multi-class hosting and a VST2/32-bit bridge are unfinished. See [latency scope](docs/PLUGIN-LATENCY.md).
- MIDI-note/audio recording, sequencing, private headphone cue output, Smart DJ, stems and audiovisual lanes are unfinished. The dancer panel, Synesthesia visualiser integration and controller-detection notification are **wishlist only**. Exact MixMeister DSP and `.mmp` compatibility remain research.

## Development and verification

Use the existing Visual Studio 2022 C++ tools, Windows SDK, CMake, Git and Python 3.11+. `scripts/build.ps1 -TestPlugin` builds the app and Gain/Delay/Latency VST3 fixtures. It fetches pinned project dependencies without installing toolchains or changing the launcher. JUCE is pinned to `ea77444750de45e036e0279bb0291144bf8e3688` (8.0.16-security); SoundTouch 2.4.0 to `d994965fbbcf0f6ceeed0e72516968130c2912f0`. Model/runtime pins live in `scripts/analysis-dependencies.json`.

```powershell
python scripts/check-project.py
python -m unittest discover -s tests -p test_verification.py -v
```

For an application change, freeze the executable with its runtime/model, then run `scripts/verify-analysis.py --exe <frozen-exe> --output <fresh-evidence-dir> --receipt <receipt.json>`. All **22 named suites** must pass. Deployment requires the exact schema-2 receipt through `scripts/deploy.ps1 -VerificationManifest <receipt.json>`; add `-VerifyOnly` to validate without deploying. [Verification and source recovery](VERIFICATION.md) owns the full commands and evidence requirements.

`Mixworks.exe --analyse "<absolute-audio-file>" ... --report "<absolute-report.json>"` produces local analysis/cache/timing evidence. An absolute `.mixworks` startup argument opens a session; absolute audio arguments import tracks. Native self-test suites must run in separate processes, with isolated report/fixture directories.

## Project map

| Location | Purpose |
| --- | --- |
| [HANDOVER.md](HANDOVER.md) | Current release, continuity, evidence and cleanup record |
| [FINISHING-PLAN.md](FINISHING-PLAN.md), [WISHLIST.md](WISHLIST.md) | Remaining acceptance gates and recorded feature requests |
| [docs/README.md](docs/README.md) | Current guide index, research and historical notes |
| `Source/`, `tests/`, `scripts/` | Application, real test plugins and verification/research tooling |
| `design/`, `.impeccable/`, `licenses/` | Approved design references and redistribution notices |
| `.local/deps/` | Pinned local build dependencies |
| `.local/source-baselines/`, `.local/` | Independent source backups and retained verification evidence |
| `build/`, `out/` | Regenerable build outputs and deployed releases |

The prototype remains untracked in the parent REA repository. Preserve source independently of Git, along with local evidence and the user's original audio, sessions and AppData. Historical plans and research are context, not instructions to resume old work.

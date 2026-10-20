> Dated investigation evidence. Earlier capabilities, failures and proposed work below are historical; the root HANDOVER.md identifies the deployed build and current scope.

# MixMeister feature comparison

Discovery audit, 2026-10-17, against Mixworks v0.22.0. These are findings and suggested priorities, not newly implemented features. Beat/downbeat and stretching reliability still come before automated mix generation.

**Current reconciliation, 2026-10-18:** use [the completion plan](../../FINISHING-PLAN.md) for current status and delivery order. The v0.28 implementation added clip-owned VST3, EQ, volume/pan and source-relative automation, including movement/split/copy behaviour; the song-ownership gap and lane-only plugin descriptions below are historical. Current session format is 13. Named treatments, master inserts, preview routing and source drift correction remain future work. Preserve the table as the original discovery record.

## Gaps worth addressing

| Feature | Current Mixworks boundary | Reference / suggested order |
| --- | --- | --- |
| Visual source-grid correction and audible click | One source BPM and first beat/downbeat can be corrected. There is no editable sequence of source measure anchors, grid audition metronome or tap-BPM control. A global mix-tempo curve cannot correct drift within a recording. | [Manual][manual], pp. 61, 93–97; **core reliability**. |
| Song/segment effects and song-owned editing | Plugins belong to mixer lanes. EQ/volume/plugin curves and labels use absolute session time; moving a song leaves them behind. Anchor fades already follow their source cues. Add song ownership while retaining intentional lane/master automation. | [Manual][manual], pp. 69–70, 92, 109; **core editing**. Exact Fusion move/split behaviour needs a UI test. |
| Reusable named song treatments (“Tweaks”) | Ctrl+W currently opens analysis settings. No named collection of a song's edits, effects and cues, or default treatment for future insertion. Analysis corrections already persist separately. | [Manual][manual], pp. 109–111; next editing stage. |
| Saved transition recipes, including EQ | Cut and selected overlap lengths exist, but no editable recipe library. This expands the existing automated-transition-presets wish. | [Fusion feature page][fusion]; after song-owned automation. |
| Private preview and library audition | One transport/output path. Double-clicking a library item appends it; there is no separate audition player or headphone bus. | [Numark guide][controller], dual-output section; useful for preparing and performing mixes. |
| Live loop capture/release | Current loops have a fixed repeat count; RMB start/end replaces the selected clip with two passes. No ongoing loop capture released during playback, with the resulting performance retained. | [Manual][manual], pp. 14–15, 58–59; partial parity, not complete. |
| Selected-range editing with ripple options | Clipboard operations act on whole clips. Split first is possible; direct range copy/replace and gap-closing cut are absent. | [Manual][manual], pp. 82–83; editing workflow. |
| Pitch transposition independent of tempo | Key detection, compatibility highlighting and metadata correction exist. Audio pitch remains unchanged; changing the key label does not transpose audio. | [Manual][manual], p. 65; harmonic mixing. |
| Parent-linked samples and a launch shelf | Ordinary overlapping clips work. No primary/overlay relationship, parent-relative movement or sample launch shelf. | [Manual][manual], pp. 86–88; remix/live workflow. |
| Compressed and divided mix exports | Render is one stereo 24-bit WAV at the device rate. Labels and text/M3U output exist, but no MP3 render, label-based audio splitting or CD authoring. | [Manual][manual], pp. 98–100; [Fusion][fusion] confirms MP3/CD on the product page. |
| Library organisation and recommendations | Search, sorting, artwork, key highlighting and Music-folder scanning exist. No categories, editable title/artist catalogue fields, similar-song results or smart mix generation. | [Manual][manual], pp. 24–39; [Fusion][fusion] advertises smart playlists. Extend existing ordering/Smart DJ wishes. |
| Live transition commands | Play/pause, stop and seek exist. No immediate-next-transition command or fade/pause/cue-next workflow. | [Numark guide][controller]; live-performance stage. |

The most consequential design gap is song ownership: several songs can reuse a lane, so an EQ move intended for one song can affect another after rearrangement. This conclusion follows from our source model, not a newly observed Fusion interaction. Source beat anchors should likewise belong to the source analysis, independently of the mix's tempo points.

## Further findings and limits

- **Recovery candidate:** installed Fusion contains an abnormal-exit playlist-recovery prompt. Mixworks has explicit-save backups and a close confirmation, but no periodic recovery snapshot. The prompt establishes a shipped recovery feature surface; its recovery coverage and behaviour have not been exercised.
- **Mixer capacity:** Fusion advertises eight simultaneous songs; Mixworks has four independently processed lanes. This is not a four-clip playback limit: overlapping clips can share a lane and its effects.
- **Master processing:** the manual describes whole-mix effects; Mixworks currently hosts plugins on lanes only. A master insert chain would also support finishing a mix. The manual's named bundled compressor dates from 2009; availability in the installed edition is not established, and this is not evidence of LUFS normalization.
- Four-bar shading, transition anchors, RMB commands, basic loops/reverse, MIDI control and automation already exist. Their existence is separate from matching every Fusion interaction.
- Automatic musical-phrase recognition, exact automatic cue selection, detector accuracy and stretch quality are **not established** by this feature audit. Preserve the existing real-track validation limits.

## Evidence and verification

The official support site serves a 132-page Fusion Mac manual under its 7.8 manuals article; the document itself carries a **2009** copyright. Page numbers above are printed numbers (PDF page = printed page + 4). Platform-specific export details and current Windows behaviour require separate verification. The official Fusion product page is available in the search index but its direct URL returned 404 during this audit; treat it as historical product documentation.

Retained REA evidence from installed Windows Fusion 7.7.0.1 corroborates measure editing, saved tweaks, overlays, smart-playlist and recovery surfaces:

- `.local/evidence/fusion-playlist-labels.json`, evidence `ev_30d1c683f6caa59994a4c5818bbf775250c172b843b84d46a6794f53a3309d65`: strings at `0x5f6d14` (measure creation), `0x5f7324` (saved measures), `0x5f6f26` (overlay), `0x52e78c` (smart-playlist dialog) and `0x5f5394` (recovery prompt). Static strings do not prove successful execution.
- `.local/evidence/fusion-track-settings.txt`: prior UI capture exposes key adjustment, primary/overlay style and overlay beat sync.
- `.local/mixmeister-feature-audit/audit.json`: downloaded manual identity, SHA-256, URLs and audit scope; PDF and extracted text retained beside it.

Compared documentation with `Source/Analysis.h`, `Model.h`, `Engine.h/.cpp`, `Main.cpp`, `Workstation.h/.cpp`, `TimelineEditing.cpp`, `Beatmatch.cpp`, `ElasticAudio.cpp` and library interfaces. No app code changed, no new DSP tests, no claim of exhaustive parity or fresh Fusion runtime verification.

[manual]: https://support.mixmeister.com/en/support/solutions/articles/69000867963-mixmeister-7-8-macos-manuals
[fusion]: https://www.mixmeister.com/fusion.html
[controller]: https://www.numark.com/images/product_downloads/mixmeister_control_quickstart_guide_v1.1.pdf

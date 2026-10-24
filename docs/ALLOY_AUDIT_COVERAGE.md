# Alloy audit coverage and interface inventory — 21 October 2026

Read with [findings](ALLOY_PRODUCT_AUDIT.md) and [repair batches](ALLOY_REMEDIATION_PLAN.md). Target: native Windows **0.55.12**, analysis 9, session 27; exact candidate/source hashes in [identity.json](../.local/product-audit-20261021/identity.json).

Coverage terms:

- **R** — operated/observed in the native application during this audit. A visible response is distinguished from a completed persisted/audio outcome.
- **F** — fresh production-component suite on this executable through `verification.run_checks`; no physical hardware/listening inference.
- **S** — source/control/state path inspected. This is not a runtime pass.
- **H** — historical exact-candidate gate, not rerun here.
- **G** — verification gap, including available functions not fully exercised or combinations not exhaustively sampled.
- **N/A** — incompatible tool/platform or absent/deferred feature, with a reason.

This is a whole-product inventory and representative audit, **not an exhaustive control-combination test**. A row with R does not mean every control in that family was exercised; the last column states exactly what remains.

## Framework, integrations and existing capabilities inspected first

| Capability | Established evidence | Decision |
|---|---|---|
| Application framework | CMakeLists.txt:2,5–13,22,62; native JUCE 8 GUI/C++17, SoundTouch, local ONNX analysis, VST3 hosting; `JUCE_WEB_BROWSER=0` | Native UI, source and audio tests are the appropriate primary tools. No web frontend/package.json was established. |
| Impeccable 4.3.1 | Installed SKILL.md, context loader, critique, audit.native and audit references read | Two isolated agents after user authorisation; Windows adaptation. See assessment A/B and run notes in the findings. |
| Windows runtime automation | Installed Computer Use skill and guidance/API/confirmation policy; `@oai/sky` windows, UIA, screenshots, click, drag/key/value APIs | Used existing runtime; no custom PowerShell UIA, helper protocol or new automation installation. |
| Native harness | Main.cpp CLI/self-test modes, MusicTests dispatch, `scripts/verification.py`, test artifacts retention, TESTING.md, current gate/receipt | Reused exact executable. Fresh fusion-workflows/export suites; historical coverage separately marked. |
| Source tools | PowerShell/Python, rg, Git rooted at Mixworks, native source, 374-line source-binding/label index | Targeted reads and static mapping. [Source index](../.local/product-audit-20261021/control-source-index.csv) is supporting search evidence, not 374 runtime tests. |
| Playwright/browser tools | Browser automation exists in Codex, but the product has no compatible web interface | N/A for operating Alloy. No fabricated browser route or web overlay. |
| axe-core / Knip | axe requires a DOM; Knip targets JS/TS module/dependency graphs | N/A for the native C++ product. No installs. Native UIA/tree inspection supplies useful accessibility evidence but not a full screen-reader test. |
| REA integration | Tools discoverable; native observation tool described Mach-O target binding | Not selected for this Windows source-available app. Native Windows Computer Use already supplied compatible interaction. No reverse engineering was needed. |
| skills.sh | Directory accessed before choosing method | No additional skill was necessary; installed native runtime/harness was the smallest compatible approach. |
| Audio measurement | Python standard-library WAV decoding, hashing and sample measurements | Verified real export format/duration/energy/clipped samples. No new audio package installed. Physical listening/loopback remains a separate gap. |

The essential product capability gap is semantic access to painted musical content, not a missing browser package. The smallest useful addition is a JUCE/Windows accessibility object/cell model inside Alloy. For driver/dropout acceptance, add a bounded loopback/output capture to the existing diagnostics when that work is taken up; do not install a broad new web-testing stack. For usable text-heavy automation, resolve the observed JUCE text-injection/chord limitation in the existing helper before proposing another native automation library.

## Product state and persistence owners

| Owner | User-facing material | Implementation / route | Audit |
|---|---|---|---|
| Project | Name, clips, source references/manual overrides, tempo, links/treatments/alternatives/presets/locks, labels, clip/lane/master automation, mixer/master settings, plugin descriptions/state | Model parse/serialisation; Engine capture/prepare/commit/save; File/Open/Save | R/F/S/H. Two saved ten-track documents differ only by selected input gain; reopen retains it. Unused media admission defect AUD-003. |
| Workspace | Panel widths/heights, four lane shares, tool page, Editing detach/bounds, follow/grid/hints, monitor mute, forge preferences | WorkspaceLayout/WorkspaceUI/PerformanceUI; View, dividers, F11, panel hexagons | R/S/H: 1920×1080, 1440×810, Editing/Transitions detach and native return. No DPI or monitor-unplug acceptance. |
| Source corrections/analysis | Automatic revision-9 facts, manual BPM/key/downbeat; source identity/cache compatibility | AnalysisCache/GridCorrection/LibraryWorkflows; BPM/Key, Tweaks, Re-analyse | R/S/H: inspected actual source dialog, middle navigation/audition/Cancel. No correction committed; no detector development/reset. |
| Audio preparation | Decoded/prepared PCM and tempo cache | PcmCache/ElasticAudio/Beatmatch/Engine; import/drop/open | R/F/S/H: current analysis reused during open, real WAV output; broader cold/warm timing not benchmarked. |
| Library catalogue/scan | Searchable remembered sources/assets; scan settings and progress | LibraryCatalogue/MusicScanner/LibraryAssets/Workstation | R/F/S/H. Existing 359-entry library; fresh M3U worker import/cancellation in suite. No fresh full production scan requested. |
| MIDI | Startup snapshot, named profiles, current bindings/learn/setup/connection | Engine/MidiConnection, Audio/MIDI and desk Learn | R/S/H read-only status and restart comparison. AUD-013 records initial active/startup discrepancy. No profile write or device programming. |
| Recovery | Per-window UUID lease, checksummed current/previous snapshots, source retention | Recovery/RecoveryUI; File > Recover unsaved mix | S/H. Normal close/save exercised; genuine crash/previous-generation recovery not injected. Paused-only snapshots are documented, not continuous-performance protection. |

## Menu-to-command map

This maps actual dispatched commands rather than treating every internal helper as a missing UI feature. Source: `WorkspaceUI.cpp:6–135`, `BarNavigation.cpp`, `AutomationUI.cpp`, `AppWindow.cpp`. Menus were inspected in source; Beatmatch and its problematic commands were directly operated.

| Menu/surface | IDs/routes and actual owner/outcome | Coverage / issue |
|---|---|---|
| File | 1 New → unsaved guard/newSession; 2 Open → guard/prepare/commit; 3 Save → chooser/save; 71 Recover; 4 Add files; 29 playlist import; 5 WAV export; 30 source M3U; 31 text tracklist; 45 Exit → close guard | R/F/S/H. New/save/reopen/export/close and Cancel observed. Tracklists/playlist exports covered fresh F. Actual File Recover, failed save and arbitrary paths remain G. |
| Edit | 6/7 Undo/Redo; 8 Split; 9 Duplicate; 10 Remove; 11 Reverse; 12 Loop; 88 reset selected clip cutoff; bar-move submenus | R/S/H: lock/Undo, gain persistence, split/Undo, master Undo. Broader edit commands mapped and historical production tests; fresh combinations are G. |
| Mix | Selected automation submenu; 73 effect ending; 14 tempo marker; 15 Snap; 32–36 labels/edit/remove/previous/next; 16/17/18 play/stop/home; bar jumps | R/F/S/H: transport, WAV ending prompt and suite label workflows. No fresh manual tempo-ramp or label navigation sequence. |
| Beatmatch | 13 rebuild; 25 source corrections; 60/61/62 reanalyse selected/needing/all; 63 select needing; length IDs 40–45 | R/S/H: wrong-source dialog, length→Quit; source audition. Global rebuild/reanalysis not run over retained user library. AUD-001/002. |
| View | 86 hints; 85 suggested energy sections; 72 master/spectrum; 81 forge; 82–84 motion; 70 desk detach; 74/75 Transitions; 76 Controller; 77 attract; 78–80 attract motion; 19/87 Editing; 20 VST; 21 keyboard; 22–24 fit/zoom; Display submenu | R/S/H: fit, F11, keyboard, master, Editing/Transitions detachment and compact layout. Visual policy matrix, all detach combinations and arbitrary window bounds are G. |
| Tools | 26 Audio/MIDI; 27 Add VST3; 28 assign plugin controls; 37 scan now; 38 auto scan; 39 stop scan | R/F/S/H: inspected settings/devices/profiles/diagnostics; worker import/cancel through fresh F. Physical MIDI, scan interruptions and third-party plugin setup are G. |
| Display button/submenu | 1000 fullscreen; 1001 maximised; 1002 windowed; 1003–1006 size presets; 1007 span; 1100+ monitor choice | R/S/H: F11 fullscreen→1440×810 recreates peer; rebind required in automation. Spanning/hot-unplug/high-DPI are G. |

ID 45 is the confirmed harmful collision. Standard alternate toolbar/menu/shortcut routes are useful and should share an owner, not be removed merely because they are duplicated. Save uses different default/current-file behavior across routes (AUD-007), and correction targeting differs by preceding selection (AUD-002).

## Interface and interaction inventory

| Surface / interactive elements | Command → state/outcome | Evidence / remaining gap |
|---|---|---|
| Header and document toolbar | Display, fullscreen/windowed, minimise, close; New/Open/Save/import/export; Undo/Redo/Split; Beatmatch; Auto Arrange; controller status opens settings | R/S/H. Responsive toolbar hides lower-priority buttons into existing menus. No header version yet (planned P49); no Help entry. Minimise not exercised. |
| Transport/display | Return-to-start, Play/Pause, Stop/rewind, time readout/ruler seek, BPM slider/exact entry, Snap, grid brightness, Energy/Detail/Peaks, Follow | R/S/H. Playback, stop, seek, fit and F11 observed. BPM/ramp/brightness/mode/Snap combinations source/historical, not fresh manual audio acceptance. |
| Library | Library/Playlist tabs; text search; BPM/Key; key-format selector; resizable/sortable/reorderable ART/TITLE/TIME/KEY/BPM/CONFIDENCE columns; scrollbars; multi-row selection; double-click append; drag description/file drops | R/F/S/H. Selected real library/playlist songs and source path were verified. Native suite imports actual owned M3U/audio and tests cancellation/deduplication. Manual multi-select/drag/drop/search text paths were not completed in this run. AUD-006/009. |
| Playlist | Track, Transition In picker, lane/start/length/art columns; selection selects clip/lane; row sorting; multi-selection for supported library/reanalysis actions | R/S/H. Ten rows, second-track owner and all nine Custom/edited labels observed. Picker has no incoming transition on first row and disables for locks/preparation. Multiple selected rows do not imply multi-clip editing is implemented. |
| Lane headers ×4 | Lane selector, mute, solo, cutoff, pan, volume/meter; lane-edge resize | R/S/H. Selection follows song, cutoff lock state observed; detailed routing inspected. Volume/pan/EQ ownership differs from cutoff/mute/solo (AUD-005). No physical live-mix/auditory route acceptance. |
| Detailed timeline | Clip body selection/move/lane move; clip bounds/trims; source IN/OUT cues; transition frame/corners/song grips; forward-suffix hexagon and Shift slip/Alt snap bypass/reset; fade/swap handles; source ticks/sections; wheel zoom | R/S/H. Actual selection, cursor, whole-mix fit, split/Undo. Historical timeline suite has 660 checks covering forward move, linked clocks/anchors and production PCM. Full real-arrangement dragging/trimming stress and alternate modifiers remain G. |
| Timeline context menu | Cut/Copy/Paste/Delete; Volume/Treble/Mid/Bass points; pair bass swap; automation; label/tempo/effect markers; loop start/end; split/reverse; Tweaks/BPM/key; Editing; set downbeat here | S/H; popup paths traced to performTimelineAction. No empty placeholder handler identified in this review. Fresh individual context actions, clipboard combinations, invalid loops and deletion/Undo remain G. |
| Overview/ruler/tempo footer | View rectangle pan, click centre, double-click fit; ruler seek/double-click play; label add/move/edit/remove; tempo marker select/drag/numeric edit/remove; keyboard navigation | R/F/S/H. Fit and ruler seek operated; labels/tempo alignment freshly exercised by F; historical timing/navigation tests. All long seeks/ramp combinations not rerun. |
| Editing console | Start, Source in, Length, Fade in/out, input gain; Reverse, Loop; tab, scrolling body, detach/attach and native close | R/S/H. Selected input gain −6 dB saved/reopened, native detach retained value, native close reattached. Fields disabled without selection/locked. Numeric values remain stale but disabled after New, with an explicit select-a-clip message; recorded as a minor observation, not data corruption. |
| Transition defaults | New-track Align IN/OUT, Bass swap, Crossfade; Cut/4/8/16/32/Sections 8–64 lengths | R/S/H. Defaults/panel inspected; source generator consumes them. Menu route broken only for Sections (AUD-001); panel's own selector has a separate valid handler. Not every insertion mode was regenerated. |
| Selected transition | Previous/Next, length, apply preset, Lock; Volume/Bass/Mid/High modes, Show/Only curve, strength, gap, swap position; filter mode/depth | R/S/H. Generated first-pair settings inspected, lock/Undo verified. No fresh audible comparison of every band/filter/depth/gap treatment. Existing native/model/PCM evidence is distinct. |
| Rehearsal & presets | Disclosure; before/after 0/1/2/4/8 bars; Store/Hear/Apply A/B/C; Prepare/End; saved session presets; save/delete/import/export | R/S/H. Store A → prepare/Hear → advancing transport/levels → End → previous cursor paused. B/C, named preset exchange and loudness listening not freshly verified. Comparison is RMS attenuation, not LUFS normalization or headphone cueing. |
| Automation | Selected curve Read, point at playhead, Clear, choose VST parameter; Off/Touch/Latch, lane/master arming; node reveal/select/drag/exact units/Delete; Record desk shortcut | S/H plus R lock restrictions/gain controls. Source conversion/ownership traced. No new full Touch/Latch performance, write-return, point precision or reopened curves listening journey; keep these explicit acceptance work. |
| Controller desk | 8 pots (paired Bass/Mid/High/Cutoff); 8 lane-selector pads; 4 mute/4 solo pads; 4 level +4 pan faders; master; 4 EQ resets/4 cutoff resets/all-pan centre; Undo/Redo/Play/Stop/Snap/Record and 2 disabled Spare pads; MIDI Learn/guided setup/Skip/Cancel; detach/scroll | R/S/H: status, labels, transport and mapping availability observed. Many parameter mappings/source paths inspected. Hardware-aligned alternate access is useful duplication. Physical pickup/jumps/reconnect/feedback and a full recorded performance are G. Spare pads intentionally disabled, explained, not broken handlers. |
| Audio/MIDI window | Audio driver/output/channels/rate/buffer/Test; MIDI input toggles; callback/xrun diagnostics; action Learn/cancel/setup; reload/clear; profile selection/open/save/delete/name/save-as/import/export and diagnostics | R/S/H: Windows Audio/Realtek/48 kHz/480 samples, enabled FLkey port, callback deltas, initial 50/reopened 0 profiles observed. No device/profile changes or test tone. UIA has mojibake in one fader tooltip; low priority, no functional claim. |
| Effects rack and keyboard | Add discovered/other VST3; list selection/double-click/Open; Bypass/Remove; MIDI parameter assignments; piano toggle, octave/channel note input | R/F/S/H: keyboard reveal and rack coexistence, selected owner labels inspected. Fresh F uses actual test Delay VST3, bypass/tails/cancel/close. No fresh commercial instrument, native editor, crash, arbitrary plugin automation or hardware note performance. Zero-latency restriction is deliberate. |
| Master output/spectrum | Separate header emblem/View action; six sliders (gain, low/mid/high EQ, width, saturation drive); saturation toggle; monitor mute; Reset tone/width and clip warning reset; post-master spectrum and stereo level display | R/S/H: inspected full window; EQ +3→Undo→0 verified. Real output meters during playback; no calibrated loudness or independent listening judgment. Monitor mute is explicitly output-only; multiple access points share its owner. |
| Progress/error/save/recovery dialogs | Import/open/reanalysis/auto/rehearsal/export progress; cooperative Cancel/Stop; unsaved Save/Discard/Cancel; file pickers/overwrite; export ending/tail; numeric/loop/label/grid/profile errors; recover candidates | R/F/S/H: save, New, close Save/Cancel, missing-source rejection, export ending, cancel/retry through native F. Discard, disk-full/unwritable and genuine crash recovery remain G. No user production file was deleted to force a case. |

Reverse reachability review found useful alternate access for editing, automation, transition, master, profile and export operations. No general set of intentionally public capabilities with entirely absent handlers was demonstrated. Internal parsing/rendering/model helpers are not counted as missing UI. A missing Help/About/version route and obscure suffix edit are recommendations; future P12/P22/P27/P36/P37 work is classified as planned rather than unreachable shipped functionality.

## End-to-end journey ledger

| Journey/state | Actual outcome | Coverage boundary |
|---|---|---|
| Discover already analysed music and inspect grid | Library selection → Playlist selection → Track BPM/key opened the wrong old source; source Middle/audition/Cancel worked | R defect AUD-002. No new ground-truth BPM/downbeat annotation, manual correction commit or reanalysis. |
| Import/analyse/find | Fresh native suite dropped an owned M3U through the real worker; deduplicated audio, preserved mix/labels, cancelled preflight safely | F 38-check suite; manual picker/cold neural analysis/search-and-refind not all replayed. |
| Arrange/inspect/correct automatic mix | Supplied untouched ten-track result, nine incoming treatments, panel states, lock/Undo examined | R. No fresh generator run or exact-duration claim; initial result was user-generated. |
| Edit musical material | Selected clip gain persisted; native split at 5.024 seconds produced pieces and Undo restored project; six Editing fields and broader code paths mapped | R/S/H. Not a fresh sample-identical split or every clipboard/ripple case. Full range/ripple editing remains P12. |
| Transition comparison | Store A → render/Hear A → advancing cursor/output → End restored zero paused | R. +0.8 dB right peak warning in modified copy documented with conditions; not original-mix analysis or listening. |
| Performance/editing switch | Lower tabs, same selection, Editing/Transitions detached, native Editing close reattached, compact scrolling and keyboard view inspected | R. No real two-monitor performance session, full automation take or physical control reconnect. |
| Save/close/reopen | Baseline saved; edited copy differs only in second clip gain; process exited; ten rows and −6 dB reopened | R complete persisted outcome. Named Save still opens picker. Selection/zoom resets are not misreported as lost musical data. |
| Empty/no selection | New cleared arrangement/Undo, disabled clip controls, displayed placement/select guidance | R. Stale disabled values and previous view scale observed; no false claim that they alter empty project audio. |
| Missing unrelated source | Two valid clips plus one unused nonexistent media entry rejected whole open; error acknowledged, app responsive | R; fixture never moved/deleted real media. Existing-project rollback is S/F/H, not this blank-instance runtime case. |
| Export and locate result | App picker → Last clip → render → Documents/a.wav; file independently decoded/measured | R: 20 s, 960,000 frames, stereo 48 kHz/24-bit, nonzero RMS, no clipped samples. Temporary audio removed after measurement; hashes survive. |
| Tail/cancel/retry/close | Fresh production export suite loaded actual Delay VST3 and checked exact PCM/tails, cancellation preservation, retry and late close/admission | F 69 checks. No new real-plugin tail listening, RF64-size file or disk-full runtime case. |
| MIDI startup/reopen | Initial in-memory 50 mappings versus pre-existing empty startup snapshot; reopened app displayed zero | R/S. Root cause uncertain; named profiles kept. No audit restoration or controller reprogramming. |
| Resize/accessibility | 1920×1080 and F11 1440×810; detached Editing/Transitions; native UIA trees of controls and custom surfaces | R/S. No Windows display-setting changes, multiple DPI classes, complete Tab order or screen-reader completion. |

## Keyboard/MIDI map reviewed

`Workstation.cpp:2292–2334`, `TransitionNavigation.cpp`, `TimelineAutomation.cpp`, `TimelineHeader.cpp`, `BarNavigation.cpp` and MIDI dispatch/profile code are the source inventory. Global edits guard focused TextEditor. Mapped controller actions share engine/undo owners for supported actions; actual physical acceptance remains separate.

| Family | Implemented route |
|---|---|
| Global | Space Play/Pause; Delete selected label/clip; Ctrl+S Save; Ctrl+O Open; Ctrl+Z / Ctrl+Shift+Z Undo/Redo; Ctrl+D Duplicate; S Split; F11 window mode; Ctrl+Shift+A attract; Escape exits attract/cancels loop start |
| Clip/context | Ctrl+X/C/V Cut/Copy/Paste; Ctrl+W Tweaks; Ctrl+T Editing; V/T/M/B automation points; E effect point; 9/0 loop bounds; P tempo marker; L label; [ / ] label navigation |
| Transition | Alt+Left/Right navigation; Ctrl+Alt+Left/Right frame move; Shift+Alt+Left/Right resize; Alt+L lock, plus documented band/swap shortcuts |
| Navigation/precision | Overview arrows/Home/End/+/-; lane Up/Down/Home/End; splitter arrows/Home and Shift precision; node Left/Right/Enter/Delete; relative dial drag, Shift fine, double-click/Home reset, focused wheel |
| Controller | Paired pot actions/selectors, four lane levels/pans/mutes/solos, master, EQ/cutoff/pan resets, transport/Snap/Record, transition navigation and learn/profile actions; MIDI clock transport where implemented |

F11 was directly exercised and caused expected peer recreation; the helper initially reported a lost foreground/process/window and then recovered on the new returned window ID. Text injection and some chords did not reliably edit JUCE text fields. Single-key filename entry and native mouse/value controls worked; long-path reopening used the application's existing CLI session argument. Those automation failures are **not** logged as keyboard defects in Alloy. Complete shortcut/focus/hardware coverage remains open.

## Evidence and retention notes

The evidence directory contains independent `source-checkpoint.zip`, `identity.json`, fresh suite reports/retention journals, A/B assessments, detector attempt, source index, UIA snapshots/screenshots, saved-state comparisons, fixture generators, export measurements and final retention/process manifests. Full-gate reports referenced from `.local/f512-g` remain historical.

Early 002–004 saved images reused the initial workspace because of a capture-wrapper state error; they cannot prove a modal and are excluded. Later `saveState(label, explicitState)` captures bind the intended window and tree. Native transient popup fragments may appear as additional PNGs; the main/full-modal image cited in findings is selected explicitly.

No full production reset, music edit, MIDI profile update, dependency installation, application source edit, build, deployment, Git commit or public binary release was performed. Fresh ordinary native suites used `run_checks` cleanup; manual test outputs were measured and exact-path cleaned after their process exited, with summaries/hashes/source/visual evidence retained. Current audit documents and evidence are not a distribution clearance decision.

## Outstanding acceptance that matters

The most important open work is: physical listening/output capture for lock skips and generated handovers; independent constant-tempo beat/downbeat truth; actual FLkey reconnect/pickup and chosen named-profile restart; ASIO/device changes; a full control-automation performance and reopen; arbitrary real VST editor/instrument/latency behavior; genuine crash recovery; disk-full/unwritable/Unicode paths; high-DPI/high-contrast/screen-reader journeys; and 60/90-minute mixed playback. These cannot be inferred from screenshots, detector output, simulated native callbacks or the historical gate. Use bounded cases and preserve retained analysis/caches rather than resetting them to diagnose UI/performance issues.

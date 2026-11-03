# Whole-project coverage and boundary map

1 November ua01: discovery registries refreshed against current source, adding
ear/retry tools, original-file streaming, clarity UI and visual runtime/planning.
MW-V01–V03 extend the rule catalog to97. [Current cases](inventory/current-cases.csv)
is the current acceptance ledger; historic counts below remain dated. See the
[execution checkpoint](UA01-CHECKPOINT-20261101.md) for source findings and limits.
Visuals now form an explicit subsystem: VisualScenes/VisualArrangement/SceneLane,
Python worker/compositor/presenter and image/transition persistence. Its new journey
is music → project image → complete automatic/manual scene schedule → handovers →
preview/output → save/reopen → late-scene pixels, under V01–V03/D01/U01/U05.
OriginalAudio belongs to audio; ear/retry review belongs to analysis. Generated
control rows are lexical discovery sites, not a count of distinct user commands.

25 October ra09: AUD-D14/D15 repair stale seek lifecycle completion and progress-window dismissal.95 lifecycle/error plus87 performance/replay/export/parser checks pass on the staged candidate. Original red89/9 includes7 application failures and2 corrected fixture errors. Evidence: `.local/ra09/`; broad engine regression, physical testing and whole-rule acceptance remain open.

25 October ra08: mode, capture-exclusion and seek controls add 107 focused checks on the staged candidate. AUD-D11/D12/D13 repair blocked/current-mode side effects, stale eligibility choices, malformed seek input, late Cancel and changed-mode completion. Red 107/10, initial fixture crash and intermediate 107/1 fixture failure retained. Evidence: `.local/ra08/`; full gate and whole-rule acceptance remain open.

25 October ra07: AUD-D10 repairs stale take-export targets across chooser and deferred-tempo completion.75 focused checks pass on the newly staged candidate, including five capture exclusions and event Remove/Undo. Original red60/7 and intermediate75/1 fixture failure retained. Case evidence: `.local/ra07/`; no full gate or whole-rule PASS.

25 October ra06: the named catalog now has23 suites, separating split-only finite-echo coverage from ordinary ownership. Complete invocation flags are reviewed/allowlisted.29 focused checks and31 harness tests pass; no revised full gate or whole-rule PASS. Case evidence and remaining conditional partitions: `.local/ra06/`. Application/launcher unchanged.

The authoritative item list is [inventory/files.csv](inventory/files.csv), generated from tracked plus nonignored untracked files in this repository, never the parent REA repository. Each row records purpose, release relationship, dependencies/consumers, rule IDs, method and exclusion rationale. Headers, `.inc`, build patch inputs, native tests compiled into the app, Python/PowerShell, JSON fixtures, licences, reference/design assets and documentation are included. A row means **accounted for**, not source-line-reviewed or verified.

Inventory generation rejects unclassified new items. Hashes detect changed bytes outside the framework; framework files are explicitly unhashed to avoid self-reference. CMake registration is independently checked by `scripts/check-project.py`. Vendor/ignored/generated treatment is in DEPENDENCIES. README/STATUS identify discovered baseline and contradictions. No first-party item is excluded from audit coverage.

## Subsystem requirements, consumers and methods

| Inventory group | Owners / release purpose / dependent boundary | Requirements and rules | Initial verification route |
| --- | --- | --- | --- |
| app | Main/AppWindow, startup flags/quit/display; constructs Workstation/Engine/dev controller | U01/U04/U05/U08, R01/R04/R05 | source dispatch, `window`, actual close/launch and clean package |
| ui | Workstation, WorkspaceUI/Layout, AlloyLookAndFeel, AttractLights, ToolPanel; commands/selection/layout feed model/engine | U01–U08, R11 | `alloy`, `window`, captures + real keyboard/mouse/accessibility |
| time | Model tempo/Clip, Beatmatch, Timeline*, Automation*, BarNavigation, Transitions*, EditSelection/Insertion, PresetNodes, HC/section UI | T01–T12, U01/U02, D01, A01/A04 | `tempo`, `placement`, `beat-grid`, `timeline-editing`, `track-ownership`, supplemental selectors; independent math/PCM |
| analysis | Analysis/NeuralRhythm/RhythmDecoder/GridCorrection/DownbeatReferences; normal/retry/grid consumers Library/arranger/playback | N01–N09, L06, R03 | `rhythm-reliability`, `beat-grid`, `grid-correction`, `rhythm-decoder`, `analysis-refresh`, `--targeted-analysis-only`; independent corpus |
| library | Model decoder, LibraryAssets/Catalogue/Workflows/Preview, MusicScanner/ImportRecovery, JUCE MP3 patch | L01–L06, N09, D04 | `library-assets`, `tempo-browser`, `fusion-workflows`, import-scan/preview/cache focused; independent decode |
| audio | Engine, ElasticAudio, Mastering/Spectrum/OutputPeaks/PeakMeter/PluginLatency, EffectTail and mapped PCM | A01–A11, T02/T03, X02, P09 | `core`, `playback-cache`, `mastering`, `latency`, `export`, timing/ownership; devices/listening |
| midi | MidiConnection/Engine/Model actions and UI; controller messages→active routing→audio/take | P01–P04, U01/U02/U03, D03 | core/Alloy native + MIDI connection functions mapped from SelfTest; physical FLkey/USB |
| performance | PerformanceDeck/UI/Mode/Take/Capture, LaunchClock/Pause/LiveHotcues, Preroll, PrivatePreparation, MasterRecorder | P05–P12, A03/A04/A07, D01, X02 | `--performance-take-only`, `--live-hotcues-only`, core/Alloy dispatch mapping, hardware private routing |
| data | Model JSON, AnalysisCache, PcmCache/PcmView, ContentDigest, Recovery; all musical/preference/cache owners | D01–D07, L06, R06 | current-format roundtrip/parser/fault and focused cache/recovery/native; no legacy migration matrix |
| stems | StemJobs/Mixing/UI, Python setup/worker/benchmark and stem manifests; source/cache→child→timeline/export | S01–S06, D01/D04, A01, X02, R03/R10 | `--stems-only`, worker harness, optional integration when qualified runtime present; listening |
| arrange | AutoArrangement/EnergySections/MusicalEnergy, PlaylistTransitions, TransitionStyles/Rehearsal/Context | O01–O05, N08, T07/T08/T12, X06 | `--auto-arrange-only`, `--musical-energy-only`, `alloy`, native section/transition/rehearsal tests; constraint/listening |
| export | WavExport/UI, Playlist and tests; renderer→file/backup→external decoder/player | X01–X06, A09/A10, D03 | `export`, `fusion-workflows`, independent wave/libsndfile/ffmpeg decode if available; listening |
| tooling | CMake/build/fetch/deploy/launcher, SelfTest/VerificationMetrics/PromoControl, all scripts/harness/tests | R01–R09/R12 and affected feature | targeted Python unittest, source manifest/bridge and build/receipt inspection |
| rights | LICENSE/notices/licenses, Source/ThirdParty BeatThis, ignored dependency assets | R03/R10, N01, S01 | exact SBOM/licence/patch/entitlement audit; no vendor-line certification |
| research | Auftakt feature/network/filterbank, RhythmResearch, tests JSON/probes/scoring/recovery/tonal dictionaries | N01, R03/R09/R11 | identify compiled reachability, provenance/independent reference; no claim this is normal production detector |
| assets | design concept text/boards, docs reference PNGs | R03/R11, U07 if selected at runtime | nonshipping design/history unless actual bundle selects them; provenance and visual check when used |
| docs | requirements/plans/history/audits/research guides | R11/R09, relevant feature IDs | dated claim reconciliation/link checks; documentation never acceptance evidence alone |
| framework | all files here incl helper/registries/templates | R12 | structural checks and explicit self-review; not a product suite |

Rule IDs in this table omit `MW-` for readability; the CSV contains full IDs. Cross-cutting headers/engine/model carry more obligations than a single filename grouping. `Source/Playlist.cpp` is exchange; `PlaylistTransitions.cpp` is arrangement policy. Source/StemMixing is audio; setup/worker scripts retain tooling obligations **and** S01–S03/R10 regardless of their inventory directory classification. First-party `.inc`/JUCE patch integration is not excluded merely because the target lives in a vendor directory.

## Complete user journeys

| ID / start | Concrete sequence and terminal oracle | Primary rules / required evidence |
| --- | --- | --- |
| J01 First local launch | Clean owned preferences, no device/media/model variants → import two independent known tracks → fresh analysis/review → place independently → explicitly link → play → save → graceful close/reopen → export/decode | U08,L01,L02,N01–N06,T02/T04/T07,A01,D01,X01/X02; real UI+PCM+listening |
| J02 Returning library | Warm catalogue with valid analysis/corrections + missing unused media → start → search/sort/review/add → compare no unnecessary decode → cancel/retry pending import | L03/L04/L06,N09,D04,R07; call counters/disk/state |
| J03 Source review | Library A/timeline B conflicting selection → correct named B BPM/key/downbeat/source section/HC → source-only save → verify disk/reference → explicit rebuild → reopen | U01,T11,N06,D01/D03; actual dialogs/PCM/reference identities |
| J04 Arrange/edit | Four lanes, overlapping independent tracks → split/slip/range/copy/paste/ripple/insert/Undo → Add Transition/frame/mute-outside/bands/filter → nodes/plugin → tempo edits/navigation → save/reopen/export | T01–T12,A01/A04/A09,D01,X02/X06; gestures+independent output |
| J05 Auto Arrange | Owned contrasting library → count/duration/start/style/energy/genre → replace/extend/cancel/insufficient supply → review decisions → A/B/C/store/apply preset → save/export | N08,O01–O05,T07/T08,X06; constraints+listening |
| J06 Controller performance | Configure intended physical desk map → Learn/Cancel/profile save/reload → actual faders/pots/pads/notes → automation Read/Touch/Write → disconnect/reconnect and restart → audible control/active layout | P01–P06,A07,D03; raw MIDI+physical/listening, no production map rewrite |
| J07 Stem workflow | Runtime preflight → Split/Cancel/retry → independently validate four assets → Use on timeline/A-B original → stem Solo/nodes/split/tempo → save/reopen → missing stem failure → export | S01–S06,D04,A01,X02; PCM+listening+asset identities |
| J08 Record/Perform | Independent private track/grid/transition preview → Apply/quantized Launch → Play capture with edits/direct VST/Return/pause/cues → Stop → original arrangement compare → take edit/seek/cancel/replay/export; optional live WAV | P07–P12,T12,A04,X02/X06,D01; float null+physical route+listening |
| J09 Failure/recovery | Dirty owned mix → pending export/import/take seek/stem/write failure/cancel → graceful close or controlled owned crash fixture → recovery/current/previous/relink → retry → save/reopen | U05,D02–D07,X03/X04,R01; identity/side effects/child ownership |
| J10 Clean package | Install/extract actual candidate under ordinary user on declared clean Windows → J01/J04 core and optional-integration missing states → upgrade/rollback/remove as claimed | R03–R06/R10/R11; package manifest/rights/install and decoded output |

## Critical handoffs and independent oracles

| Boundary | Failure that feature-only tests can miss | Rule / independent check |
| --- | --- | --- |
| decoder→identity→analysis→grid | truncated MP3 or stale cache yields plausible grid | L01/L06,N02; independent decode and sample-stamped fixtures |
| source grid→project beat→pixel→PCM | same wrong helper used by UI/test/renderer | T01–T03,T10; analytic integral and located source/output attacks |
| original placement→split/stem/effects→take/export | duplicate or lost owner/history | T05/T06,A09,P07/P08,S04,X02; owner graph and known-delay PCM |
| job→UI/project generation→disk | stale success overwrites new state/manual review | U05,N09,D03/D07; deterministic barriers and disk re-read |
| private→programme→record/export | headphone leak hidden by monitor mute | P09/P11,X02; tagged independent multichannel capture |
| UI/MIDI/plugin gesture→sample event→replay | UI says captured while event absent/drifts | P01/P05–P08; authored event frames versus live/replay PCM |
| source/tests→bundle→launcher→clean machine | historical receipt or repo path hides missing resource | R04/R05/R09; exact manifests and no-repo package journey |

## Scope exclusions with reasons

The requested starting areas are all represented: A UI/workflow→U01–U08/C01–C23; B end-to-end→J01–J10; C timeline→T01–T12; D analysis→L01–L06/N01–N09; E engine/DSP→A01–A11; F playback/export→X01–X06/P08/P11; G intelligent mixing→O01–O05/N08; H persistence→D01–D07/S05; I lifecycle→R01/D07/P12/A07; J performance/capacity→A06/R07/R08; K controllers/integrations→P01–P04/A09/A10/S01; L security/rights→R02/R03/R06/R10; M build/package/install→R04/R05; N test quality/diagnostics→R09/U05/R11; O documentation/release claims→R11/R12. Source-specific additions include private isolation, live master WAV, editable take clocks, reviewed-reference retention, explicit transition independence and optional stem worker publication.

Not implemented/claimed: browser/account/auth/cloud collaboration/payment/updater services; their feature tests are NOT APPLICABLE once absence is checked, while network/path/log boundaries still apply. Linux/macOS/mobile installation, compressed-audio export, variable-meter source detection, general themes/skins, phone controller, built-in synth and editable MIDI-note performance are roadmap/excluded features. Unsupported inputs still require safe rejection/disclosure. Existing older-session branches require safety review, not backward acceptance. Installer/uninstaller/signing are unresolved package choices, **not automatically NOT APPLICABLE** until REQ-01/06 decides actual release claims. Physical MIDI/ASIO/private headphones/listening are applicable, currently unavailable/unverified methods, never exclusions.

Ignored `.local/`, build/out, music, sessions/caches and third-party vendor trees are not source-inventory members. Candidate-specific manifests must account for all actually shipped bytes and relevant build inputs. Protected downbeat references/original music are test inputs/evidence only with authorization and source identities; never cleanup targets. Research/design/history nonshipping role is explicit in each inventory row, with provenance/claim checks retained.

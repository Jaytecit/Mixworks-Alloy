> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# DJ.Studio editing and stems — 22 October 2026

This is a reference investigation for Alloy's node/preset audit. It does not implement stem separation in Alloy. No DJ.Studio project was opened, edited, separated or exported during this investigation; the installed application and its data were read only.

## Evidence and version boundary

Installed frontend `1823b9d5ce41ab66810c7263a2f75f017a48bbd5`:

| File | SHA-256 | Coverage |
| --- | --- | --- |
| `simple-studio-B215MvWv.js` | `5a60a5db34cf16a3e3447460268982f091861ac62a1421a0434522304b7aaf23` | REA application `ev_4d72f3fe08867121a3c911f383e766d786ff830eb743b9054a59da83ad9babc2`: parsed, application inventory complete within this file; semantic graph partial at 100,000 nodes. |
| `FullScreenButton-BhZUkJRm.js` | `e19d8f7c27e0cbea61eb78529c6170b0643b3a57e535d0848e86a75e3cb0dd32` | REA application `ev_e2992a830f3a5b9e143d1368307156e14ce441c5bb73f78af67394976f70619e`: parsed; worker exhausted its heap during semantic analysis. Retained partial observations are not complete negative evidence. |
| Exact AST-selected classes `tB`, `HAe`, `qAe` from the preceding file | Per-class hashes and original UTF-16 ranges in local provenance | REA application `ev_915068ff97edb75c1a0d06932de3e51b0bef8eaa1acfa85a083dbcec6b82fa57`: all three class fragments parsed; dependencies omitted; dynamic calls and external ownership unresolved. |

Local evidence: `.local/node-audit-20261022/`: source inventory, exact selected bytes, `dj-focused-provenance.json`, editing-method observations, stem-method index and exported REA ledgers. The fragments are analysis inputs, not executable reconstructions or code to import into Alloy. Final REA evidence includes source-node trace `ev_f9ab2a369c3f6df9b0a5552d41e8417870844e7050bb2f0af36c1506e770059f`; it finds `processStems` and one static relationship, with 12 relevant unknowns. The property-name trace had no seed match and is not evidence that processing is absent.

## Editing tools and methods

| Workflow | Established evidence |
| --- | --- |
| Select and move a transition/effect block | Installed `handleDown` selects an effect/channel, captures drag state, remembers duration/start/edge and tracks the mouse. Read-only and locked records refuse that path. Node `jsrg_node_10ee81e1f3a17cc43874eebe3de1de2567d6ebbd3fd063ef539879c8f0c50633`, simple-studio line 719, columns 19217–20718. Static handler evidence; successful dragging was not observed here. |
| Resize and move track effects | A separate installed handler records edge/duration/visual scale, constrains movement between track boundaries and shows a drag ruler. Alt on its header takes a temporary sample path. Node `jsrg_node_2c6bcbbaac2460f457226f7c99cd6ebc86fa74f78b5b089f5a16f48500cd0916`. |
| Samples: copy, extend, repeat and move | The sample handler distinguishes Alt-copy, repeat and left/right extend hit regions, header dragging and multi-selection. Node `jsrg_node_13855aefc555db243d341796827542a3ed40d49473040d3870f09783c303a11e`. Presence of branches does not prove their complete runtime behavior. |
| Ordinary automation | Official instructions describe selecting a parameter, adding points on its line, moving time/value, deleting points, bending a line with Option/Alt, shortcut/RMB insertion, temporarily bypassing snap and recording control changes for later editing. [Automation editor](https://help.dj.studio/en/articles/8213331-automation-editor-and-effect-blocks). These are documented methods, not newly exercised gestures. |
| Transition presets | Independent parameter treatments, selected-pair scope, locks, gap/strength and manual curves are covered by the [earlier installed transition audit](DJSTUDIO-TRANSITION-AUDIT.md). Its runtime observations remain dated; unresolved drag and listening claims remain unresolved. |

Current online [Studio View documentation](https://help.dj.studio/en/articles/8106005-studio-view-editing-arranging-controls) explicitly applies to **5.0**, with changed selection, zoom and scrolling. Do not apply those newer interaction defaults to the installed 4.2 frontend. The automation guide's published shortcuts also differ from Alloy's bindings; keep Alloy's existing shortcut identities.

The automation guide also documents movable effect blocks with editable boundaries, parameter curves, XY-pad recording and saved presets. It describes merging/removing overlapping automation points. These destructive point-edit semantics should not be assumed for Alloy; Alloy keeps its explicit Delete and conflict guards.

## Stem separation and its timeline use

**Observed shipped code:**

- The processing controller binds the source audio key, mix key, source sample rate and track timeline. It distinguishes whole-track requests from rounded/chunked time ranges; checks the stream cache before generating data; reports progress and refreshes cached waveform views. `setTargetTrack`, `getRoundedStartEnd`, `stemReady`, `getOrGenerateStems`, `processStems` are indexed in local evidence.
- `_generateStems` selects native-channel operations named `processDemucs`, `processDemucsFast`, `processMDXVocals` or `processMDXInstrumental`, conditional on quality, CPU/device and instrumental/vocal mode. The syntax identifies dispatch candidates; the actual model weights, model versions, separation quality, device availability and native implementation were not established.
- The native-channel wrapper has a cache retrieval operation and a versioned request/progress interface. Cache identity includes source and range, with separate ordinary, fast and HQ vocal/instrumental categories. This is a source-processing/cache layer, distinct from timeline automation.
- Waveform-view generation names vocal, drum, bass and other/melodic data separately. The transition editor's `createStemViews` creates **two or four subviews per song**, with separate waveform/automation controls, source channel and stem index. It wires stem selection and cue-edit callbacks and supports a selected stem's enlarged view. A track-effect/header path creates four views with linked-block/manual-edit distinctions. The exact methods and original locations are retained in `dj-editing-method-observations.json`.

**Documented user workflow:** [Stem Separation](https://help.dj.studio/en/articles/9112447-stem-separation) describes a transition Stems category exposing drums, bass, melody and vocals with independent levels/automation. It also covers vocal/instrumental separation, stem use in samples/mashups, quality selection and preloading. Its current preview mode is described as mono 22,050 Hz; exported mixes use high-quality separation. These current documentation statements are not measurements of the installed native processor.

Outside transitions, Show Stems displays sublanes; range mute or per-stem automation applies an edit. Full-track splitting can retain instrumental audio in the main lane and place vocals in a sample lane. Selected regions can become movable isolated samples. Transition choices include None, Crossfade, Swap, Full and Manual, with linked fade/swap handles and gap controls. These are documented workflows; this session did not execute them.

**Inference for Alloy's future design:** keep separation jobs and cache identity outside the audio callback; give each source's stems a shared source-time mapping; expose editable stem sublanes inside the selected transition and ordinary sample/clip paths; retain independent stem gain automation, mute/solo, range editing, Undo and persistence. Preview versus export quality needs explicit readiness/progress and export verification. A separation button alone would not provide timeline stem editing.

No extension download, licence purchase, native model execution, stem audio rendering/listening or export acceptance was performed. The separation feature remains future Alloy work.

## Alloy audit decisions

Empty curves should not show invented editable nodes. An explicit Add creates one point; presets create their chosen shapes. Necessary boundary guards preserve exterior audio, while equal-valued coincident guards are avoided. Sharp cuts need visible vertical amount handles on both selected participants. A flat-side amount edit must keep that plateau flat. RMB pairing must understand both a partner's cut and its complementary opening, keep the source's bend, and preserve clear refusal for conflicts and locks. Reference-app behavior informs these decisions; no proprietary implementation was copied.

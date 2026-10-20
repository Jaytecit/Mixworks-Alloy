# Alloy feature wishlist

Current status reconciled on **20 October 2026 / accepted 0.55.1**, analysis revision **9**, session format **26**. Delivered foundations include transition presets/locks, V/B/M/H, filters, curve visibility/isolation, selected-node percentages, A/B/C rehearsal, named/portable presets, paired swaps, bar navigation/linked moves, recovery snapshots, mastering/monitor mute, WAV tails, **four real lanes with even resizing**, MIDI status/profile reload, section-based placement, Auto Arrange and shared Alloy menus/dialogs. Physical controls were sampled successfully; broader device/listening acceptance remains. Private cue, full plugin delay compensation and continuous recovery remain open. Dancer, Synesthesia integration and broader controller notification/mapping invitation remain wishlist only.

This wishlist owns the request inventory; [the implementation plan](FINISHING-PLAN.md) owns the current build order and acceptance gates. Its ten stages supersede the older ordering below. Original requests and dated discoveries are retained. The user authorised beginning implementation on 20 October after the planning-only consolidation.

## Auto Arrange follow-up — 20 October 2026

The user reported that automatic treatments were layered over a default volume crossfade and requested selectable energy direction, no-loss blends, hard cuts and other useful options. The current increment fixes replacement of that provisional fade and adds **Unrestricted / Keep pace / Build energy / Wind down**, independently of **Balanced / Smooth / Bold / Use my settings / Constant power / Hard cuts only**. Keep pace compares source RMS in both halves of the placed overlap and prevents BPM drops; Build/Wind down additionally require a measured change of at least 1 dB. Unknown energy stays unknown and can shorten a constrained mix. Constant power addresses the normal halfway power dip between similarly loud unrelated recordings; it is not perceptual loudness normalisation. These are requested P22 increments, not completion of the broader Smart DJ/energy-curve/listening goals. Verification and delivery status are owned by the handover and finishing plan.

Recorded from the user's requests on 2026-10-17 and 2026-10-18. Proposed additions are distinguished from existing features. Reliable beats, downbeats and tempo matching remain prerequisites for unattended mixing.

## Integrated To Do requests — 20 October 2026

Source: the user's `C:/Users/jayte/Desktop/To Do.txt`, preserved unchanged, with a byte-identical local planning copy. The imperatives in that source are recorded requirements for planning, not instructions to implement now. IDs below map every source paragraph to the current plan; shared tasks combine overlapping wishes.

| Request | Integrated requirement and current context | Plan |
| --- | --- | --- |
| R01 — automation editing | Nodes are hard to grab or hidden by text. Make switching the edited automation immediate and visible in the transition itself. Curve isolation and selected-node captions exist; improve their accessibility. | Stage 2, P06 |
| R02 — tutor | First-run and revisitable tutor with highlights: library import, analysis, compatibility, two compatible tracks and the automatic/manual transition workflow. | Stage 9, P37 |
| R03 — hints | Popup hints for every feature/control, with an on/off toggle. Extend existing tooltips. | Stage 2, P07 |
| R04 — segment editor | Full track-segment editing in a lower-section tab with a detachable console. Merge with range/ripple/repeat and reusable treatment wishes. | Stage 3, P11–P13 |
| R05 — right panel | Keep VST instruments/effects at far right; remove Clip/MIDI tabs there. The source asks whether Clip is for editing: **yes**, it currently owns start, source-in, length, fades, gain, reverse and loops. Move these into the editor and retain reachable instrument keyboard/setup access. | Stage 3, P11 |
| R06 — ruler | Double-clicking the ruler starts playback. | Stage 1, P03 |
| R07 — active mapping | Show the title of the mapping actually in use on the live desk. | Stage 1, P04 |
| R08 — toolbar | Replace duplicate controller/Audio–MIDI buttons with one settings button that shows detected/connected controller status/name, falling back to Audio / MIDI when absent. | Stage 1, P05 |
| R09 — MIDI reliability | Fix support, mapping, remapping, saving and deleting mappings. Build on delivered setup, working FLkey routing and transactional reload; audit the complete lifecycle. | Stages 2/4, P08/P18 |
| R10 — gaps | Audit gaps and advise on desirable additions. Maintain an inventory throughout; complete the exhaustive audit before final acceptance/manual. | Stage 9, P38–P39 |
| R11 — popup borders | Add orange borders to all application-owned menu popup panels. Shared Alloy styling is already delivered; verify this remaining detail. | Stage 1, P02 |
| R12 — keys/confidence | Show compact `Gbm`/`Gb` notation and an immediately adjacent coloured confidence column: no key found/low/good/excellent, numbered 1–4. Original preference: “Bias toward 99% positive because every one I have tested has been correct so far”. Keep this feedback without presenting uncalibrated scores as 99% accuracy; validate score thresholds and distinguish unknown/manual results. | Stages 1/2, P01/P09 |
| R13 — forge/attract | Intensify all forge/attract lighting and smooth the waveform weld-heat leading edge. Combine with the existing fusion-scrollbar wish; activation already exists. | Stage 8, P31, after P10 |
| R14 — audio breakup | Investigate whether UI drawing/visual effects contribute to occasional playback breakup; other PC processes are a possible cause, not an established explanation. Measure before intensifying visuals. | Stage 2, P10; escalate a reproduced defect immediately |
| R15 — manual | Full plain-language manual/quick start after the app is ready; audit every detail, function, control and background process, deciding what is essential, useful, advanced or internal. Preserve information users need about limits, recovery and consequences. | Stages 9/10, P38–P43; manual last |

## Existing wishlist coverage in the current plan

**Additional planned requirement — 20 October 2026:** a **Follow playhead** toggle
that keeps the current timeline position in view. Scheduled as **Stage 1, P44**,
after ruler double-click playback; not implemented in 0.55.1.

**Idea only, outside the implementation plan:** the user's broader
[musical-map-based arranger concept](docs/ideas/MUSICAL-MAP-ARRANGER.md), with the
complete original text retained alongside it. Its suggested steps, model choices
and proposed order are not implementation instructions or an adopted roadmap.

| Existing wishes, combined by dependency | Plan |
| --- | --- |
| Private headphones; plugin timing/isolation/master VST/band effects; continuous recovery/loading; live next-track/FLkey modes/pickup/diagram/feedback/notification | P15–P18 |
| Full editing/ripple/repeat; treatments/overlays/samples/live loops; flexible panels | P11–P14 |
| Timing/key truth, phrase/vocal explanations; categories/tags/crates/key shifting | P19–P21 |
| Smart DJ, compatible ordering/hints, more generators, make-two-tracks-work, energy curve, exact duration, preferences/cues and mix versions | P22–P24 |
| `.mxm`, DJ-library exchange, FL Studio send/return, editable Ableton exchange, FL live sync/hosting | P25–P26 |
| Drifting-source anchors/other meters; stems/stem effects/drum replacement; patterns/loops/sampler/recording/MIDI export/synth/effects creator/key-aware playing | P27–P30 |
| Forge/fusion; dancer/Synesthesia/video; complete portable skins; API AI/music platforms; dedicated hardware; compressed/split export/publishing/CD | P31–P36 |
| Existing quick-start/tutorial wishes and complete user documentation | P37–P43 |

Historical requests already delivered, such as master mute, spectrum/mastering, basic transition presets and live instrument hosting, remain below as provenance. Their further work is covered above rather than scheduled as duplicate implementations.

## Reviewed priorities — 18 October 2026

Reviewed the preserved [DJ feature recommendations](../DJ_App_Feature_Recommendations.md), [FL Studio / FLkey integration ideas](../FL_Studio_and_FLkey_Integration_Ideas.md), [distinctive feature ideas](../Distinctive_DJ_App_Feature_Ideas.md), and [MixMeister / DJ.Studio comparison](../MixMeister_Fusion_vs_DJ_Studio.md). These are reference proposals, not implementation instructions or independently verified competitor specifications. The order below reflects Alloy's current build, the user's mixing workflow and the dependencies between ideas; it supersedes the documents' suggested ordering.

### Existing foundations — extend rather than rebuild

- **Transition editing:** linked IN/OUT, paired bass points, frames with independent song handles, local length/preset application, independent four-band modes/strength/gaps/positions, Previous/Next and persistent enforced locks are delivered. New additions default to alignment only. Session/portable named presets and A/B/C rendered rehearsal now retain competing treatments, with adjustable before/after bar context and matched RMS. Paired swap drag handles retain off/on gaps. A private cue route remains a follow-up.
- **Controller performance:** FLkey 49 guided MIDI setup, user mappings, live VST instruments and editable armed control-automation recording already exist. A ready-made profile, control diagram, safe pickup, mode switching and LED feedback remain additions; MIDI-note/audio recording and stem control are separate dependencies.
- **Correction and ownership:** source-grid audition/metronome, tap/half/double BPM and beat-one correction, source-owned effects/automation, finite loops and resizable side panels exist. Multiple anchors for drifting source tempo, private cue routing, range/ripple editing and rearrangeable panels remain work.

### Historical recommended build order — superseded by the current plan

| Order | What to build | Why now / first useful outcome |
| --- | --- | --- |
| 0 — finish the foundation | Resolve or explicitly classify the four beat-one conflicts and variable-tempo failures; admit the recovered engine to automatic analysis only when real-track evidence supports it. Recovery snapshots, master mute and explicit export tails are now delivered; finish their physical/listening acceptance. | More automation would otherwise reproduce timing errors or put long sessions at risk. Keep manual transition editing usable while analysis improves. |
| 1 — first increment delivered | **Transition rehearsal and A/B/C comparison**, with persistent transition locks, saved alternatives and named session/portable presets. | The strongest distinctive idea reuses the new frame. Loop one selected transition with adjustable lead-in/out, compare two or three manually edited/preset alternatives at matched audition loudness, then apply one with a single Undo. Preserve both originals and other transitions. Start with user-controlled alternatives; automatic generation comes later. |
| 2 | **Independent headphone preview** and **bar-based editing**. | Audition library tracks or another transition while the main mix continues. Cue audio must use an explicitly selected private output and never leak to master/export. Add 4/8/16/32-bar jump, move, repeat and remove actions over the existing grid, with explicit ripple behaviour and Undo. A rehearsal loop on the main output alone does not fulfil private preview. |
| 3 | **Phrase/section display, vocal regions and explainable placement**. | Show intros, breakdowns, builds, drops and bars to the next change; flag overlapping lead vocals. Start with editable annotations; add automatic suggestions only with independently evaluated structure/vocal detection, confidence and unknown states. A four-bar ruler is not a phrase detector. Explanations must identify the regions/assumptions and let the user correct them. |
| 4 | **FLkey performance polish**. | Add named/importable control profiles and an on-screen layout; safe pickup and clear modes first, then pad feedback for cues, loops, mute and selection. Extend existing automation recording instead of implementing it again. Verify the actual device/messages physically; do not guess factory assignments or assume a DAW handshake. |
| 5 | **Send to FL Studio / return an edit**. | Export a selected transition first, then a whole mix, as aligned per-track WAVs plus tempo/position information and a reference mix. Import an edited render at its original position as an undoable alternative, retaining the previous audio and effects decisions. Preserve a changing tempo map and render tails. Separated stems are optional later; native FL project creation is not required for this useful first exchange workflow. |
| 6 | **Smart crates, harmonic tools and assisted arrangement**. | Add editable tags/rule playlists, audio semitone shifting/key-sync audition and pairwise ordering with reasons. Then build exact-duration and energy-guided mixes on corrected grids, section edits and protected regions. Preview proposed edits before applying them. |

**Recorded priority, reconciled 19 October:** transition locking, a first rehearsal/A/B/C workflow and saved session/portable presets are delivered. Real-track timing/recovered-engine validation and physical/listening acceptance remain open; private preview and broader bar editing remain proposed additions. Curve visibility/isolation and paired filter sweeps are delivered. Adjustable rehearsal context, paired swap handles and 4/8/16/32-bar jumps/linked-track moves are also delivered; repeat/remove/ripple remain follow-ups; other effect generators remain follow-ups. Stems, cloud AI and a new synthesizer are separate later increments.

### Useful additions carried forward

| Idea | Useful Alloy version and dependency |
| --- | --- |
| Make these two tracks work | Offer short blends, breakdown handovers and percussion bridges as editable alternatives, explaining why. Requires rehearsal, presets and reliable section evidence; vocal/stem swaps wait for separation. |
| Draw the energy of a set | User-drawn target curve with peaks and breathing space; propose an arrangement while respecting locked tracks, transitions and protected moments. Requires an evaluated energy/section representation. |
| Fit an exact running time | Target any duration, including 30/60/90 minutes, using only permitted loops, cuts and extensions. Show a preview and report when constraints prevent an exact fit; protect favourite moments and locks. |
| Learn my mixing preferences / personalised cues | An editable, local, opt-in profile learned from explicitly approved transitions and corrected cue positions. Offer reset/export and a visible rationale; do not treat every edit as approval or silently alter the library. |
| Related versions of a mix | Radio, extended, instrumental and performance variants with explicit shared edits and differences. Start with transition alternatives; whole-set branching follows reliable session/recovery handling. |
| Frequency-band or stem-targeted effects | Band-targeted processing and stem-isolated processing are separate capabilities. Retain existing three-band EQ; add frequency routing with timing/latency verification, and stem effects only after aligned separation/recombination is proven. |
| Hands-on stems and drum replacement | Reuse controller modes for vocal/drum/bass/instrument levels once stem separation exists; later support drum capture/replacement with an aligned original retained. |
| Percussion patterns and FL-created transition loops | Import tempo-aligned fills/risers first; a synchronised percussion sequencer and drum capture follow sample ownership, quantised launch and recording. Extend the existing creation-tools wish rather than adding a second engine. |
| Keyboard sampler and key-aware playing | Reusable captured samples with velocity/pitch and quantised note recording; note/chord suggestions must reflect key uncertainty. Live VST keyboard input already exists; sampler sequencing and MIDI export do not. |
| FL live sync / FL plugin hosting | Later feasibility work using the actual available plugin formats, transport, multiple outputs and measured clock/latency behaviour. Existing VST instrument hosting is a foundation, not proof of FL Studio integration. |
| DJ-library and editable-project exchange | Inspect cue/grid/tag import and prepared-set export for the user's actual DJ formats; investigate editable Ableton export after aligned audio exchange. Preserve source identities and unsupported fields explicitly. |

### Deferred rather than discarded

Stem separation, visualiser/video output, complete skins, built-in instruments/effects creation, dedicated hardware and music-service integrations retain their original wishes below. Direct publishing and CD burning are finishing conveniences after reliable local export; begin with exportable files rather than tying the core mix workflow to a service. Competitor edition/pricing claims in the comparison are not Alloy requirements. Do not add Windows AU support solely because a cross-platform comparison lists it.

For FL feasibility, Novation documents custom control messages and incoming-MIDI pad feedback in its [FLkey 49 Custom Modes guide](https://userguides.novationmusic.com/hc/en-gb/articles/23741692855826-Using-FLkey-49-s-Custom-Modes-and-Components); that does not establish an Alloy LED implementation. Image-Line documents separate mixer-track WAV export in its [audio/MIDI export guide](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/fformats_save_export.htm); an Alloy exchange package and reliable return workflow still need building. These checks support the proposed approach, not a compatibility claim.

## Product direction: DJ.Studio-led transition editing

Updated from the user's 2026-10-18 reference image. Use DJ.Studio as the primary reference for arranging and editing transitions, while retaining useful MixMeister workflows and existing Mixworks capabilities. Do not assume a feature is absent from DJ.Studio without checking its installed implementation.

The supplied [transition-band and lock reference](.local/references/djstudio-transition-lock-2026-10-18.png) is retained locally.

- **Visible transition region** — orange pulse frame, resize grips and independent song handles implemented; see [the transition guide](docs/TRANSITION-PRESETS.md). Source key movement display is implemented. Original request: a clearly bounded band spanning the outgoing and incoming tracks, with its start, end, affected audio, transition identity and key movement visible beside its controls.
- **Transition lock in context** — implemented with orange frames, a green lock badge, inspector lock/unlock, model/controller protection, Undo/Redo and format-15 persistence. Alloy conservatively protects both participating tracks; see the transition guide for scope and exclusions. Wider DJ.Studio mutation parity remains unverified.
- **Keep the timing foundation first** — validate BPM, bar phase and key on real recordings before presenting automated transitions as dependable. The broader DJ.Studio comparison follows the earlier four-track checks.

## Requested features

- **MixMeister arrangement import (.mxm)** — import MixMeister `.mxm` arrangement files into Alloy. Added 2026-10-19; wishlist only, not implemented.

- **Stem separation** — separate tracks into individual stems.
- **Video and music visualiser channel** — a dedicated, editable timeline lane integrating the existing **Synesthesia reconstruction**. When enabled, it should automatically populate the visualiser lane. Synesthesia confirmed by the user on 2026-10-19. Wishlist only; integration, cue generation and editing behaviour remain future work.
- **Audio-reactive dancer panel** — a dancer in its own small panel, dancing reactively to the playing music. Added 2026-10-19; wishlist only. Appearance and motion design remain open.
- **Detected MIDI controller notification and mapping offer** — notify the user when a MIDI controller is detected and offer to map it. Added 2026-10-19. Persistent detected/open/receiving status and profile reload are delivered in 0.54.5; broader notification and mapping invitation remain future work. Retain existing mappings.
- **Flexible panel layouts** — resizable and rearrangeable panels (tiles) for preferred workspace layouts.
- **Skins and themes** — preset and user-created skins covering the whole interface: colours, typography, icons, control appearance, panel arrangements, sizes and visibility, and toolbar layouts. Users should be able to create their own skins and share them as portable packages that others can import and apply. Provide creation guidance and starter templates; the package format and authoring tools remain to be designed. This expands the original theme wish and complements flexible panel layouts.
- **Built-in creation tools** — a synth, drum machine, and effects creator.
- **Smart DJ / automated mix producer** — automatically create complete mixes targeting the user's usual 60–90 minute duration. Select compatible tracks, arrange them and mix live or add automation. **Partial foundation delivered in 0.55.0–0.55.1:** Auto Arrange has count/approximate-duration targets, harmonic/BPM ordering and editable blend styles. Exact duration, shaped whole-set energy curves, broader protected-edit workflows and musical listening acceptance remain P22–P24; do not rebuild the existing generator.
- **AI mixing integration** — intelligent mixing through an API using the user's API key.
- **Dedicated control hardware** — a dedicated piece of hardware for full, easy control of Mixworks.
- **Music platform integrations** — integrate Beatport, SoundCloud and other music platforms.
- **Mix-compatible track ordering and guidance** — sort the entire music library, or a selected group of tracks, into an order where each track is well matched for mixing into the next. Include hints, tips and transition ideas for moving between each pair of tracks.
- **Automated transition presets** — basic Align IN/OUT, Bass swap and Crossfade are delivered. Named session presets, portable import/export and A/B/C rehearsal now retain editable volume/EQ treatment and timing within an existing frame, with manual conflict checks and one-Undo application. Broader automated generation remains dependent on reliable musical evidence. Build on song-owned automation; see the [transition guide](docs/TRANSITION-PRESETS.md) and [MixMeister comparison](docs/research/MIXMEISTER-GAPS.md).
- **Live effects mixer controller** — live performance controls for mixing and effects, similar to DDJ features.
- **Spectrum analyser and global mastering mixer** — clicking the small hexagon logo at the top left of the timeline should open a full spectrum analyser and global mastering mixer. Added 2026-10-18.
- **Master output mute** — add a mute tickbox next to the master output slider at the top right. Added 2026-10-18.
- **Timeline scrollbar fusion effect** — a laser or fire effect on the timeline scrollbar that visually represents the melding of two different things.

## Additional discoveries from MixMeister

Audited against v0.22.0 on 2026-10-17. See [evidence, current coverage and limitations](docs/research/MIXMEISTER-GAPS.md). These are suggested additions; the existing wishes above remain intact.

First, strengthen reliable editing:

- **Source-grid editor and metronome** — audible grid checking, tap/half/double BPM and beat-one correction are delivered. Remaining: multiple source correction anchors for drifting recordings and broader real-track accuracy validation.
- **Song-owned automation and effects** — delivered for clip EQ, automation and effects through rearrangement. Extend these owners for named treatments and linked overlays rather than rebuilding ownership.
- **Named song treatments** — save and recall complete song edits across sessions. The current Tweaks command only opens analysis settings.
- **Recovery snapshots** — explicit recovery snapshots and restore are implemented; continuous snapshot policy remains a follow-up. Fusion's recovery prompt is confirmed statically, but its behaviour remains untested.

Then expand preparation and performance:

- **Separate preview output** — audition the library and upcoming transitions independently of playback.
- **Live loop capture and release** — extend the existing finite-repeat loop editing.
- **Range and ripple editing** — select parts of clips directly; make insertion, replacement and gap closing explicit.
- **Audio key shifting** — transpose audio independently of BPM; existing key overrides only change analysis metadata.
- **Linked overlays and sample launching** — keep samples with a parent song and make reusable snippets readily available.
- **More render choices** — compressed output, audio division at labels, and optional CD production.
- **Library categories and metadata editing** — complement the existing search, sorting and key display; recommendations remain part of the ordering/Smart DJ wishes above.
- **Live next-track controls** — immediate transitions and fade/pause/cue-next actions, extending the live-controller wish.
- **Master effects and more independent mixer lanes** — historical eight-lane work was superseded by the user's four-lane design in 0.54.2. The current app has **four** independently mixed lanes and shared built-in master processing. A general master VST plugin chain remains P16; extra lanes are not newly requested by retaining this historical discovery.

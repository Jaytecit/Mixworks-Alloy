> Historical snapshot preserved on 4 November 2026. Completed tasks and old next steps below are inactive. Use [the outstanding roadmap](../../../FINISHING-PLAN.md) and [current handover](../../../HANDOVER.md).

# Mixworks current handover — 4 November 2026

## Current milestone

**Transition sharpness recovery — complete locally, selected for next launch,
4 November:** Jay reported a smoky/soft scene followed by a jarring snap to
sharpness after a join. Reproduced the cause: `GenerativeHandoff` continued
showing recurrent (temporally softened) output throughout its fixed 12-second
post-overlap tail, then `OriginalScenePlayer` abruptly bypassed it. The old
synthetic moving-detail probe still differed from live pixels by about 48/255
immediately before retirement. This timer is seconds, not a fixed bar count.

A smooth endpoint-flat envelope now restores the incoming live scene during
that same tail and fades inherited shader material to zero before resource
retirement. Generative overlap, direct-join capture, typed-state seeding and the
bounded target lifetime remain. No scene shaders, approved punch motion, music
transition timing, media configuration or projectM internals changed.

The initial focused GPU reproduction failed seven assertions; retained as red
evidence. Final packaged recovery probe **36/0** covers synthetic moving detail,
all four Original/MilkDrop overlap directions, direct joins into both engines,
late-tail convergence, retirement and exact pause. All six real-scene routes
matched native pixels before retirement. Existing sprite/join GPU checks
**38/0**, focused original-visual Python tests **14/0**. Checks ran serially,
below normal priority. A 17-second, 340-frame/20fps side-by-side comparison
uses identical authored PCM with Prism Bloom into Jumping Techno Tunnel;
encoded pixels at the old cutoff were inspected and the whole video decoded.
No native source change, rebuild/native suites, full regression, stress,
sustained-performance, hardware/listening or interactive UI acceptance claimed.

`out/Launch.cmd` selects
`out/releases/0.55.68-a970e76d6fb2-smooth-feedback/Mixworks.exe` for the next
launch. Native executable SHA256 remains
`a970e76d6fb222cfa9a9fae421a1b93168592f8ad927292cd88feb55b03eeea6`;
the separately copied Python/GLSL runtime changes are bound in the new local
receipt. Previous bundle retained, immutable dependencies shared by exact-file
hard links; launcher backup `out/Launch.cmd.bak.smooth-feedback-20261104`.
No production app started/restarted, session/cache reset, commit/push or public
package. Source checkpoints, red/green results, packaged source hashes, visual
comparison and activation receipt: `.local/transition-recovery-20261104`.
Preview: `Before-and-After.mp4` (silent video; authored PCM drives rendering).

**Transparent sprite evolution, generative joins and brightness — complete locally,
verified and selected for the next launch, 4 November:** Jay requested MilkDrop
sprite nesting, the last-frame generative technique for every scene join, and
transparent sprites evolving **while used inside a scene**, rather than just
transitions between sprite files. Opaque backgrounds keep their existing path.
He subsequently reported dark sprite entry; this is included in the same update.

`visual_feedback.py` supplies bounded recurrent premultiplied sprite material.
Original scenes with cutout support consume it through their native shader
inputs. MilkDrop uses an Alloy-owned recurrent stage steered by its rendered
field; projectM's private buffers, DLL and preset files are unchanged. Initial
capture is full strength, with source fitting deferred to the original scene.
Sprites retain moving fragments after removal. Every join now inherits the
last displayed image, including all four engine directions, long overlaps and
joins without overlap (two-second evolution). Native typed state adapters remain;
inherited material survives outgoing teardown. The lane labels joins Feedback.
Pause holds pixels; seek/discontinuity clears histories. MilkDrop's existing
wall-clock/non-deterministic seek and offline-export limits remain.

Found double opacity in shared RGBA compositing: half-opacity pixels could be
attenuated to a quarter before shading. The runtime source adapter corrects
**107 passes across 99 scenes**; RGB-only blends already applying coverage once
remain intact. Sprite-only upload preparation adds bounded hue-preserving
exposure and softer faint coverage. Hidden RGB cannot leak, black detail and
clear pixels survive. All **67** current transparent sprite files were audited:
visible contribution is retained/increased, premultiplied bounds hold, original
file hashes unchanged. Opaque backgrounds bypass preparation. No scene shader
files, approved motion, descriptors or gallery JPGs were rewritten.

One single-worker/below-normal native build passed. Final focused results:
**239 native** (MilkDrop43, original-visual134, visual-arrangement62), **38 GPU**
and **29 Python** checks passed, serially; native runs use `verification.run_checks`
with stress off and clean generated-fixture retention. GPU evidence includes an
independent alpha-compositing pixel oracle, visible first capture, source-removal
survival, exact pause/capture switching, all four engine joins, direct-join
continuity, outgoing teardown, seek reset and 32:9 output. Native UI/scene pixels
were inspected, including the Feedback lane label. Packaged bridge root
`.local/msp06` verified protocol/root/PID/capabilities, saved/reopened an empty
isolated session, captured inspected UI and gracefully closed. A 15-second,
450-frame/30fps 640x360 packaged preview with authored PCM and Circuit Martini
artwork fully decoded; encoded frames were inspected. Artistic acceptance is Jay's.
No full regression, stress, sustained-performance, hardware/listening or mouse
gesture acceptance is claimed. Existing gallery previews were not regenerated.

`out/Launch.cmd` now selects
`out/releases/0.55.68-a970e76d6fb2-generative-sprites-v3/Mixworks.exe`, SHA256
`a970e76d6fb222cfa9a9fae421a1b93168592f8ad927292cd88feb55b03eeea6`.
Python/GLSL-only visual corrections were assembled after that native build;
final receipts additionally bind all six runtime source hashes. Backup:
`out/Launch.cmd.bak.generative-sprites-a970e76d`. No production app was started
or restarted. User sessions, analysis/corrections, caches, protected references,
original media, external presets/JPGs and deferred NestDrop scan remain untouched.
No commit/push or public package. [Behaviour and limits](../../MILKDROP.md).

Independent before/after source archives, bundle/runtime identities, final checks,
preview, activation receipt and retained failures are under
`.local/milkdrop-sprites-20261104`. First native MilkDrop attempt40/1 lacked the
neighbouring preset/JPG expected by its existing check; an authored owned fixture
corrected that environment, with original failure and retry provenance retained.
Early previews exposed weak initial capture/double fitting, then the shared alpha
issue; superseded evidence and the preview script's retired-scene-key failure
remain recorded. These are not concealed complete-gate passes.

**Scene energy and screen coverage — complete, verified, launcher staged, 4 November:**
All **60 remaining candidates** are resolved: **38 changed, 22 retained with
observed motion evidence**. [Per-scene decisions](../../SCENE-ENERGY-20261104.md).
All 60 baselines and 38 corrected scenes received controlled 16-second
quiet/strong/recovery probes with identical artwork, controls and authored 120 BPM
PCM; pixels reviewed, exact pause and fresh-state seek/reset asserted. Fourteen
selected scenes also received 32:9 probes; eleven had wide-framing corrections.
All **228** refreshed main/named-look previews were inspected and rendered
finite/nonblank with no GL errors and exact pause. The approved trio and first
Jumping Techno Tunnel are exact; all 166 other scene directories, every descriptor,
control and catalogue key are unchanged. Lucent Ranges changes only its sky.

Plasma Orchard's collapse was input-dependent: low-food background/cutout fixtures
lost population while the procedural no-image fixture stayed alive. Viable
biological recruitment now sustains all four tested source modes. Velvet Spores
still left its lower field sparse after 32 seconds; a second image-fed bed repairs
that. Tidal Lichen receives secondary growth beds. No display decoration hides
the population issues.

One single-worker/below-normal build passed. Native renderer **134/0** and
visual-arrangement **62/0** passed serially through `verification.run_checks`,
with stress disabled and generated-fixture retention clean. Native 720p scene
and browser pixels were inspected. Twelve added family representatives exercise
real placement, disk round-trip and mixed PCM. Packaged bridge startup in fresh
`.local/se04` verified protocol/root/PID/capabilities, saved/reopened an empty
isolated session, captured inspected UI, saved again and gracefully closed.
No full regression, stress, sustained performance, hardware or listening claim.

`out/Launch.cmd` now selects
`out/releases/0.55.68-768fee771c5d-scene-energy/Mixworks.exe`, SHA256
`768fee771c5db896b02163d779b2beb3ababd43f48b7ac2b5afbfef906e9523f`.
Previous bundle and media-folder settings retained; backup:
`out/Launch.cmd.bak.scene-energy-768fee771c5d`. No production app started or
restarted. Production sessions, analysis/corrections, caches, protected references,
user media, external MilkDrop presets/JPGs and the deferred NestDrop scan remain
untouched. No commit/push or public package.

Independent checkpoints, source/bundle hashes, failures/superseded attempts and
receipts: `.local/scene-energy-20261104`. All 38 labelled before/after videos
fully decoded: 240 frames each, 16 seconds, 15 fps, 768×244, identical probe audio.
Gallery: `comparisons/index.html`; representative reel:
`comparisons/Alloy-Scene-Energy-Before-After.mp4` (960 video frames / 64 seconds;
AAC is 64.023 seconds). Encoded frames inspected. Artistic acceptance remains
Jay's. This completes the authorised implementation and local technical scope,
superseding older preview-only/broad-changes-unapproved notes below.

**Approved scene motion + native tunnel - implemented, verified, launcher updated,
4 November:** Jay approved the three punch-motion proposals and requested adding
the first jumping tunnel to Alloy's scenes. This supersedes preview-only scope
for these four scenes. The three checked-in `main.glsl` files now exactly match
the inspected proposals in `.local/punch-motion-20261104/proposal`; other scene
shaders are unchanged. `original/jumping-techno-tunnel` is registered in Originals
as **Jumping Techno Tunnel**, with its native GLSL330 shader, descriptor, original
cyan/pink named look and library JPG. It retains the original time-times-bass
jumps and requires no textures or MilkDrop import. There are now204 originals;
the preceding203 image scenes retain their six named looks. Refreshed all18
looks/main previews for the approved trio and the tunnel's one preview.

`out/Launch.cmd` selects
`out/releases/0.55.68-06aa7e887c6b-punch-scenes/Mixworks.exe`, SHA256
`06aa7e887c6b9f5cc088d01f0f49d06e06435ca6647d6b504a0bf5ec5157bd06`.
Previous release/media-folder configuration retained; launcher backup is
`out/Launch.cmd.bak.punch-scenes-06aa7e887c6b`. No production app was started or
restarted. Existing library, user mix, analysis/corrections, caches, NestDrop
presets/JPGs and the deferred full collection scan remain untouched.

One single-worker/below-normal build passed. Focused native renderer66/0 and
visual-arrangement62/0 passed serially via `verification.run_checks`, including
all four changed scenes' placement, disk round-trip and live mixed-PCM rendering.
Native browser captures were inspected. Nine Python catalogue checks passed;
the first attempt used system Python without OpenGL and is retained separately,
then the packaged visual Python ran the suite successfully. Nineteen look renders
were nonblank, had no GL errors and held exact pixels on pause. Packaged candidate
started through the bridge in fresh `.local/pm04`; protocol/root/PID/capabilities
and empty isolated library were asserted, screenshot inspected, session saved
and app gracefully closed. No full gate, stress or hardware/listening claim.
Evidence, source snapshots, bundle/file hashes and activation receipt are in
`.local/punch-apply-20261104`; copied immutable dependencies use exact-file hard
links instead of repeated full model/Python copies. Do not modify those assets.

Jay also requested a prompt for a new session to apply this direction to every
remaining scene needing it. [Ready-to-paste prompt](../../NEXT-SESSION-SCENE-ENERGY.md)
covers the remaining60 candidates from the original63-scene audit, stronger
motion first, matching activity where necessary, Plasma Orchard investigation,
protected approved scenes, evidence and low-disruption verification. It does not
instruct indiscriminate changes to all204 scenes. Broader work is not applied in
this pass; progress/re-evaluation belongs in that next session.

**Punch-motion comparisons and original tunnel file - 4 November:** Jay requested
before/after examples for Prism Bloom, Liquid Torus and Orbit Theatre, plus an
Alloy-ready file of the **first cyan/pink rings-and-spokes tunnel, retaining its
jumping motion**. `.local/punch-motion-20261104` contains three labelled12s/30fps
side-by-side videos, current left/proposed right, with identical controls/images
and authored120BPM rhythm. Proposals alter actual scene camera/scale/orientation,
not finished-video zoom; no generic backgrounds added. Original activated scene
hashes are unchanged. Source snapshots, render script, controls, contact sheets,
coverage measurements and video hashes are retained independently of Git.
Inspected spaced frames show much larger excursions without broken geometry;
Prism Bloom gains the strongest coverage; Orbit Theatre still has outer space
between surges. These are isolated movement proposals, not approved replacements.

`Jumping-Techno-Tunnel.glsl` and native descriptor/catalogue package successfully
render through Alloy's original-scene runtime. Original `time*(.6+1.5*bass)`
jumping depth/palette/rings/spokes retained. No native GLSL import UI exists;
the separate `Jumping-Techno-Tunnel.milk` companion can be tried via **Import
MilkDrop...** in the current build. It rendered60 frames through that release's
projectM DLL with actual PCM response and exact held-frame pause. Its relative
bass mapping (`saturate(bass*.35)`) and projectM clock differ from native uniforms;
do not describe the two as bit-identical. Native tunnel video and actual .milk
JPG are separately labelled. No textures required, no production import made.
Initial .milk version-header omission caused a fallback image despite successful
API return; corrected with explicit preset201/composite2 versions and visually
confirmed cyan/pink tunnel output. Initial evidence is preserved.

All four final videos have360 video frames, audio streams and successful complete
decode. Four native scene renders have no GL errors and exact frozen-frame pause.
Focused render/decode evidence is in `render-results.json`, `delivery-checks.json`
and logs; no build, native suite, full gate, stress, launcher activation, app
restart, production data mutation or full NestDrop scan. README and tunnel ZIP
explain both file formats. Jay's artistic acceptance remains pending.

**Preferred motion direction - 4 November:** After the simple-tunnel comparison,
Jay said: "i prefer the jumping version I think. It gives much more energy to the
scene. Perhaps this is what is missing from many of our scenes." Preserve this
artistic preference: strong pumps, snap rotations and forward/back camera surges
are useful scene behaviours, not automatically defects to smooth away. Prioritise
punchy movement comparisons for central/slow scenes, then add matching backgrounds
where coverage remains weak. Later implementation can control jump amount/timing
while retaining impact and exact pause. This is an emerging preferred direction,
not a mandate to make every scene jump identically. **Broad scene changes remain
unapproved; the isolated proposals and tunnel-file request above are authorised.** Audit report
has this direction update; prior measurements/CSV remain baseline evidence.

**Tunnel shader examples and Alloy adaptation review - 4 November:** Jay clarified
that original/revised examples meant his simple tunnel GLSL, not scene-background
changes. Interactive, verified WebGL comparison is in the chat visualization
`tunnel-original-and-revised.html` with independent copy and GLSL files under
`.local/tunnel-shader-comparison-20261104`. Same rings/spokes/palette; accumulated
travel avoids elapsed-time multiplied by changing bass, and flash falloff is
clamped outside radius1. Simulated-bass slider,16:9/32:9 and bounded8s demo checked.
The example needs a host-provided accumulated travel uniform; it is not a
single-pass Shadertoy Image shader drop-in. No Alloy scene installed or changed.

Jay then provided an8KB raymarched Techno Tunnel and asked what changes Alloy
requires. `.local/techno-tunnel-alloy-example-20261104/main.glsl` is a compile-only
adapter example: GLSL330 entry/output/resolution, FFT replaced with Alloy scalar
bands, projectBeat replaces fixed132BPM, travelTime drives secondary motion.
Original raymarch/reflection/shadow/AA constants retained. It successfully compiles
and links on Alloy's OpenGL3.3/NVIDIA renderer; **not rendered, performance-qualified,
registered or activated**. Scene descriptor, catalogue/presets/preview and relevant
frame-time/pause/seek/tempo checks remain for a later implementation request.
This larger shader already has continuous beat-step travel; do not blindly apply
the earlier simple shader's time-times-bass diagnosis to it. User source, exact
adapter, hashes and compile receipt are retained. New advanced-shader visuals must
not be represented by the earlier simple-tunnel comparison. Current app unaffected.

**Original scene space/motion review - complete, recommendations only, 4 November:**
Jay requested identifying empty scene backgrounds, then clarified that large,
frequent pumps/rotations/sweeps can satisfy screen activity. He explicitly chose
shortlist/recommendations first; **no shader, descriptor, preview or app changes**.
[Full audit and scene-by-scene shortlist](../../SCENE-SPACE-AUDIT-20261104.md).
All203 main previews were visually inspected;1,218 named-look stills were measured.
Initial65 still candidates (45 stronger) were refined by20 inspected16-second
motion probes on the activated d79c930d runtime at320x180/10fps, same opaque
background/cutout and authored120BPM input. Final63 proposals:38 priority,
19 secondary,5 motion-first conditional and1 separate behaviour investigation;
3 initial flags retained because motion supports the brief;137 others have no
obvious still-review concern. Most proposals are still/source evidence only,
not individually motion-qualified.

Start with Iridescent Wake, Graphite Wake, Ember Advection and Nebula Press for
matching moving depth/outer-field activity. Liquid Torus, Prism Bloom and Orbit
Theatre need much larger camera/scale excursions; current rotation does not
fill their outer field. Keep Neon Run, Soma Lanterns and Circuit Confetti for now.
Cellular Lace and travelling particle/band scenes may need stronger/more frequent
coverage rather than generic backgrounds. Plasma Orchard stayed almost empty
under the sampled strong input at both10fps and a focused30fps probe; diagnose
population/visibility first, not decorate over it. Cause/other inputs unverified.

Evidence `.local/scene-backgrounds-20261104`:203-scene CSV/decisions, contact sheets,
20 clips, two inspected motion sheets,30fps check and labelled16-second640x360
comparison reel (160frames, full decode passed). Failed missing-image-path and
superseded partial cutout-only attempts are retained separately. No rebuild,
native gate, stress, production UI/session, launcher, data/cache or MilkDrop scan.
Jay then supplied a Shadertoy-style tunnel mainImage and asked whether enough for
a scene. Yes: visual core complete, needs Alloy uniforms/main/descriptor wrapper;
FFT sampler can map to existing bass feed. Use accumulated travel rather than
elapsed-time times current bass (jumps), and clamp1-r for wide-corner falloff.
No new tunnel scene implementation requested yet.

**Linked MilkDrop library - implemented, verified and activated, 4 November:**
Jay explicitly requested activation and launcher update, keeping the full collection
scan deferred until NestDrop finishes generating previews. Exact packaged candidate
passed a fresh `.local/md03` bridge startup/protocol/root/PID/capability check,
saved, captured inspected UI and gracefully closed. Native 43/0 and Python 9/0
receipts were reused for the unchanged executable/runtime; no rebuild, full gate,
stress or full collection scan. Staged 6,687 payload identities are in the receipt.

`out/Launch.cmd` now selects
`out/releases/0.55.68-d79c930d987f-milkdrop-linked/Mixworks.exe`; normal PID1812
is running with the existing3,011-track library. Previous release is preserved;
launcher backup is `out/Launch.cmd.bak.milkdrop-linked-d79c930d`.
Jay saved the playing untitled mix as
`C:/Users/jayte/Desktop/Mixworks/Automix 100.mixworks` and closed the old app himself.
Independent exact saved copy and preferences backup are retained in the evidence.
The startup file argument did not reopen the mix, and computer-use text entry
into the Open picker had no effect. Picker was cancelled; **current workspace is
clean Untitled, stopped; Automix100 is saved but not reopened**. Do not claim its
reopen succeeded. User music, analysis/corrections, caches and external presets/
JPGs were not reset or imported. NestDrop and its generation were left running.
No commit/push or public binary release. Activation details are in
`.local/milkdrop-linked-20261104/activation-receipt.json` and `activation-ui.json`.
The subsequent implementation notes describe its earlier pre-activation state.

Jay requested linking an existing preset/texture tree without copying or moving
source files, a separate MilkDrop view and previews. Existing NestDrop JPGs are
used directly first. Read-only filename enumeration found 59,811 `.milk` files,
22,773 JPGs and 22,749 matching same-stem pairs in his Presets folder. Background
recursive indexing writes only per-root metadata under MilkDrop-v1/linked;
content deduplication replaces the 128-entry cap. Originals/MilkDrop browser views
are separate, with search and 24-preset pages. Thumbnails are retained in memory
only for the MilkDrop page; disk location alone does not reduce decoded RAM use.
Missing images use serial 320x180 low-priority child previews with demo audio;
generation pauses during playback, output or a hidden browser. External JPGs are
never copied. Presets validate on use; reconnect/rescan guidance covers missing
or changed sources. Choose the same folder again to rescan. [Usage](../../MILKDROP.md).

Two serial below-normal builds passed (initial, then two-file fault correction).
Final executable SHA256 `d79c930d987fa6b9a366a40ae7e64582a2d66f0f784952d2dcf81a819d7cc2f2`.
**43 focused native checks and 9 Python checks passed**, with stress disabled and
native retention clean. This includes recursive 140-preset indexing, duplicates,
invalid files, rescan/deletion/cancellation, metadata-only storage, async UI link,
real linked preset playback, generated preview, external existing JPG reading,
separate library views, saved content IDs, originals, Both, Match mix and Undo.
Existing-JPG gallery/selected and generated-preview native captures were inspected.
Project check:173 documents,643 local links,173 CMake sources,0 errors.

Initial 41-check candidate had 3 failures, preserved independently: embedded
Python lacked its script import path, and the full installed collection did not
successfully index within the check while Jay was generating JPGs in NestDrop.
The fallback path is fixed and reverified; scanner entry-status errors are now
local to a file so disappearing/replaced JPGs cannot abort traversal. Do not blame
all initial failures on contention. At Jay's report of active generation, no
second collection-wide scan was run. Final external-JPG proof uses one existing
preset/image through isolated index metadata. **Full 59,811-file scan performance
and concurrent NestDrop-generation acceptance remain unverified.** No full gate,
stress, chooser gestures, physical audio/listening or deterministic export claim.

Evidence, failed/final receipts and source/executable checkpoints are under
`.local/milkdrop-linked-20261104`. Normal app/session/data/cache, source collection,
launcher and release bundles were kept; no production link/import, activation,
commit or push. Stop only owned checking if responsiveness worsens. Jay also asked
about the Ableton Link DLL: the NestDrop manual documents shared BPM/peer sync as
a Pro feature; this does not enable Link in Alloy or make it a preset dependency.

**Automatic visual sources — implemented, verified and activated, 4 November:**
Jay requested Originals / MilkDrop / Both for automatic visual timelines, then
explicitly requested build, activation, launcher update and Git push. Auto Arrange
now has **Scene sources**, with the same choice above **Match mix** in the scene
browser. Originals remains the default; Keep visual lane disables the selector
and preserves all blocks. Both balances engines before choosing individual scenes
so a small imported collection is represented beside the203 originals (a one-block
show starts with an original). Imported scenes keep their own image behaviour,
receive only declared controls, and use supported inter-engine transitions.
Missing imports/runtime preserve the current arrangement before preparation.
The choice is shared in the current window; no saved-format/analysis change.
This supersedes the originals-only automatic status in the first import below.

One serial below-normal build passed, executable SHA256
`4430528d60256b4f3e32e63e979014da6b92957c7122ed0ab9cb34e89c6fcb08`.
**62 visual-arrangement checks +28 imported/native integration checks passed**
through `verification.run_checks`, stress disabled and retention clean. Tests
include all source filters with a selected image, schemas/handovers, preserving
music, save/parse, missing imports, real Make mix/Both publication, original and
imported rendering from actual mixed PCM, pause/seek, Match mix and Undo/Redo.
Actual source-choice form, mixed-engine timeline and shared browser selector
captures were inspected. The last browser capture shows renderer preparation
and is selector/UI evidence, not an additional rendered-frame claim. No full
gate/stress, physical/listening or deterministic MilkDrop export claim.

Local evidence: `.local/milkdrop-auto-20261104`, before/candidate/final source
checkpoints, reports, asset identities and activation receipt. All2598 source
visual files are exact relative to the start of this milestone; Jay's previously
confirmed duplicate-Sprites cleanup remains intact.6686 staged payload files
were hashed. The exact packaged candidate passed fresh `.local/md02` bridge
protocol/root/PID/capability/startup checks, saved and gracefully closed; its
captured UI was inspected. Native workflow checks cover the same executable
and renderer identities. The startup smoke used no music or production data.

Normal launcher now selects
`out/releases/0.55.68-4430528d6025-milkdrop/Mixworks.exe`.
Previous scenes-90971337 release remains intact; launcher backup
`out/Launch.cmd.bak.milkdrop-4430528d`. Normal PID28316 started with no previous
Alloy instance running. No user session was closed, no fixture injected into
production, no cache/data reset. Existing media-library settings are exact;
normal workspace/library/scan preferences were independently copied before
activation. The user-authorised Geiss preset was copied once into the normal
separate `MilkDrop-v1` library (content hashbb71f1af...); source preset/textures
stay untouched. Optional projectM/GLEW and notices are in the local bundle.

The final delivery commit/push is recorded in the local evidence receipt. It
includes the earlier approved, previously uncommitted original-scene refinements
and required source/assets so Git represents the activated app. No binaries,
user music, arrangements, AppData, `.local`, build or out payloads are published.
Local activation is focused technical delivery, not public binary clearance.
MilkDrop wall-clock/seek-reset, mono feed and export limitations below still apply.

**MilkDrop first import — implemented and verified locally, not activated:**
Jay authorised a separate MilkDrop path using his NestDrop preset installation,
keeping all existing scenes. New **Import MilkDrop...** button in Visuals/scenes
copies one bounded `.milk` into a separate content-addressed local library;
imported entries are prefixed MilkDrop and excluded from automatic Match mix.
Unmodified upstream libprojectM4.1.8/GLEW2.3.1 are optional dynamically loaded
DLLs in the existing GL child worker. No NestDrop plugin/executable or preset
pack bundled. Original native renderer does not load projectM until needed.
[Usage, dependency pins and limits](../../MILKDROP.md).

One serial below-normal main build passed, executable SHA256
`d8afffc3cc1f8e4577e09eabb203799cf3e3a33e66ede09e17eb454a81f9628f`.
**18 new native checks +50 original visual checks passed** through
`verification.run_checks`, stress disabled, generated arrangements removed by
retention. Native fixture `Geiss - 3 layers.milk` from Jay's installation
(SHA256 `bb71f1afe8ca7bbe11cc7fcb614ebffae60d8ece212f9f39d0cbe09d5779bfe0`)
rendered actual two-source engine mixed PCM; import/deduplication, placement,
save/parse, pause, seek and switching back to Prism Bloom passed. Native browser
and original captures inspected. Raw saved native facts describe the final
paused seek (RMS0); the earlier live mixed-PCM assertion is in checks.txt.
19 Python tests and10 bounded GPU checks passed, including simultaneous
silence/audio pixel response (mean absolute difference55.97/255), exact pause,
owned/imported dissolves, generative fallback, seek reset and original recovery.
GPU response/composite pixels inspected. Project check:173 documents,
642 local links,173 CMake sources,0 errors. These are scoped checks; no full gate,
stress, physical audio/listening, chooser gestures or fresh-process reopen claim.

Evidence: `.local/milkdrop-20261104` (before/final source archives, dependency
archives/build logs, native/GPU reports, identities and screenshots). Frozen
review candidate: `.local/md01/bundle/Mixworks.exe`, same executable/runtime
identities, imported preset under isolated `data/MilkDrop-v1`; staged only,
not launched. Bridge root is fresh and reserved for this candidate. Model/runtime
assets use canonical hard links; its Python runtime links to the unchanged
activated scenes-90971337 bundle. Keep those canonical files intact.
All2512 original scene/catalogue/reference files are exact. During the task
Jay intentionally removed13 duplicate source Sprites PNGs; every exact original
remains in Backgrounds. Do not restore those duplicates. Native tested/frozen
runtime retains its existing media pool. No original preset file, production
session/data/cache, normal launcher or production PID36396 was changed, and no
commit/push or public release was made.

Limits: live mixed feed is mono from the louder stereo side. projectM4.1.8 uses
its wall clock; pause holds pixels, seeks restart feedback, no deterministic
reconstruction/offline export promised. MilkDrop3 formats unsupported; shader
compatibility is preset dependent. Native looks/images/palettes do not apply;
handovers needing Alloy simulation state use dissolve. projectM's internal
GPU targets are additional to host targetBytes. Preset/texture rights and all
existing public-distribution prerequisites remain separate and unresolved.

**Promo polish and r/vjing post pack — complete locally:** Jay requested stronger
titles and restrained alloy/forge/molten effects. New export:
`.local/p65/deliverables/Mixworks-Alloy-Forge-Your-Mix-60s-v5.mp4`.
Fixed-scale steel/copper Oswald lettering replaces the fly-through zoom, with
short heat sweeps, small extrusion, readable holds and two restrained molten
seam/spark accents. Existing footage, cut timings, two manual arranging gestures,
48 feature views, native visuals and fiery ending are preserved. Original p64/v4
and all 11 checked source identities are unchanged. No application/scene/session
or launcher changes. Serial below-normal editorial render; no native tests.
Full video/audio decode passed: 60s, 1920x1080, 30fps, 1800 frames, 50574832 bytes;
SHA256 `3ad982e561fc7dc00d82550132e815643fc096a656a91bcfa3469a0864daf777`.
Original v4 AAC payload matches exactly; nine untouched pre-encode samples are
pixel-identical. Encoded title/contact/motion frames and phone-size readability
were inspected. Reproducible scripts, before/after checkpoints and verification
are under `.local/p65`; README and receipt are in `deliverables`.

Jay also requested r/vjing posting details/body text. Checked the community's
published rules and moderator announcement on 4 November: Software / Tools
flair, one own-app post per 30 days per person, [AI] for significant AI-assisted
development/content. [Promo] applies to monetisation/payment/donations; moderator
explicitly says no payment option yet does not require it. Both title variants,
ready-to-paste body, source links and posting advice are saved in
`.local/p65/deliverables/Reddit-vjing-Post.md`. No Reddit post/message or public
upload was sent. Existing p64 YouTube copy/thumbnail also apply to v5.

**YouTube promo materials — prepared:** Jay requested a title, description,
keywords/tags and thumbnail for the60-second p64 promo. Copy pack:
`.local/p64/deliverables/YouTube-Promo-Pack.md`, with separate title/description/
tags TXT files. Thumbnail: `YouTube-Thumbnail-Forge-Your-Mix-v1.jpg` in that folder,
1280x720 /395628 bytes; PNG master and exact generation prompt are retained.
Built-in imagegen used the existing copper Alloy poster and actual app footage
as references; the thumbnail's UI panel is an illustrative treatment. Title:
“Mixworks Alloy: Forge Your Mix | DJ Software First Look”. Description accurately
states development status and shown features, with no download/release promises.
Original promo v4 is unchanged; asset hashes/method are in `YouTube-Promo-Pack.json`.
No upload or publishing. Current normal application/launcher activation is below.

**Latest local build/activation — user requested and open:** Jay requested a
build, activation and launcher update on3 November2026. One serial below-normal
build passed. `out/Launch.cmd` now selects
`out/releases/0.55.68-dea9bbdc672f-scenes-90971337/Mixworks.exe`, containing all
current scene refinements through Dunes/Escarpment. Version remains0.55.68 and
executable SHA-256 stays
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
asset digest/bundle suffix90971337 identifies the update. All6250 payload files,
current frozen/source/built runtime and dependency pins were validated. Latest
50-check native visual receipt and scene GPU evidence were reused on the unchanged
binary; no full gate/stress was run. Previous images80 bundle remains intact and
launcher backup is `out/Launch.cmd.bak.scenes-90971337` plus independent evidence.

Fresh `.local/az31` verified protocol/root/PID33788/capabilities,203-scene library,
muted moving native delivery of Ranges/Dunes/Escarpment and exact scene/control
save/reopen. Saved `closed-reviewed` and gracefully closed the owned app. Its
master stayed-60dB and monitor mute was on; this was a delivery check, not listening
acceptance. Dunes native pixels were inspected. Escarpment frame delivery passed;
its pixel capture was occluded by Jay's Explorer promo-folder view, rejected and
removed without taking foreground again. Earlier offscreen Escarpment proof
remains separate. Retain owned excerpt/artwork to reproduce this native evidence.

No production instance was running when switching. New normal PID36396 opened
from the new release with the existing2198-track library, clean Untitled workspace
and transport stopped; normal background source checks resumed. No fixture was
loaded into production, no data/cache reset, no existing session closed. Exact
previous media-library settings were copied. Evidence:
`.local/scenes-activation-20261103/activation-receipt.json`,
`activation-opened-final.json`, source checkpoint and native review files. This
supersedes earlier not-activated notes for the cumulative scene changes. Activation
is user-authorised local delivery, not a new artistic verdict on pending landscapes.
No commit/push or public binary release. Remaining similarity review is unfinished.

**Dunes / Escarpment — implemented, preview ready:** Jay authorised implementing
the accepted comparison direction. Dunes now uses a continuous curved surface,
long lateral ridges and wind ripples with sideways camera drift; Escarpment is a
steep image-relief wall with a top silhouette, low ground and travel along its
face. [Details](../../LANDSCAPE-PAIR-20261103.md). Lucent Ranges and every other
approved scene/reference remain exact, as do source-height shaders, runtime/native
source and launcher. Four scene files changed (two mains/descriptors); two new
authoring recipes. Evidence `.local/landscape-pair-20261103`, frozen `candidate-01`,
before-render baseline `.local/landscape-comparison-20261103/runtime`; independent
before/final source archives and identities retained. Focused mechanism review:
23/0, including bands before common finishing, finite targets, exact pause,
white-source surface/wall separation, two dissolves and independent scene/motion
clock probes. Camera-only travel and fixed-camera dune wind are separately
demonstrated; quiet fixed-camera cliff output is exact. Preview:26/0 across six
24-second logo/chart jobs. Four revised motion sheets,12 looks, isolated movement
probes and encoded samples inspected; all eight anchor samples match the previously
inspected baseline exactly. One serial below-normal build and50 native visual
checks passed; no full gate/stress or native-window rerun. Executable SHA-256
remains `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed assets identify this candidate. The24-second1152x520/30 stereo movie has
720 frames and complete decode: `media/Alloy-Landscape-Pair-Preview.mp4` under the
evidence root. Common Electric neon, identical image/PCM/times per row, current
scene-specific responses and approved unchanged Ranges; labelled offscreen.
No production app/session/data/cache, launcher/release bundle, commit/push or
activation changes. Await Jay's artistic judgement. This supersedes the
comparison-only/no-refinement status below for these two siblings.

**Landscape comparison — complete, no scene edits:** Jay requested Dunes and
Escarpment against approved Lucent Ranges, preserving all approved scenes. The
24-second same-image/PCM comparison uses temporary common Electric neon, with
logo and opaque-chart rows. Dunes remains closest to Ranges: the same faceted mesh
and camera dominate its additional wave. Escarpment has distinct oblique parallax
but still reads as low terrain rather than a cliff. Recommend a future two-scene
refinement: smooth lateral dune ridges/wind travel and a dominant steep wall with
visible travel along it. Lucent Ranges stays exact. **No refinement started.**
[Findings/evidence](../../LANDSCAPE-COMPARISON-20261103.md). Evidence:
`.local/landscape-comparison-20261103`, frozen `runtime`, independent source ZIP
and before/final identities. Every scene/reference/runtime/native source and
launcher remains byte-for-byte unchanged. Six24-second music jobs plus three
8-second silent raw-main camera probes pass41 bounded checks. All six motion
sheets, three silent camera strips and encoded samples inspected. Silent probes
keep image-height targets exact while camera movement changes raw scene pixels;
movement is not inferred from colour cycling. The1152x520/30 stereo movie has720
frames and complete decode: `media/Alloy-Landscape-Comparison.mp4` under the
evidence root, labelled offscreen rendering. No build/native suites/full gate/
stress, app/session/data/cache, launcher/release bundle, commit/push or activation
changes. Assessment covers two inputs/one passage, not all looks or isolated
musical bands. Jay accepted the comparison with “All good. next?” on3 November2026.
Next recommended implementation: refine Dunes and Escarpment as described above,
keeping Lucent Ranges and every approved scene exact. No refinement started.
Feedback is retained separately at `.local/landscape-comparison-20261103/review.json`;
the original comparison receipt/source archive remains unchanged.

**Wake / Press / Fault — approved by Jay:** Jay authorised these three
refinements while preserving Monsoon, Undertow and all approved scenes. Wake now
emits a narrow moving retained jet from one inlet; Press has two separated curved
compression fronts and an empty central corridor; Fault has two broad image banks
sliding across an open jagged boundary. [Details](../../PIGMENT-TRIO-20261103.md).
Every other scene directory/reference, runtime/native source and launcher remains
exact. Evidence `.local/pigment-trio-20261103`, frozen `candidate-03`, previous02;
independent before/final archives, identities and failed drafts retained.
Final mechanism review:31/0, including isolated bands before colour finishing,
finite state, exact pause, retained jet transport/release, separated front feeds,
bass-driven approach, opposed bank slip/open gap and three dissolves. Preview:
26/0 across six24-second logo/chart jobs. Six motion sheets,18 named looks and
encoded samples inspected. One serial below-normal build and50 focused native
visual checks passed; no full gate/stress or native-window rerun. Executable
SHA-256 remains `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed assets identify this candidate. The24-second1440x628/30 stereo movie has
720 frames and complete decode: `media/Alloy-Pigment-Trio-Preview.mp4` under the
evidence root. Identical image/PCM/times per row, common Solar fire and current
scene-specific responses; labelled offscreen rendering. Initial missing Press
front, weak Wake, sustained-tone fixture and central-pigment failures are recorded.
No production app/session/data/cache, launcher/release bundle, commit/push or
activation changes. Jay approved all three with “All good” on3 November2026.
Preserve them in subsequent work. Approval is retained separately at
`.local/pigment-trio-20261103/approval.json`; the original receipt/source archive
remain unchanged. Next recommended comparison: Opaline Dunes / Lucid Escarpment
against approved Lucent Ranges; keep that anchor unchanged. No new batch started.
This supersedes the comparison-only status below for these three scenes;
Monsoon/Undertow stay exact.

**Pigment/weather — comparison complete, no scene edits:** Jay requested the
five-scene comparison while preserving all approved scenes. The24-second same-PCM
comparison uses identical current defaults/Solar fire, with logo and opaque-chart
rows. Monsoon's swirl and Undertow's downward flow separate more clearly with
opaque imagery; leave both for now. Remaining priority is Wake/Press/Fault: their
one-way flow, compression and shear still produce similar broad horizontal bands.
Recommend exposing a narrow jet, two opposing fronts and a slip boundary in a
future three-scene batch. **No refinement authorised/started by this comparison.**
[Findings and limits](../../PIGMENT-COMPARISON-20261103.md). Evidence:
`.local/pigment-comparison-20261103`, frozen `runtime`, independent source ZIP and
before/final identities. All visual assets/references, runtime scripts, native
source and launcher remain exact, including every approved scene. Ten24-second
cases/7,200 rendered frames;42 bounded render/state/pause/movie checks passed.
Ten motion sheets, final comparison sheets, opaque velocity overlay and three
encoded samples inspected. The1920x520/30 stereo movie has720 frames and complete
decode: `media/Alloy-Pigment-Weather-Comparison.mp4` under the evidence root.
It is labelled recorded-input offscreen rendering; one scene job at a time,
below-normal priority. No build, native suites, full gate, stress, app/session/
data/cache, launcher/release bundle, commit/push or activation changes. Artistic
recommendation is bounded to two sources/one passage; all looks and isolated
musical bands remain unassessed. Await Jay's judgement/direction on the comparison.

**Signal scenes — approved by Jay:** Jay authorised this batch while
preserving all approved scenes. Glass Interlace now has opposing temporal columns,
Raster Roulette a rolling acquisition front, Afterimage Static actual staggered
held captures, Circuit Confetti sparse independently drifting islands and
Chromatic Splice broad sliding diagonal cuts. Broken Spectrum and every other
scene directory/reference/runtime module remain exact. [Details](../../SIGNAL-SCENES-20261103.md).
Evidence `.local/signal-scenes-20261103`, frozen `candidate-02`, previous01.
Final GPU review:46/0, including bands before common finishing, exact pause,
opposed travel, old/new source captures, progressive scan rows, exact held blocks,
sparse moving islands, diagonal seams and five dissolves. Five motion sheets,
30 look references and encoded movie samples inspected. One serial below-normal
build and50 focused native visual checks passed; no full gate/stress or
native-window rerun. Executable SHA-256 remains
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
runtime asset hashes identify the candidate. The12-second1440x632/30 stereo
comparison has360 frames, complete decode and the approved unchanged Broken
Spectrum in its sixth panel; identical image/PCM/times throughout. Movie:
`media/Alloy-Signal-Scenes-Preview.mp4` under the evidence root, labelled
recorded-input offscreen rendering. Initial fixture/control and diagnostic
failures are retained. Source archives/identities and exact preservation checks
are independent of Git. No production app/session/data, launcher/release bundle,
commit/push or public release changes. Jay approved all five with “all good” on
3 November 2026. Preserve these scenes in subsequent work. Approval is retained
separately at `.local/signal-scenes-20261103/approval.json`; the original receipt
and source archive remain unchanged. Not activated. This supersedes
signal-not-yet-changed notes in earlier checkpoints.

**Liquid trio — approved by Jay:** Jay requested Satin Vortex as a
deep spiral with one clear centre, Pressure Garden as four separated expanding
wave sources and Oilprint as a flatter thin-film surface with colour shear and
treble shimmer. All three now have distinct main shaders; Garden also injects
local retained waves at its four displayed sites. Mercurial Echo and every other
scene directory/runtime module remain exact. [Details](../../LIQUID-TRIO-20261103.md).
Evidence `.local/liquid-trio-20261103`, frozen `candidate-03`, previous02. Final GPU
review:36/0, including isolated bands before common finishing, exact pause, the
vortex eye, all four pressure sources, outward propagation after a burst ends and
three dissolves. Three motion sheets,18 look references and encoded movie samples
inspected. One serial below-normal build and50 focused native visual checks
passed; no full gate/stress or native-window rerun. Executable SHA-256 remains
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed runtime assets identify this candidate. The
12-second 1440x316/30 stereo comparison has360 frames and complete decode; each
column uses identical image/PCM/times. It is labelled recorded-input offscreen
rendering. Movie: `media/Alloy-Liquid-Trio-Preview.mp4`. Failed initial eye check,
first-draft visual refinements and missing-system-OpenGL attempt remain recorded.
Source archives/identities and reference manifest are retained independently of
Git. No production app/session/data, launcher/release bundle, commit/push or
public release changes. Jay approved all three with “All good.” Preserve the
trio in subsequent work; signal scenes follow, not yet changed. Approval is
recorded independently at `.local/liquid-trio-20261103/approval.json`; the original
verification receipt/source archive remains unchanged. This supersedes the earlier
liquid-batch-not-started note below.

**Stage/glass identities — approved by Jay:** Jay accepted Afterhours'
new lateral travel and asked to continue with the next five scenes. Pulse Gallery
is now a frontal lightbox, Orbit Theatre a moving perspective image plane with
satellites, Resonant Iris an opaque opening shutter. Rhombus Foundry has gliding
diamond glass panes; Enamel Scales has locally hinged curved scales seen obliquely.
Both retain one complete continuous source behind their carriers. All other scene
directories and shared renderer/controller modules remain exact, including every
previously accepted scene and the five original approved image instruments.
[Details/evidence](../../STAGE-GLASS-20261103.md). Evidence root
`.local/stage-glass-20261103`, frozen `candidate-03`, previous02. Main GPU review:48/0
on02; final glass-only follow-up:19/0 on03, with the three stage files identical.
Checks prove source-plane movement/aperture opening before colour finishing,
complete glass/Gallery source regions, pause and targeted live dissolves. All five
motion outcomes and 30 current look references inspected. One serial below-normal
build and 50 native visual checks passed through `verification.run_checks` with
artifact retention enabled and stress disabled; no full gate or native-window rerun. Executable
SHA-256 stays `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed asset identities distinguish this candidate. The 30-second 640x360/30 stereo
movie `media-final/Alloy-Stage-and-Glass-Preview.mp4` has 900 frames; complete decode
and encoded sample inspection passed. It is labelled recorded-input offscreen
rendering. First draft visual failure (undefined negative-base power clipping half
of Gallery) and earlier glass refinement are retained; applicable final evidence
is described in the record. No production app/session/data, launcher/release bundle,
commit/push or public release changes. Jay approved all five with “Ok, passed.”
Preserve them in subsequent work. Recommended next batch: Satin Vortex, Pressure
Garden and Oilprint, preserving Mercurial Echo; signal overlaps follow. No new
batch has started from the “What's next?” question.

**Afterhours travel correction — 3 November:** Jay said Afterhours did not appear
to travel across the city and accepted the rest of the scene refinements. Keep
Dreamline, Graphite/Ember/Opal, the 28 plain-default changes and the five original
approved image instruments as they are. Afterhours' lateral travel multiplier is
now 1.4 rather than .18; the default scene clock previously covered only about
.55 world units in six seconds. Height, forward distance, horizon, geometry and
musical lighting mappings are unchanged. Only its main shader and the CITY_KIND0
authoring recipe changed; all other scene directories/runtime scripts are exact.
Fresh silent pre-finish renders isolate camera movement from colour cycling;
finite state and exact pause passed, and the visible skyline displacement was
inspected. The old general movement check counted lighting changes and did not
prove perceptible camera travel. Correlation estimates were exploratory and not
used as acceptance evidence because repeating windows/parallax produce aliases.
Six Afterhours look references refreshed and inspected. One incremental serial,
below-normal build passed; no native/full/stress suites or app-control reruns.
Evidence and before/final checkpoints: `.local/afterhours-travel-20261103`, frozen
runtime `candidate-01`. Review movie `Afterhours-Before-After.mp4` is 12 seconds,
1280x360/30, stereo; same recorded input on both sides, complete decode and encoded
sample inspection passed. Production app, session, data, launcher and immutable
release bundle remain untouched. No commit/push. The four other revised scenes
and plain defaults have Jay's acceptance; Afterhours' new travel remains ready
for his visual judgement. This supersedes the Afterhours camera-speed assessment
in the earlier checkpoint below.

**Scene identity refinements — implemented, preview ready:** Jay authorised plain
defaults for the 28 sixfold scenes, distinct city/ribbon-feedback scenes and
preservation of the five approved image instruments. Default/Signature/Silk views
are now plain; optional Prismatic reflections remain. Afterhours is a waterfront
skyline, Dreamline a street canyon; Graphite is a calligraphic wake, Ember separate
rising streams, Opal broad billows, with distinct band responses. Exact approved
five scene bytes/looks and shared runtime code are unchanged. [Details and limits](../../SCENE-IDENTITIES-20261103.md).
Evidence root `.local/scene-distinct-20261103`, staged runtime `candidate-01`.
159 GPU checks, 14 runtime tests and 50 native visual checks passed; 86 affected
reference images refreshed. One serial below-normal build passed; executable
SHA-256 remains `dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed runtime assets identify the new candidate. Fresh `.local/sd1` verified
protocol/root/PID25592/capabilities, actual 203-scene selector, muted moving native
delivery of 11 scenes and exact scene/control save/reopen. Inspected native
pixels, including developed smoke, are retained. Saved `closed-reviewed` and
gracefully closed the owned app. Its excerpt/artwork are deliberately retained.
The 30-second, 640x360/30 comparison movie is
`.local/scene-distinct-20261103/preview/Alloy-Scene-Identity-Preview.mp4`;
900 frames, stereo, complete decode and encoded sample inspection passed.
This is recorded-input offscreen rendering; native evidence remains separate.
The native setup's false initial mute assumption stopped before playback; corrected
through its real checkbox and verified by bridge state. An occluded capture was
rejected; retained captures were foreground-validated. No production app/session/
data/cache or launcher/release bundle changes, commit/push or full gate/stress.
Existing saved reflection overrides are not migrated. Other audit groups remain
deferred. This supersedes the audit-only status below for these authorised changes.

**Full scene similarity audit — 3 November:** Jay asked to compare the entire
scene list, starting with Dreamline Metropolis / Afterhours Skyline, before
considering appearance/reactivity changes. [The audit](../../SCENE-SIMILARITY-AUDIT-20261103.md)
records 25 comparison groups and a disposition for all 203 scenes. Every scene
directory matches the activated images80 bundle. Fresh serial 320x180 offscreen
renders sampled all 203 current defaults; endpoint contact sheets and selected
motion strips were inspected. Six plain-view and ten opaque-image/common-palette
probes also completed and were inspected. Main finding: 28 relief scenes default
to the same sixfold finish, obscuring their different geometry; all six plain-view
probes recovered clearer family identity. Other priorities are the city pair,
Graphite Wake/Ember Advection/Opal Smoke, three image stages, Rhombus Foundry/Enamel
Scales, and liquid/raster siblings. Pigment similarity is more input-dependent.
The 48 Priority participants include those 28 defaults and anchors to preserve,
not 48 requested rewrites. Preserve the five approved image instruments, bright
palettes and continuous particle motion. Proposed next preview starts with default
view separation and the closest pairs; **this is an audit only; no scene changes
are implemented**. Evidence/scripts,
hash inventory, independent source ZIP and failed initial packaging attempt are
retained at `.local/scene-audit-20261103`. Older thumbnails were stale in places
and used only for triage. This is short-sample artistic evidence, not full-look,
long-passage, band-isolation, native UI, listening or performance qualification.
No application source/defaults, production app/session/data, launcher, build or
native tests were changed/run; no commit/push. Documentation checks are recorded
with the audit evidence.

**Latest local activation — user requested and open:** On 3 November Jay asked to
update, activate and open the latest build. `out/Launch.cmd` now selects
`out/releases/0.55.68-dea9bbdc672f-images80/Mixworks.exe` (same verified executable
SHA-256 below). No Alloy instance was running before activation, so no session
was closed or discarded. New normal instance PID31572 was opened; actual selector
pixels/accessibility confirm **203 scenes**, including the80 additions, without
development retain controls. Scene library is left open. Existing2198-track
library loaded and normal background source checks resumed. Exact previous media
configuration was copied; no data/cache reset or test fixture loaded. Prior pulse
bundle is retained and the former launcher is backed up at
`out/Launch.cmd.bak.images80` and in the evidence root. Installation validated
6243 payload files and reused the existing scoped receipts; no rebuild/full gate.
`.local/visual-eighty-20261103/activation-receipt.json`, `activation-opened.json`
and `activated-selector.png` record delivery. This supersedes preview-only/no
launcher-activation statements below. No public binary release or commit/push.

**Eighty image instruments — implemented and preview ready:** Jay approved the
five examples and full eighty additions. Source/build now contain **203 scenes /
1,218 looks**, ten new families of eight including those five, renamed Chromaflora,
Kinetic Bloom, Lucent Ranges, Mercurial Echo and Broken Spectrum. Their shader bytes
and look/control values are unchanged. Development retain UI/read/write paths are
removed; review files remain. Automatic shows space recent families/motifs.
[Catalogue](../../IMAGE-INSTRUMENTS.md) and [evidence/limits](../../VISUAL-EIGHTY-20261103.md).

Current frozen runtime is `.local/visual-eighty-20261103/candidate-04`, previous03.
One single-worker build passed; executable SHA-256
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`.
17 runtime units, 50 native visual, 47 arrangement, 480 GPU review and 38 causality/
blend checks passed. Full gate/stress unrun. All80 were visually reviewed;480
actual palette references are installed. The 200-second labelled tour is
`.local/visual-eighty-20261103/tour/Alloy-Image-Instruments-80-Scene-Tour.mp4`;
complete decode passed. Movie SHA-256
`90b1403b9f0574e7e70125fbc91a7a8bdf6d906fbbf680dbf035424f1e0c3fa7`.
Fresh `.local/v80` verified the selector, muted moving native delivery of all ten
families, exact scene/control save/reopen and inspected native pixels. Saved
`closed-reviewed` and gracefully closed the owned app. Retain its own excerpt and
artwork for review. Before/final checkpoints and failure provenance are retained
independently of Git. At that preview checkpoint there were no production session/data/cache changes,
launcher update, restart, commit or push; subsequent activation is recorded above.
New75 need Jay's artistic judgement; do not add further scenes automatically.
Earlier five-preview wait instructions below are historical and superseded.

**Five image generators — preview ready:** Jay approved five examples informed by
Sprinkles, Swarm, Terrain Lattice, Molten and Glitch, plus brighter palettes and
continuous particles, before the eighty-scene expansion. Source now has **128
scenes / 768 named looks**: Neon Culture, Chromatic Shoal, Terrain Prism, Liquid
Memory and Signal Fracture are the five additions. All active scenes have an
independent Colour palette menu; 29 active particle scenes pass through assembly
without a hold. [Implementation/evidence record](../../VISUAL-FIVE-PREVIEW-20261103.md).
The review movie is `.local/image-five-20261103/preview-final/Alloy-Five-Image-Generators.mp4`:
60 seconds, 960×540/30, stereo; complete decode passed. SHA-256
`d1c966bcb9508efe3ac1c7646baf3197592853ee06fb70a450cd975b9d7cd05c`.
Actual encoded passage frames and native pixels for all five were inspected.

Current frozen runtime is `candidate-09`, predecessor `candidate-08` under that
evidence root. Native executable SHA-256 is
`a5ca1ff9605d10246e7a82c4f1f24ac9a50ab9c7da6e401c35549346e4b62c5b`.
One single-worker build, 17 runtime units, 227 focused GPU checks, 41 existing GPU
behaviour checks, 48 native visual and 47 arrangement checks passed. Failed drafts
and native catalogue-entry failure are retained and described in the record.
This is scoped verification, no full gate/stress/reference parity claim.
The fresh `.local/ig1` app verified protocol/root/PID/capabilities, real palette
menu selection, muted native delivery of all five, and save/reopen. Session
`final-reviewed` was saved and the owned app gracefully closed. Its excerpt and
artwork are deliberately retained for review. No production session/settings,
analysis/caches, launcher or release bundle were changed. Existing dirty source
edits remain preserved; no commit/push. **Wait for Jay's preview feedback before
the remaining seventy-five additions.** Motif cooldown and broader surface
materials remain deferred; the future catalogue bound must rise from 160.

**Earlier visual proposal:** Jay wants stronger selectable palettes
including neon, less recurring wavy/vein surface patterning across a mix, particle
motion continuing through image assembly, and about 80 genuinely new scenes in
8–10 new families. [The ten-family proposal](../../VISUAL-EXPANSION-203-PROPOSAL.md)
defines 80 concepts, taking 123 to 203 scenes, independent palette/material
choices, motif-aware automatic selection and continuous particle crossings.
At that proposal checkpoint no runtime/source behaviour changed and no
build/tests or app control occurred. The first
suggested implementation milestone is one moving representative per family plus
the cross-catalogue foundations. Existing dirty source edits were preserved.
Pre-edit continuity documents are retained in
`.local/visual-expansion-proposal-20261103` independently of Git.

**Reference follow-up:** Jay named twenty preferred Synesthesia scenes using
images, now listed in the proposal. Use this to prioritise genuine image-driven
generation within the expansion; initial mechanical/type/shadow families remain
contrast candidates, and the proposed eighty-scene allocation needs refinement.
Earlier installed-scene research is in `docs/commercial/SYNESTHESIA-SCENE-INDEX-20261102.md`.
It describes mechanisms, not current parity or artistic acceptance. No new GPU
render, build, test or production app control occurred during this assessment.

**Active p64 promo revision:** Jay requested centred type that continuously
passes through the camera, JAYLABS PRESENTS over the two-wave intro with a visual
crossfade, twice-speed/48-shot feature montage, more visualiser cuts, and his
supplied four-second copper ALLOY ending. He explicitly authorised using and
changing the actual open instance. New captures are from production PID 6280,
release `0.55.68-48506fdd360f-pulse`, not the initially created isolated p64 app
(which was closed). Before/after session checkpoints are retained under
`.local/p64/sessions/open-instance-{before,after}-filming.mixworks`; the current
session was saved at `C:/Users/jayte/Documents/p.mixworks`. Library scanning was
left running. Monitor mute restored off, transport stopped, production app open.
Filming added Pro-Q3 to Sonic Barrier with a small bell curve, unlinked/moved
Counterfeit and Jackers Revenge independently, and enabled the alignment guide.
Jay explicitly requested real manual track movement footage. Window GDI capture
missed redraws and was rejected; the corrected display capture records both real
drags. Only the Alloy monitor is used in the final. The two 1.5-second excerpts
are slowed to 2.25 seconds each, at 12–16.5 seconds. Forty-eight unique UI views
cut every seven frames at 32–43.2 seconds; twelve one-second native visual clips
follow. The supplied ending's audio is discarded. Original harder Sonilo theme
only; no narration/ducking or new generation. `.local/p64/edit-promo.py` and
`prepare-media.py` retain the edit. Completed preview: `.local/p64/deliverables/Mixworks-Alloy-Forge-Your-Mix-60s-v4.mp4`.
Title motion, 48-shot contact sheet, encoded individual-move frames and closing
were inspected. Exactly 60 seconds / 1800 frames / 1080p30, stereo AAC; complete
decode passed. Original-music correlation 0.999808, gain -1.518 dB, true peak
-0.4 dBFS, integrated -12.8 LUFS. 52,656,267 bytes; SHA-256
`fe765daf9b316e19025e5bfdf4ffecb02259b293338e5ba4fa171b86eb282a05`.
One failed render exposed a 134-frame manual clip; the final captured frame was
extended by one frame to supply the required 135, then rendering/verification
completed. Evidence in p64/evidence/render-retry.json. No app source changes,
build/tests or public upload. Earlier p63 is preserved and superseded by p64.

**Latest promo direction:** Jay rejected the voiceover (including replacing it
with another voice) and requested text-only, minimal branding until the ending,
and centred text fading in and expanding from the distance. The active revision
is `.local/p63`, retaining the accepted harder Sonilo music directly, without
narration or ducking. Top-left headers, counters and explanatory subtitles are
removed; 24 feature cuts remain, with no captions over the montage. The copper
logo artwork is reserved for the closing reveal. The earlier p62 narrated film
is superseded but preserved. Two supplied 60.029-second MP3s from Downloads are
copied under p63/audio with hashes in `evidence/new-tracks.json`; they are not used
in this edit and their tempo/key compatibility is unverified. No new generation
or credits, production UI actions, or application changes. The completed preview is
`.local/p63/deliverables/Mixworks-Alloy-Text-Only-60s.mp4`: 60 seconds, 1080p,
30 fps / 1800 frames, stereo AAC, 39,869,799 bytes. Complete decode passed; title
motion samples, contact sheet and encoded Auto Arrange frame were inspected.
Audio matches the original music (correlation 0.999808, measured gain -1.518 dB
over 59 seconds), confirming no narration/ducking; true peak -0.4 dBFS.
SHA-256: `011b0591611f60fc325cefec516e966b5eb05544b28200268d1e6070a7c762cb`.
Edit sources, provenance and verification remain in p63. No public upload.

**Promo revision — Forge your mix:** Jay requested stem separation, Auto Arrange,
a rapid menu/tab/feature montage, existing text artwork excluding Jay Martooni,
and ideas from his copper/graphite script. He selected sparse voiceover. Work is
isolated in `.local/p62`. Real separation completed four validated stems from a
30-second harder-theme excerpt; those were applied and captured. Auto Arrange
completed three requested tracks and a 60-second arrangement with visual scenes.
Sessions `stems-captured`, `auto-generated` and `forge-filming-complete` preserve
the results. The owned filming app was saved and gracefully closed; production
instances/data were untouched. No application source changes were needed.

The revision retains the accepted harder Sonilo soundtrack rather than adopting
the reference script's 128 BPM. One ElevenLabs British narration take (voice
`DEFpwxCUkrj3WAbTDRTZ`, generation `bEQxFQTQPmj3aKfJEzEI`, 196 credits) supplies five
sparse phrases. The edit includes 24 rapid feature cuts, actual prior Controller
playback/native GPU footage, and JAYLABS/MIXWORKS/ALLOY artwork. Editorial sparks
and converging waves are trailer graphics; product UI remains actual captures.
Jay offered Summer Engine audio credits if needed; none were used. Blender/Unity
were discussed as optional effects tools and were not used. Render, provenance,
optional captions and verification scripts are retained under `.local/p62`.
The revision is ready for creative review:
`.local/p62/deliverables/Mixworks-Alloy-Forge-Your-Mix-60s.mp4`.
Exactly 60 seconds / 1800 frames, 1920x1080 at 30 fps, stereo AAC; complete decode
passed. Encoded Auto Arrange/closing frames and contact sheets were inspected.
Audio measures -13.7 LUFS integrated and -1.0 dBFS true peak. File size is
30,114,722 bytes; SHA-256 is
`8543c68ce26adaa0a9af7786dd315213611482664f08833635f8a8e2ab480e1c`.
No subjective listening acceptance is claimed. No public upload was made.

**Active follow-up:** Jay expanded the promo to a full 60 seconds, showing menus,
controls, transition presets, editing and visuals with cuts/flows timed to the
harder Sonilo theme. Before production he requested the Controller follow timeline
automation, and explicitly selected a Follow timeline mode with named editable
track targets. Follow timeline is implemented: source level/EQ/filter/pan controls
follow the playing track, with selected-track priority during overlaps, named
owners, lane fallback and gestures pinned to their original owner. Master stays
master-owned; physical MIDI mappings and mute/solo remain lane-owned.
The single-worker build and all 25 focused `--audio-ui-only` checks passed through
`verification.run_checks`, with no stress/full gate. Candidate SHA-256 is
`48506fdd360fb50e0e3ec3f5dfe64a489280cdd600d722bec8ecda8738a66ab0`.
Evidence/source checkpoints are in `.local/controller-follow-20261103`.
Actual toggle operation and moving controls were inspected during muted playback
in the isolated `.local/cf1` preview, including a +9 dB source fader value.
Screenshots/state are in `.local/cf1/evidence`; its owned soundtrack excerpt is
deliberately retained for review. The preview was saved as `follow-final-review`
and gracefully closed. This does not establish physical MIDI or musical
listening acceptance. No engine/DSP or saved-format changes were made.
Jay authorised activation and the full promo. The launcher now selects
`out/releases/0.55.68-48506fdd360f-pulse`; existing production processes were not
restarted. The executable is the focused-tested candidate above. Activation and
payload receipts are in `.local/controller-follow-20261103`.

Jay requested renaming Beats in 4D because that was its Synesthesia name. Its
display name is now **Dimensional Pulse**, verified in the native browser and
timeline. The existing internal key and CC0/author attribution remain intact.
This is catalogue metadata only; the prior successful build remains applicable.
The renamed runtime is included in the activated bundle.

The promo uses the original 60-second harder Sonilo theme as its soundtrack.
Jay rejected demonstrating the 120/144 BPM pair; corrected UI footage in
`.local/p61` uses two sections of the harder theme instead. `.local/p60` UI
footage is superseded and must not enter the final cut. Its native GPU output
contains no old scene-name text and remains usable. The corrected filming session
was saved as `promo-filming-complete` and closed gracefully. Actual Split/Undo,
presets, automation following, scene library and named looks were captured.
Native visual output and editorial sources are retained under `.local/p60` and
`.local/p61`. The finished film is
`.local/p61/deliverables/Alloy-Forged-in-Sound-60s-Promo.mp4`: exactly 60 seconds,
1920x1080, 30 fps / 1800 frames, stereo AAC, 50,952,061 bytes. SHA-256:
`d1375933e35f4f2b2479bc84ae727e4d6ec46a449a2520bd9033637a2452b37d`.
Full decode passed; frame count/duration/audio streams were checked. Contact sheets
and an encoded Controller frame were inspected. The UI sequence includes genuine
stills and timestamped playback captures edited to 1.5x; full-screen scenes are
actual Alloy GPU output rendered offline from the released runtime. No subjective
listening acceptance is claimed. The original soundtrack is reduced 1.5 dB with
a final 0.3-second fade; no replacement music or reconstructed app UI is used.
Edit timings, provenance, poster and verification are beside the film.

Jay suggested 1–2 more tracks in the same key at 144 BPM. Sonilo account inspection
found both text-to-music trials consumed and zero balance; no new generation was
started. The suggested E-major brief follows Alloy's current key label, not an
independent musical verification. AI prompt tempo/key is not acceptance evidence;
verify any later companion before treating it as a matched pair. No replacement
local music synthesis was attempted.

Jay requested translucent mosaic scenes that refract a complete image behind
moving tiles, followed by an obsolete-document/test audit, a new local app version,
launcher activation and a source commit pushed to Jaytecit/Mixworks-Alloy.
Version **0.55.68** was built and verified for the preceding glass update. Its
previous local launcher selected
`out/releases/0.55.68-110a215dcfdd`; executable SHA-256 is
`110a215dcfdd16fb8cc4ba82dfcbc086c1e3ccedd7fc00fc2e4323052f29cd8d`.
Evidence and the activation receipt are under `.local/glass-release-20261103`.
The previous `0.55.67-60c6494debe5` bundle remains available. Existing production
processes were not restarted; the updated launcher applies on the next launch.

The active library has **123 scenes / 738 named looks**. All 61 retained choices
remain in the active catalogue; retired registries and source remain intact.
Mosaic Kiln and Spinning Stamps offer a **Glass image** look. Rhombus Foundry and
Enamel Scales use continuous-image refraction by default. Their tiles distort
one image that remains visible between tiles. The opaque tiled looks remain.
See [visual direction](../../VISUAL-DIRECTION.md) and
[maintenance/activation record](../../MAINTENANCE-20261103.md).

## Accepted visual direction

Movement must be the main musical response, not only brightness or colour.
Particle trajectories must respond to music and periodically reassemble into
one image. Use more solid/geometric compositions in the direction of Beats in 4D;
automatic shows allow at most one particle slot in five when alternatives exist.
Whole images should often remain readable, pump to the beat and receive different
scene effects. Mosaic tiles may each carry a complete image, or refract a complete
image behind them. Fragmentation and nested imagery are optional treatments.
Quieter scenes benefit from kaleidoscopes. Strong cycling and beat-driven
recolouring applies to images and scene geometry. Extended track joins use live
blends; fragment/reveal handoffs are reserved for short transitions. Artistic
acceptance remains Jay's, independent of numerical tests.

## Verification and continuity

The 0.55.68 single-worker build passed. Focused checks passed: 17 visual runtime
unit tests, 45 native renderer checks, 47 native arrangement checks, 41 authored
GPU behaviour checks and 16 selected preset/method checks. Both retired selectors
returned the expected refusal, before suite setup. Current document/CMake links
passed. Native reports used the shared retention harness; no stress/full gate ran.
The 24-second glass preview and native browser pixels were inspected. Isolated
`.local/gla1` opened, saved, reopened and gracefully closed the four-scene fixture
with playback stopped. Its owned excerpt/images are deliberately retained for
review. Production media folder settings were copied to the new local bundle.

The preceding motion checkpoint is `.local/motion-20261103/candidate-04`, executable
`e8384e406ba4083452d237003bfde7289a202d91d64c893571c01c12829b5cbf`.
It passed 14 runtime unit tests, bounded GPU checks and 45 native checks through
`verification.run_checks`. Its soundtrack movies show seven scenes and long live
blends. See [motion record](../../VISUAL-MOTION-20261103.md). The isolated
`.local/mot1` review was saved and closed without starting playback.

The current glass/audit checkpoint preserves the entire previous publishable
worktree in `.local/glass-release-20261103/before-source.zip`. The removal plan
contains original paths, SHA-256 hashes and reasons. Obsolete duplicate documents,
external-renderer tests and one-off scene proof/generator scripts were removed
only after verifying their archived bytes. Original failed evidence, historical
receipts, user requirements and licensing/provenance records remain available.

Maintain `scripts/stage_visual_runtime.py`, `scripts/verify_visual_behaviour.py`,
`scripts/verify_scene_looks.py`, the three catalogue preview renderers and runtime
unit tests. Native `--original-visual-only` covers the actual owned renderer;
`--visual-arrangement-only` uses current catalogue fixtures and current save
precision. Removed external-renderer selectors must not be reintroduced.

Builds use one worker, `/MP1`, below-normal priority and no numeric RAM guard.
Tests run serially through the retention harness. Use essential focused checks
under Jay's responsiveness override; no automatic stress/capacity/full regression.
A local launcher update is not a public package or complete-regression claim.

## Protected inputs and running work

Use this folder's Git repository, not the parent REA repository. `.local/`, build,
out, music, sessions, caches and original user artwork remain outside commits.
Never stop/restart a running production instance just to activate a future-launch
bundle. Preserve AppData, analysis, corrections, audio caches, library/recovery,
original music, and protected DownbeatReferences-v1 plus independent dated baselines.
This visual/maintenance update requires no detector reset or session migration.

The 64 extracted transparent sprite PNGs remain in `visuals/original/Sprites`;
source/crop/hash evidence is `.local/sprite-extraction-20261103`. Main app image
folder settings must be copied when staging a new release. Never import isolated
review retention checkboxes into production settings.

Use the [development bridge](../../DEVELOPMENT-BRIDGE.md) with a fresh short owned
`.local` root, verify protocol/root/PID/capabilities, wait for asynchronous outcomes,
save before close, and inspect pixels. Do not retry mutations after timeouts
without inspecting their actual outcome.

## Other work and limitations

Constant-tempo 4/4 BPM/pulse/downbeat alignment remains the music priority.
The source A/B/C correction workflow and source-save semantics are described in
[Source grid alignment](../../SOURCE-GRID-ALIGNMENT.md); protected manual evidence
is documented in [Downbeat references](../../DOWNBEAT-REFERENCES.md).
Prior audio/grid/export/readiness results are dated evidence in
`docs/release-audit/`; they do not establish current complete acceptance.
Hardware/ASIO/FLkey/private-output listening, independent detector accuracy,
sustained performance and clean-environment distribution remain unqualified.

Custom libraries/crates were explicitly deferred on 2 November: plan later,
implement later. Preserve the full P01–P56 backlog in [the roadmap](../../../FINISHING-PLAN.md)
and original requirement detail in the retained history files. Manual/help work
comes after workflows settle. Do not resume old bulk-recovery or detector-reset
instructions from historical reports.

The [Personal Beta Licence](../../../LICENSE) covers original work; third-party terms remain.
Public binaries are blocked by [licensing prerequisites](../../LICENSING.md).
Source commits and local technical checks do not establish distribution clearance.

## Promotional teaser in progress — 3 November 2026

Jay requested an original theme and a ten-second teaser using actual Alloy visuals
and a brief interface view. Isolated workspace/evidence: `.local/pt1`; production
sessions remain untouched. The 30-second Sonilo theme, Signal Into Motion, is
downloaded and playable. The second, 60-second harder techno theme, Forged in
Sound, is also complete in `deliverables/` as M4A and listening MP3. Complete-file
decode passed; generation metadata and hashes are in `evidence/hard-techno-generation*.json`.
Accepted direction is in `evidence/creative-direction.json`. The isolated app was
saved as `theme-review-checkpoint` and gracefully closed; the source screenshot
still shows the first theme. No subjective listening acceptance is claimed.
Jay explicitly rejected switching to locally synthesised replacement music.
Sonilo API-specific guidance permits commercial marketing use separately from
the website Free-plan rules; the correction/source is in `evidence/music-provenance.json`.
This original teaser brief is superseded by the full 60-second film above.
Preserve the original music and both alternatives.

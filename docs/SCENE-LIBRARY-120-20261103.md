# Scene library expansion and named looks — 3 November 2026

Jay authorised another polish pass across the retained catalogue, executable
presets, and continued autonomous work until at least 120 distinct scenes can
use the shared methods. This supersedes the historical 100-scene target and
one-scene-only milestone restrictions. Artistic acceptance remains Jay's.

## Current candidate

120 active entries: the 61 retained choices, the three direction scenes, and 56
new compositions in 14 groups. The retired 66 entries remain retired and their
source remains intact. Presets are not counted as additional scenes.

Every scene has six named looks: Signature, Silk drift, Percussive, Faceted
motion, Woven images and Prismatic. Each stores executable, validated controls.
The library picker shows the look's captured reference image and applies it when
using Add at playhead, double-click or dragging. Search matches names and families.
Right-click a timeline scene for Preset, View method and Image method. A preset
replaces that clip's controls and automation with its authored look; changing
one method retains the other settings and removes automation only for that
control. Both operations support Undo. Values persist in the existing scene
control representation; no document-format change is needed.

View methods: original, mirror pair, four-way reflection, six-sector and
ten-sector kaleidoscope. Image methods: native material, flow, turning tiles,
travelling ribbons. The common display stage supports every scene, including
simulation scenes; it does not feed a transformed view back into simulation
state or change the camera. Image methods act inside material sampling.
Separate bass/mid/treble response, release momentum, image movement and bounded
highlight glow are implemented, not prose-only mappings.

## Visual work

Retained scenes receive controlled band response and follow-through. Fluid
families have longer release; particles keep clearer open compositions with
four selectively reflected defaults, stronger scenery and less oversized
fragments. Their nine settings remain visibly distinct. Every active preview
is regenerated from actual GPU pixels with authorised artwork and music.

The new scenes use woven strata, folding architecture, lanterns, shutters,
porcelain ridges, contour terraces, stained rotors, ribbon routes, crystal
fleets, optical instruments, branching relief, falling curtains, paper theatre
and mosaic relief. Four topology recipes per group are documented in
`visuals/original/scene-diversity.json`. They share a layered analytic relief
renderer; they are not 56 separate simulation engines. Nested image charts
inherit their carrier and turn independently. Integrated band phases produce
travel and follow-through rather than returning to the pre-hit position.

First image review found overly similar particle kaleidoscopes, flat image-led
terraces and an overly empty optical ring. Candidate 02 reduces default
reflection, strengthens particle scenery, separates terrace image domains and
uses multiple focal rings. Candidate 01 remains as independent evidence.

## Final candidate and evidence

Independent source ZIPs, frozen runtimes and reports are under
`.local/scene-expansion-20261103`. **Candidate 05 is current**; candidate 04 is
the preceding working runtime. The native executable SHA-256 is
`60c6494debe55c7f7bf2fc8c35776e828689a84c43da897485d70b679f647599`.
The build output has the final library, but the production launcher/release and
existing apps were not replaced or restarted. The isolated review app is
`.local/lk121`, saved as `sessions/final-review.mixworks`, stopped, muted and
showing Scene overview. Its excerpt/artwork are deliberate local review assets.
The earlier `.local/lk120` review was saved and closed gracefully.

All 720 reference images are installed in the source, build and candidate library.
Their manifest identifies each capture's runtime and scene source. Candidate 03
corrected paused material changes without restarting positional simulation;
candidate 04 corrected compressed kaleidoscope sectors. Candidate 05 raises
shaded-face readability in eight contour/paper scenes, with 48 recaptured looks.

Verification was serial and scoped, with single-worker below-normal native builds:

- `unit-final.log`: **14 runtime tests pass**.
- `looks-final`: **480 checks / 0 failures** across all 120 scenes. Each has
  six distinct preset outputs and 20 distinct method pairings, finite pass pixels,
  nonblank output and exact pause preservation. This is bounded 320×180 coverage.
- `looks-surface-final`: **32 / 0**, rerunning all looks and method pairings for
  the eight final shading edits. Other runtime modules and scene shaders are unchanged.
- `material-edit-checks.json`: four paused simulation workflows change captured
  material while preserving non-material state and subsequent paused pixels exactly.
- `native-checks-final`: **34 / 0**, through `verification.run_checks`, including
  preset placement, independent methods, invalid controls, Undo and actual save/reopen.
  The first native failure was a test fixture's 15-digit JSON writer; the test now
  uses production Save's 17-digit representation. Original failure evidence remains.
- Native mouse workflow in `.local/lk120`: chose Prismatic, inspected its reference,
  added at playhead, applied Mirror pair, checked exact saved controls, Undo and
  reopening. Typed single-letter search filtered the catalogue. Automated bulk text
  entry was unreliable; it is not claimed as a full keyboard/search acceptance test.
- `.local/lk121/evidence`: final muted live playback and paused contour/paper
  inspection. Pixel delivery used the shared GPU texture path at 720p; short observed
  runs do not establish sustained performance or physical monitor scanout.
- Six `tour-01b`, `tour-02` through `tour-06` captures cover all 120 scenes at
  960×540/30 fps with real soundtrack excerpts and live outgoing transition checks.
  Three moving samples per scene were inspected in `review-all.jpg` sheets.
  `tour-contours-final` and `tour-paper-final` supersede the eight changed examples;
  `tour-presets` shows all six Crosswoven Silk looks. These are offline captures,
  not recordings of native delivery. All pause/transition assertions passed.
- Static 1080p target allocation peaks at **51.42 MiB**, below 128 MiB per scene.
  This is allocation accounting, not a capacity benchmark.

The first 30 paused-material failures remain in `looks-01`; successful corrections
and later source identities are separate evidence. `selection-preservation.json`
records unchanged retained/retired registries and no overwritten pre-existing new
scene folders. `final-source.zip`, `candidate-05-identity.json` and
`completion-evidence.json` preserve the final work independently of Git.

This is a preview ready for Jay's visual judgement, not a public release or complete
regression receipt. No full regression, stress, audio-device or musical listening
acceptance was run. The 56 new scenes share 14 relief mechanisms with four topology
recipes each; they do not represent 56 independent simulation engines. Further
artistic refinement should follow Jay's reactions to these actual moving previews.

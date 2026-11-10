> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# Wake / Press / Fault refinement — 3 November 2026

Jay authorised refining Iridescent Wake, Nebula Press and Chromatic Fault after
the [same-input comparison](PIGMENT-COMPARISON-20261103.md). Chroma Monsoon,
Saffron Undertow and every approved scene remain unchanged.

| Scene | Result and musical response |
| --- | --- |
| Iridescent Wake | A single moving inlet emits a narrow retained image jet into open space. Bass changes inlet width/emission/forward velocity; mids move the inlet and bend the stream; treble adds fine perturbation and edge light. Old colour travels away and fades after input ends. |
| Nebula Press | Two separated curved compression fronts carry complementary source halves. Bass brings both fronts inward and broadens them; mids crease them vertically; treble lights their local ridges. The central corridor stays empty, including in the retained pigment state. |
| Chromatic Fault | Two broad image banks slide across one open jagged boundary. Bass opens the gap, mids accumulate opposing bank slip and retained shear, and treble lights the fault lips and changes re-acquisition. |

Only these three scenes' main/vector/pigment shaders and descriptors change:
**12 scene files**. Four new `shared/pigment-trio-*.glsl` authoring fragments expand
into their three-pass shader sets using `PIGMENT_KIND` 0–2. The existing pass
graphs, resolutions, state contracts, catalogue keys, renderer/controllers and
native source stay exact. These are authored retained-pigment effects, not a new
physical fluid engine. Older image-flow recipes must not overwrite the revised
instances. Defaults and Signature response controls are updated; the other named
looks retain their explicit control values. There remain 203 scenes/1,218 looks.

## Verification and evidence

Evidence root: `.local/pigment-trio-20261103`; final frozen runtime `candidate-03`,
previous `candidate-02`. Independent before/final source archives, target-reference
backup, identities, reports and failed attempts are retained. Every other scene
directory and reference image is checked byte for byte, explicitly including
Monsoon, Undertow, the five original approved image instruments and all later
approved city, feedback, stage/glass, liquid and signal scenes. Runtime Python,
native source and launcher identities also remain exact.

The final focused GPU review passes **31 checks**: recorded-input motion, finite
retained states, exact pause, independent bass/mid/treble causality before the
common colour finish, three targeted dissolves and solid/coordinate-source
mechanism probes. A white-to-black inlet switch proves real retained Wake dye
transport after new colour ends: its horizontal centroid moves from .199 to .724,
while total dye falls from 171 to 51 in the sampled interval. The developed white
jet occupies about 10.5% of the frame at the stated brightness threshold. Press
has two retained feed regions and an open central corridor; pulsed bass moves
their displayed centroids from about .299/.707 to .320/.685. Fault's coordinate
ramp moves in opposite directions on its two banks, with a dark exposed gap.
These are bounded behavioural checks, not physical-fluid or listening acceptance.

The first draft hid Press's left front because `pow(dx,4)` is undefined for
negative GLSL bases; explicit multiplication fixes it. Wake was also too faint
and thin in the logo preview, so its emitter and brightness were increased. The
initial sustained-tone Press fixture was changed to pulses to test attack-driven
movement. The second review then caught retained pigment and a soft highlight
filling Press's centre; the final state/display both clear that corridor. Original
failed reports, source candidates and pictures remain alongside final evidence.
No additional optional polishing pass followed the successful checks.

The longer preview passes **26 render/state/pause/movie checks** across six
24-second jobs: three scenes with the logo, then the opaque chart. All columns use
the same image and exact recorded PCM/times, the same Solar fire palette and their
own current response defaults. The movie is 24 seconds, 1,440×628/30, 720 frames,
stereo: `media/Alloy-Pigment-Trio-Preview.mp4` under the evidence root. Complete
decode and stream/frame counts passed. It is labelled offscreen rendering. All
six motion sheets, all 18 current named-look references and actual encoded movie
samples at 4, 12 and 22 seconds were visually inspected. Only the three targets'
18 references and their new manifest are installed.

One serial, below-normal build and **50 focused native visual checks passed**
through `verification.run_checks`, retaining reports/hashes while generated test
fixtures are cleaned and stress stays disabled. The executable SHA-256 remains
`dea9bbdc672fc16b1e724ec54f4eb9ccbb60aee369d07891eeeb0ad254081522`;
changed asset identities distinguish this candidate. Native-window delivery,
performance, all-image/all-passage, listening, full regression and stress coverage
are not repeated. No production app/session/music/data/cache, launcher or immutable
release bundle changes; no commit/push or public release. Jay approved all three
with “All good” on 3 November 2026. Preserve these accepted scenes in subsequent
work. Approval and updated continuity notes are retained separately under the
evidence root; the original verification receipt/source archive is unchanged.
Not activated.

The existing serial renderer and same-input workflow, already assessed against
the requested skills.sh method check, remain suitable. Impeccable's scoped
refinement principles informed the work; its web markup detector does not apply
to these GLSL assets. The extra correction addressed concrete failing geometry
checks rather than opening another styling cycle.

# Visual Studio scene rulebook

The scene requirements below describe the original expansion. For the current
catalogue and authoring controls, see the [Studio guide](../visual-studio/README.md).

Build 200 additional Studio scenes as **25 families of eight**. Each family has
one anchor scene and seven siblings. The existing Studio scenes are comparison
references and sit outside the count. Start from original Studio GLSL and
descriptors. Keep family relationships in the designs and reuse code where the
renderer allows. The family names and variant titles below are working names.

## Rules for every scene

1. **Give the empty stage a subject.** At default settings with media cleared,
   render a complete procedural composition. Put intentional structure or
   atmosphere in all four quadrants and carry it toward the edges throughout
   the scene's main cycle. Use quieter areas to give the eye somewhere to rest.
   A central effect plus a flat fill or a vignette fails. Quiet and paused
   frames should still read, and the field itself should visibly respond when
   the music returns. Judge this in Studio's full 16:9 output.
2. **Make artwork change what the scene does.** Use Nested alpha, edges, luma or
   colour to drive a scene-specific operation such as opening passages, bending
   geometry, casting moving shadows or redirecting stored material. The effect
   should be easy to spot at default strength in playback. An intact central
   image, tint or the global sprite motion alone fails. Honour Studio's media
   placement and injection. Choose the input deliberately, such as alpha for
   an outline or brightness for a height field. Give opaque art a meaningful
   outcome for that input. Preserve the procedural composition when the input
   is cleared or reduced to zero.
3. **Give new art a readable onset.** A live swap should change the next
   injection or interaction, by the next relevant musical event at the latest.
   Existing state can decay, travel, or age out on a stated rule. Stateful
   scenes need a working reset. Give the main artwork effect a scene-specific
   strength control when Studio's global injection does not already serve it.
4. **Assign music a physical action.** Map a band or hit to a visible change in
   shape, spacing, route, depth or state. Give the response an attack and a
   release that suit the subject. Continuous envelopes and discrete hits should
   have distinguishable roles when both are used. Vary the lead band and action
   across the library. Repeated whole-screen flashing fails as the main action.
5. **Make families distinct and siblings recognisable.** Keep one family
   invariant, such as the subject, material or motion law. For every sibling,
   author a change to both the default spatial structure (layout, topology,
   silhouette or view) and the main action (direction, timing, trigger or
   interaction). Vary the Nested mechanism within the family too. Change what
   the shader reads, what it drives, or how the contribution moves or persists.
   Each sibling should have a different answer to what the artwork becomes or
   does. Palette, seed, mirroring and speed can support those changes.
   Compare the most similar pair with no art and a common source, in colour and
   grayscale. If they still read as the same scene in motion, redesign a sibling
   before counting it. Shared camera, glow, symmetry or feedback can erase a
   real difference. Check the authored default before adding a finish.

Use [Studio authoring](../visual-studio/README.md#authoring) for supported
inputs, passes and controls. Prefer a single pass when it can express the
scene. Allocate persistent passes where history is part of what the viewer
sees. Compare against the seven existing Studio scenes and the failure patterns
in the [earlier Alloy similarity audit](SCENE-SIMILARITY-AUDIT-20261103.md).
If a design stalls, use Synesthesia for a specific behaviour to study, such
as how REDRUM sheds pigment or how Biopsy_V2 lets a field keep evolving. The
[Studio handover](../HANDOVER.md#current-outcome) records those references.
Translate the behaviour into original Studio composition, motion and code.

## Family roster

The leftmost brief in each list is the anchor. Each brief combines a spatial
idea with an action. Siblings share the procedural subject and vary what the
artwork does. The Nested examples in the roster describe the anchors. The group
headings organise the roster for comparison.

### Terrain and built space

| ID and family | Procedural subject, music and Nested effect | Eight scene briefs |
| --- | --- | --- |
| F01 Fault Ledger | Wide strata buckle on bass. The anchor routes fractures along the artwork's alpha boundary. | Ridge Unzip · Basin Collapse · Paired Fault Slip · Terrace Ratchet · Diagonal Shear · Island Breakaway · Crosscut Relay · Shelf Rejoin |
| F02 Lantern Arcade | Deep alleys and side passages carry light on mids. Artwork cuts windows into walls and casts shafts across the street. | Long Arcade Sweep · T Junction Relay · Courtyard Flood · Elevated Walk Flicker · Underpass Slit · Corner Spill · Paired Alley Chase · Roofwell Descent |
| F03 Paper Mechanics | A relief of scored sheets opens on percussion. Artwork sets crease positions and changes which faces flip into view. | Fan Unfurl · Box Inversion · Map Gatefold · Corner Peel · Accordion Advance · Pocket Turn · Pleat Collapse · Saddle Twist |
| F04 Sediment Clock | A wide cutaway exposes bands compacted by low-end swells. Artwork deposits grains that displace future layers. | Delta Prograde · Shelf Slump · Core Stack · Fan Bury · Canyon Refill · Bar Migration · Levee Breach · Bed Scour |
| F05 Dune Telegraph | Wind pushes sand across a broad overhead plain on sustained mids. Artwork makes windbreaks and reshapes their lee dunes. | Crescent March · Forked Slipface · Corridor Scour · Lee Wake · Double Dune Collision · Edge Drift · Star Dune Feed · Ridge Overtake |

### Weather and living systems

| ID and family | Procedural subject, music and Nested effect | Eight scene briefs |
| --- | --- | --- |
| F06 Rain Reedbed | Springy reeds cover a wet field. Sharp hits launch bending fronts. Artwork pins or parts stems and the opening travels with the gust. | Bank Gust · Split Channel Bend · Island Shelter · Diagonal Comb · Floodplain Flatten · Crossing Squalls · Furrow Ripple · Perimeter Sweep |
| F07 Salt Weather | A mineral crust spreads in angular fronts as sustained highs increase growth. Artwork sets where crystals branch or stop. | Shoreline Freeze · Needle Fan Advance · Branch Confluence · Salt Pan Drydown · Bridging Crust · Facet Collision · Pinhole Bloom · Frost Retraction |
| F08 Plankton Census | Several schools traverse a wide water column. Mid swells change cohesion. Artwork acts as refuge or obstacle and changes the school routes. | Estuary Sorting · Reef Bypass · Baitball Escape · Layer Migration · Counterstream Crossing · Shelter Handoff · Channel Split · Eddy Reunion |
| F09 Fumarole Field | Ground vents release layered plumes on low hits. Artwork opens, seals or deflects the vents, changing which plumes meet. | Ridge Jets · Basin Chimneys · Crack Vent Crawl · Geyser Pair · Hollow Backdraft · Shelf Plume Merge · Offset Vent Chase · Crosswind Smear |
| F10 Storm Isobars | Overhead air masses roll across the frame. Bass changes pressure and highs fray the fronts. Artwork raises local pressure and reroutes the boundaries. | Squall Bow · Triple Point Merge · Dryline Intrusion · Polar Wrap · Occluded Front · Gust Ring Escape · Jet Streak Split · Convergence Sweep |

### Materials and optics

| ID and family | Procedural subject, music and Nested effect | Eight scene briefs |
| --- | --- | --- |
| F11 Caustic Basin | An overhead basin is lit by moving caustics. Low hits deform the refracting surface. Artwork changes local lens curvature and shifts the projected light. | Shoal Focus · Trench Split · Step Refocus · Pebble Scatter · Channel Crosslight · Sandbar Bend · Pool Convergence · Rim Reflection |
| F12 Shadow Telegraph | A moving lamp casts long shadows across procedural structures on mid accents. Artwork joins the occluders and changes the cast shadows across the set. | Raking Wall · Stair Shadow Climb · Parapet Sundial · Opposed Lamps · Slat Crossover · Corner Wrap · Beam Eclipse · Screen Handoff |
| F13 Foil Register | Grazing light sweeps an anisotropic foil surface as highs rise. Artwork embosses the foil and sends its highlights in different directions. | Ribbon Corrugation · Offset Stamp Relay · Diagonal Brush Reversal · Fluted Panel Sweep · Raised Seal Flash · Woven Foil Glide · Tension Wrinkle · Creased Edge Spark |
| F14 Serrated Shutters | A screen of physical louvers opens in groups on hits. Artwork changes which blades latch and which layers of the set become visible. | Concertina Chase · Checkerboard Flip · Diagonal Unzip · Slot Crossover · Corner Aperture · Opposed Banks · Floor to Ceiling Roll · Interlocking Iris |
| F15 Ceramic Resonator | A tiled slab flexes on bass and rings down along its joints. Artwork changes the stiffness of tiles and steers the travelling displacement. | Honeycomb Buckle · Keystone Ringdown · Brick Bond Slip · Mosaic Fan Flex · Travelling Ridge · Scallop Snap · Split Panel Rebound · Corner Load |

### Signals and time

| ID and family | Procedural subject, music and Nested effect | Eight scene briefs |
| --- | --- | --- |
| F16 Kinetic Typesetter | Procedural glyphs occupy a full print bed. Mids change the spacing and hits lock forms in place. Artwork changes which glyphs assemble and where ink is withheld. | Column Set · Headline Slam · Diagonal Proof · Corner Index · Loose Type Cascade · Margin Chase · Reversed Forme · Overprint Collision |
| F17 Cable Exchange | A dense wiring map carries travelling packets on percussion. Artwork acts as a routing mask that opens junctions and changes where packets emerge. | Bus Arbitration · Switchyard Detour · Ring Bypass · Crossbar Handoff · Branch Fanout · Loopback Queue · Merge Priority · Bridge Failover |
| F18 Barograph | A field of mechanical recorder traces scrolls with the music. Artwork shifts individual pen baselines and gates marks, leaving a time-ordered record. | Stacked Seismograms · Radial Drum Record · Opposed Paper Feeds · Spiral Trace Spool · Crossfeed Pens · Lane Skips · Foldout Record · Alternating Drum Banks |
| F19 Raster Excavation | Wide scan fronts uncover procedural layers on hits. Artwork changes how deeply the fronts cut and which layers they reveal. | Rolling Sweep · Interlaced Quarry · Opposed Scan Faces · Zigzag Bench Cut · Diagonal Stripping · Patch Excavation · Corner Traverse · Depth Relay |
| F20 Sonar Curtain | Edge-fired pulses cross an echo-depth display on transients. Artwork changes the timing and positions of return echoes as the sweep passes it. | Lateral Ping Sweep · Twin Baseline · Bottom Sounding · Side Scan Handoff · Fan Beam Search · Staggered Transects · Cross Echo · Range Fold |

### Machines and motion

| ID and family | Procedural subject, music and Nested effect | Eight scene briefs |
| --- | --- | --- |
| F21 Pendulum Fleet | Weighted arms hang at several depths and transfer beat impulses. Artwork shifts their effective pivots or swing limits and changes the collisions or phase. | Crossed Arms · Ceiling Cascade · Sidewall Escapement · Counterphase Banks · Double Link Transfer · Suspended Bridge · Staggered Row Catch · Floor Pivot Rebound |
| F22 Clockwork Conveyor | Slats and cargo move through a frame-wide belt system. Kicks index its gates. Artwork changes cargo shapes and their destination lanes. | Corner Transfer · Return Belt · Sorting Gate · Belt Weave · Ramp Exchange · Gravity Chute · Turntable Merge · Recirculation Loop |
| F23 Gravity Foundry | Moving masses and their wakes span the frame. Low swells shift attraction. Artwork redistributes attraction and visibly deflects the trajectories. | Binary Capture · Tidal Slingshot · Edge Well Transit · Trojans Scatter · Flyby Exchange · Ring Shepherd · Three Body Ejection · Counterorbit Capture |
| F24 Granular Hopper | Procedural grains pour and pile across the frame. Kick impulses agitate the pile. Artwork becomes a sieve or baffle that changes where grains collect. | Funnel Arch Break · Offset Sieve · Split Hopper · Incline Slide · Pocket Fill · Talus Spill · Layer Inversion · Bridge Collapse |
| F25 Pneumatic Quilt | Connected pockets swell and deflate across the frame on sustained bass. Artwork plugs or vents pockets and changes how pressure moves between them. | Diamond Inflation · Channel Vent · Checkerboard Breathing · Corner Squeeze · Spiral Bladder Fill · Baffle Handoff · Foldback Deflation · Crossfeed Surge |

## Fault Ledger Nested treatments

| Scene | What the artwork does |
| --- | --- |
| Ridge Unzip | Its alpha boundary becomes a fracture that the advancing cut catches. |
| Basin Collapse | Its brightness raises or depresses lit terrain. |
| Paired Fault Slip | Its colours flow along mineral veins on two counter-sliding plates. |
| Terrace Ratchet | Multiple cut impressions repeat across the moving terraces. |
| Diagonal Shear | Its channels set directions and phases for the moving strata. |
| Island Breakaway | Sections of artwork leave their beds on separate moving rock chips. |
| Crosscut Relay | Bright areas hold back local shockwaves after each beat. |
| Shelf Rejoin | A stored impression moves along the seam and fades after a swap. |

## Build the library

The complete library is staged in the **Studio 0.1.14 preview**, with 25
families and eight scenes per family. The family picker groups the 200 scenes.
The 24 expanded families share authored procedural profiles and reusable
Nested shader treatments. Each sibling has its own layout, structural
settings and Nested treatment description. Jay is evaluating the preview.

1. **Deliver all 25 anchors together.** Studio 0.1.12 is the first anchor
   preview. Jay requested hands-on testing and asked to skip scene tests and
   captures for this delivery.
2. **Use the anchor review before making siblings.** Compare the anchors with
   one another and with the existing Studio library. Fix families that share
   too much silhouette, composition or motion while the cost is small. Use
   contact sheets and matched motion strips if requested for a later checkpoint.
3. **Deliver a complete family when requested.** For every scene keep its
   family and variant ID (for example `F01-01`), authored title, structural and
   motion differences, music mapping, Nested effect and evidence path. Track
   drafted, rendered and accepted separately. Count a scene once it passes the
   checks below. The target is eight accepted entries per family.

## Acceptance evidence

- Render in an isolated Studio context at the default look. Use the same time,
  dimensions and audio cue for comparisons. Capture quiet and Demo pulse
  without media, then compare Nested against Overlay and zero injection with
  the same artwork, placement, and Still motion. Where feasible, reset history
  before each matched run. Use sequential frames or a short strip for movement.
- Inspect the full output and all four quadrants at rest, at peak response and
  late in the motion cycle. Confirm the artwork changes the stated scene
  behaviour with two visibly different transparent silhouettes and an opaque
  input with luma structure. Check a live swap and clear, and verify reset on
  persistent scenes. On representative anchors, also try video and an owned
  track excerpt with a quiet section and a stronger passage before calling the
  library ready for a live preview.
- Confirm every accepted entry opens from its actual Studio scene folder. Before
  local activation, verify the batch through the normal picker in the exact
  staged bundle and retain representative captures with source and bundle
  identities under `.local/`. Follow the project's serial, focused verification
  and versioned launcher policy. Record which checks ran and which live use
  remains to be judged.

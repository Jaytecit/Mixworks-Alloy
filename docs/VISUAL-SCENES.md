# Visual scenes and automatic shows

Current workflow reference, reconciled 4 November 2026. [Visual direction](VISUAL-DIRECTION.md) describes
the intended experience; [HANDOVER](../HANDOVER.md) records the exact local build.
Earlier visual experiments remain dated evidence in their individual reports.

## Library and placement

Open **Visuals / scenes**. The library contains 204 scenes. Most scenes have six named looks;
33 surface and geometry scenes also have five independent material-method looks, and Jumping Techno Tunnel adds its native look.
Search/select, then use **Add at playhead**, double-click or drag to the timeline.
Reference stills show examples; **Live mix** shows the current arrangement. The
old temporary Retain checkboxes were removed. Counts do not qualify artistic quality.

The core workflow candidate adds **Replace scene...** to a highlighted scene's
right-click menu. The same library becomes a replacement panel, naming its target
and timing. Choose another scene/look and use **Replace this scene**. Future and
currently active scenes keep their placement, duration and join policy. Shared
images remain; scene-specific controls, automation and offset reset to the chosen
look. Programme audio/time are kept. Undo restores an Arrange edit; a live take
keeps its chronological changes. Native/window/moving-output acceptance is still
pending on the current candidate; see [HANDOVER](../HANDOVER.md).

Right-click a scene block to choose **Preset**, **View method** or **Image method**.
Applying a named look replaces that clip's authored controls/automation. Choosing
one method preserves other controls and removes only that method's automation.
Placement, moves/trims, methods and presets support Undo and persist in the session.

## Images and effects

Use the library's image-folder controls for background and transparent sprite
pools, or a project image override. Original files are read without alteration;
keep them at their selected paths. Backgrounds and cutouts have separate roles.
User media belongs outside the application source package.

**Pulse Gallery**, **Orbit Theatre** and **Resonant Iris** carry a complete image
with musical scaling and surrounding solid geometry. Mosaic scenes can repeat
whole images per tile. **Glass image** on Mosaic Kiln/Spinning Stamps instead shows
one full image behind translucent, refracting tiles; Rhombus Foundry and Enamel
Scales use that treatment by default. Refraction follows tile motion and normals.

Particle scenes use music-driven trajectories and periodically resolve into the
complete image. View methods offer original, mirror pair, four-way reflection and
six-/ten-sector kaleidoscopes. Image methods affect local sampling. Shared finishing
adds pronounced continuous and musical recolouring. Exact pause freezes pixels.
These methods are choices; every scene need not fragment or mirror a readable image.

## Sprite treatments

Choose a transparent image in the project image picker to pin it across compatible
Originals and MilkDrop presets. Clear returns to automatic rotation every 64 bars.
Originals use the sprite as geometry or material. MilkDrop warps it as part of the
preset's feedback. The 33 surface scenes have separate Relief
depth, Domain warping and Displacement colour menus, each usable alone or combined.
This includes the eight implicit sculptures, eight solid instruments, eight
landscapes, eight liquid surfaces and Molten Contours. The added controls default
off in existing scene looks. Choose a method preset or right-click a placed scene
and set each method to Off, Gentle or Strong. Relief follows the scene: raised
sculpture/solid surfaces, terrain or cliff height, and embossed liquid normals.
Colour is applied during shading, so colour alone does not alter height or
simulation state. Source sampling remains premultiplied and scene-local.
See [scene-local sprites, generative joins and limits](MILKDROP.md#transparent-sprites-and-generative-handoffs).

Supported scene groups:

| Group | Scenes |
| --- | --- |
| Sculptures | Opal Reactor, Mercury Cathedral, Gelatine Satellites, Aether Nuclei, Chromatic Torso, Liquid Torus, Velvet Geode, Pulse Cocoon |
| Solid instruments | Hexa Resonator, Lattice Pilgrimage, Facet Voyager, Prism Switchboard, Crystal Pendulum, Kinetic Vault, Octa Current, Honeylight Causeway |
| Landscapes | Lucent Ranges, Opaline Dunes, Chromatic Ravine, Glass Delta, Cinder Mesa, Sapphire Archipelago, Polar Canopy, Lucid Escarpment |
| Liquid surfaces | Mercurial Echo, Satin Vortex, Pressure Garden, Chrome Tides, Oilprint, Glass Tremor, Pearl Undertow, Folding Estuary |

Molten Contours also provides all three methods on its relief plate. Bass raises
the relief itself and lifts a dispersing echo made from the same artwork.

## Automatic shows and transitions

Use **Match mix** for an existing music arrangement, or choose automatic visuals in
Auto Arrange. The visual lane covers the music and uses the actual transition
windows without changing music timing. Image selection, controls and visual blocks
are saved. Automatic scheduling favours solid/image compositions: particle scenes
can occupy at most one slot in five when other families are available.
Each new show makes fresh random choices from the least-used eligible scenes,
using the collection before repeating. Energy and style guide the shuffle;
recent scene families and motifs are spaced apart. The selected Originals,
MilkDrop or Both collection still controls which scenes can appear. Generated
choices are saved with the mix and stay fixed during playback.

The staged renderer blends shape and camera parameters within the sculpture and
solid-instrument families. The 52 compatible relief scenes merge their height,
coverage and material coordinates across 13 families. Other pairs use feedback
with a shorter recovery.
The lane still shows the saved **Feedback** join label. See [handoff routes and
limits](MILKDROP.md#transparent-sprites-and-generative-handoffs).


## Output and limits

The scene browser and fullscreen output share GPU textures. Choose the output
monitor and render resolution in the browser; Escape leaves fullscreen output.
60 Hz is a scheduling target, not a sustained-performance guarantee. Playback
clock, rendering, physical scanout and listening are separate acceptance evidence.
Video decoding/timed media lanes and product video export are tracked under W16;
[the four preserved MP4 fixtures](VIDEO-INTEGRATION.md) are available for future work.
Dancer remains deferred under W26. Direct live scene replacement is W06.

Use [verification entry points](../VERIFICATION.md) for current checks. Earlier
external-renderer and prototype probes were [archived](MAINTENANCE-20261103.md).
Do not run old catalogue generators or infer current acceptance from historical
scene counts, numeric changes or a reference screenshot alone.

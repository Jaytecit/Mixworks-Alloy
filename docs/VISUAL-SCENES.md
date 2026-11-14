# Visual scenes and automatic shows

In the four Studio lanes, drag a box from empty space to select clips across
lanes. Ctrl-drag can start the box over a clip when no automation parameter is
selected. Drag a highlighted clip to move the group with the current snap
setting, or press Delete to remove the group. Each completed edit uses one Undo.

In the connected Studio controller, a manual movement holds the value through
clip changes, seeks and reopening the mix. Shared controls follow compatible
scenes, and a scene-specific control follows siblings with the same meaning.
Use **Reset** to return to the timeline. Each continuous control has **Auto**
and an **Automation** panel with minimum, maximum and cycle length. A full
cycle defaults to 64 bars. Choose **Bass** or **Overall energy** and set the
response depth to add bounded audio motion over the base value.

Right-click a clip and use **Draw automation** to choose its parameter. Ctrl-click
to set points and Shift-click to remove interior points. Auto supplies the base
while enabled. Turning it off reveals the keyframes again. The four **Swap**
buttons select a different available background, MilkDrop preset, GLSL scene or
sprite at the playhead. If the lane is empty there, the swap places a new clip.

Current workflow reference for Alloy 0.55.133 and Visual Studio 0.1.33. [Visual direction](VISUAL-DIRECTION.md) describes
the intended experience; [HANDOVER](../HANDOVER.md) records the exact local build.
Earlier visual experiments remain dated evidence in their individual reports.

## Studio library and placement

Open **View → Open Visual Studio**. Use **+ GLSL** in Alloy's visual timeline
to choose from 1,000 bundled Studio GLSL scenes by family or open a local shader.
Use **+ MilkDrop** for Prism Current and locally imported presets. Add a
background or a sprite with its lane's picker.

Right-click a single GLSL or MilkDrop clip and choose **Replace with GLSL
scene…** or **Replace with MilkDrop…**. The replacement keeps the clip ID,
start, length and join. Its scene controls and automation start from the
replacement's defaults. A cross-lane replacement requires enough space in the
destination lane. Undo restores the original clip. On 0.55.133 the active
MilkDrop replacement and overlap refusal were verified in the desktop
workflow. Native checks cover both renderers and placement before a clip
becomes active. Desktop GLSL selection remains open in the [roadmap](../FINISHING-PLAN.md).

## Alloy Originals library

Open **Visuals / scenes** for the separately rendered Originals. Most scenes have six named looks;
33 surface and geometry scenes also have five independent material-method looks, and Jumping Techno Tunnel adds its native look.
Search/select, then use **Add at playhead**, double-click or drag to the timeline.
Reference stills show examples; **Live mix** shows the current arrangement. The
old temporary Retain checkboxes were removed. Counts do not qualify artistic quality.

The Originals library offers **Replace scene...** in a highlighted scene's
right-click menu. The same library becomes a replacement panel, naming its target
and timing. Choose another scene/look and use **Replace this scene**. Future and
currently active scenes keep their placement, duration and join policy. Shared
images remain; scene-specific controls, automation and offset reset to the chosen
look. Programme audio/time are kept. Undo restores an Arrange edit. A live take keeps its chronological changes.

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
windows without changing music timing. The connected Studio chooses **Dissolve**
for a gradual blend, **Bass Handoff** for a linked bass exchange, and **Downbeat
Cut** for abrupt joins. Right-click the highlighted transition strip to change its
preset. Drag the body to move the transition, or an edge to change its duration.
Image selection, controls and visual blocks
are saved. Each lane starts with an element and holds it until a musical cue.
Background follows sustained energy shifts. GLSL follows track handovers,
reviewed section markers and strong section changes. MilkDrop follows sustained
changes in bass or attack density and also takes the GLSL cues in MilkDrop-only
mode. Sprites change on manual hotcues or strong section rises with a measured
bass return. The beat grid places detected changes. Manual hotcues and section
markers keep their placed times. Mouse and MIDI holds take precedence for
compatible controls until Reset, including across generated changes.

In the separate Originals show scheduler, automatic scheduling favours solid/image compositions: particle scenes
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


## Live preview and output

With empty visual lanes, Studio opens in **Live** mode. **Random scene** and
**Random MilkDrop** change the preview while the audio mix plays. With visual
clips, **Timeline** mode follows the saved schedule. Press **F11** in Studio to
enter or leave fullscreen. The connected desktop preview uses a 30 fps schedule
and a shared pixel buffer. On the isolated 0.55.133 fixture it measured 30.02 fps
in steady playback and 29.77 fps across a GLSL dissolve.

In the separate Originals browser, choose the output monitor and render
resolution there, and use Escape to leave fullscreen. Its 60 Hz schedule is a
target. Playback
clock, rendering, physical scanout and listening are separate acceptance evidence.
Video decoding/timed media lanes and product video export are tracked under W16;
[the four preserved MP4 fixtures](VIDEO-INTEGRATION.md) are available for future work.
Dancer remains deferred under W26. Direct live scene replacement is W06.

Use [verification entry points](../VERIFICATION.md) for current checks. Earlier
external-renderer and prototype probes were [archived](MAINTENANCE-20261103.md).
Do not run old catalogue generators or infer current acceptance from historical
scene counts, numeric changes or a reference screenshot alone.

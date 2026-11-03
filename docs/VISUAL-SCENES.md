# Visual scenes and automatic shows

Current workflow, 3 November 2026. [Visual direction](VISUAL-DIRECTION.md) describes
the intended experience; [HANDOVER](../HANDOVER.md) records the exact local build.
Earlier visual experiments remain dated evidence in their individual reports.

## Library and placement

Open **Visuals / scenes**. The library contains 123 active scenes, including the
61 retained choices. Search or select a scene, choose one of its six named looks,
and use **Add at playhead**, double-click or drag to the timeline. Reference stills
show an example; **Live mix** shows the current rendered arrangement. The overview
and temporary Retain checkboxes support review; they do not qualify visual quality.

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

## Automatic shows and transitions

Use **Match mix** for an existing music arrangement, or choose automatic visuals in
Auto Arrange. The visual lane covers the music and uses the actual transition
windows without changing music timing. Image selection, controls and visual blocks
are saved. Automatic scheduling favours solid/image compositions: particle scenes
can occupy at most one slot in five when other families are available.

Long track joins use two live scenes in a direct dissolve. Zoom also blends both
live final pictures. Short changes can use radial/sweep reveals or explicit cuts.
An extended manually requested radial/sweep falls back to dissolve; the lane label
reflects that actual treatment. Automatic joins longer than four seconds or eight
beats store dissolve. Short explicit reveals retain their authored handoff. The
outgoing scene keeps responding to music throughout the overlap.

## Output and limits

The scene browser and fullscreen output share GPU textures. Choose the output
monitor and render resolution in the browser; Escape leaves fullscreen output.
60 Hz is a scheduling target, not a sustained-performance guarantee. Playback
clock, rendering, physical scanout and listening are separate acceptance evidence.
Video decoding/timed media lanes, dancer and product video export remain deferred.

Use [verification entry points](../VERIFICATION.md) for current checks. Earlier
external-renderer and prototype probes were [archived](MAINTENANCE-20261103.md).
Do not run old catalogue generators or infer current acceptance from historical
scene counts, numeric changes or a reference screenshot alone.

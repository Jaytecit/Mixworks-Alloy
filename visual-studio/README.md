# Mixworks Visual Studio

Alloy 0.55.134 ships Studio 0.1.34 and a shared catalogue of 1,000 GLSL scenes.
Open Studio from Alloy to follow the audio master and visual lanes. An empty
visual timeline uses Live mode. An authored show uses Timeline mode, with a
30 fps connected preview and **F11** fullscreen. See the
[visual timeline guide](../docs/VISUAL-SCENES.md) for setup and measured results.

## Earlier preview launchers

**Launch-Scene-Library.cmd** opens the 0.1.15 preview at Lantern Arcade. Pick
a family in the left selector and one of its eight scenes in the right
selector. The seven older scenes are under **Studio originals**. Load one
sprite and keep **Media → Sprite → Render as → Nested** selected to compare
treatments as you switch scenes. Beat relays need **Demo pulse** or another
audio source with detectable beats. Some scenes retain a moving trace after
the sprite is swapped or cleared. Use **Clear Nested history** for a fresh
comparison on those scenes.

## Current launcher and playback

**Launch.cmd** opens standalone Visual Studio from the active Alloy release.
It listens for Alloy's master audio on local loopback by default.
Select **Demo pulse** to audition without Alloy. Use **Studio scenes** to
choose Signal Loom, Vector Tunnel, Glass Garden, Shrouded, Contour Bloom, Waxfall or Tidal Culture. **Open scene…**
accepts a local GLSL fragment, the `main.glsl`
inside a scene folder, or a `.milk` preset. Open trusted source files.

**Random scene** picks a different bundled GLSL scene. **Random MilkDrop**
picks from presets imported or linked in Alloy, with Prism Current available
when the library is empty. Both use Studio's live preview and are disabled
while Studio follows a scheduled visual show in Alloy.

In Alloy's visual timeline, right-click a GLSL or MilkDrop clip to replace its
scene or preset. The clip keeps its start, length and join settings. Choosing
the other scene type moves the clip to that lane if it fits. The new scene
starts with its own controls, and one Undo restores the previous look and animation.

The earlier **Launch-25-Anchors.cmd** opens the 0.1.12 scene preview at Fault Ledger. The
**Studio scenes** picker includes one anchor per family. Set **Media → Sprite →
Render as → Nested** to let artwork drive the scene. Each anchor has a motion
rate, music response and sprite imprint control.

**Launch-Fault-Ledger.cmd** opens Studio 0.1.13 at Ridge Unzip. The first
eight entries in **Studio scenes** make up the Fault Ledger family. Use the
same Nested artwork as you switch to compare cracks, height, pigment, repeated
stamps, motion, fragments, beat delays and a persistent trace. Crosscut Relay
needs a beat to fire, and Shelf Rejoin keeps a fading trace after swapping art.

## Controls

- The controller follows Alloy's graphite and copper theme. It puts wheels
  at the left, eight pots over two pad rows, nine faders with reset buttons,
  and two rows of four pads at the right for control lane and sprite routing.
  In Live mode, the loaded scene and Sprite pads are available. Overlay and
  the loaded scene's supported nesting destination are available.
- **Scene** displays the selected shader's own groups, ranges and defaults.
  Sliders, toggles, choices, colours, triggers and XY pads are available. Save
  and Load create and reopen a `.look.json` for that exact shader and descriptor
  revision. Reload re-reads the source and resets scene controls and feedback.
- **Meta** applies to the final output from either backend. It has a master
  fade, invert, brightness, gamma, contrast, hue, saturation, two mirrors,
  two-colour limiting and an alpha channel option for PNG captures.
- **Media** opens an image or a seekable video. Under **Sprite**, choose
  **Render as → Overlay** to composite it over the finished scene, or **Nested**
  to feed it into the scene. Scale, fit/fill, flips, colour and Sprite amount
  apply in both modes. Overlay blend applies to the finished composite.
  Video transport provides play/pause, stop,
  frame steps, scrub, loop and playback speed. Video plays silently. Use
  **Open audio…** for a rendered mix or soundtrack, then press Play. Studio
  streams mono and stereo audio from disk, including full-length sets.

Lock, reset and randomise the active panel or a group. Undo and redo apply to
the visible panel. **Save frame…** saves the current 800 × 450 preview as PNG.
Meta and Media settings stay in place as you switch scenes during a run.

## Sprite modes and motion

**Tidal Culture** grows a full-frame reaction-diffusion field. Kicks send
pressure diagonally through the field, bass drives the current, and highs
brighten its edges. Open a transparent PNG in **Media** and set **Sprite →
Render as → Nested** to change the chemistry where the sprite lands. Swap art
while playing to redirect growth. Existing cells keep moving and reacting.
**Sprite imprint** sets the strength of the input, and **Restart** clears the
field and starts a fresh culture.

**Waxfall** softens the artwork on each detected beat. Coloured rivulets peel
from its lower edges and keep falling as the body reforms. Select **Media →
Sprite → Render as → Nested**, then open a transparent PNG. **Melt depth**
sets the sag, **Gravity** sets the fall speed and **Drip life** sets how long
shed colour stays visible. Swapping artwork changes the source of fresh drips.
Heated currents run across the frame and brighten on the beat.

**Contour Bloom** sends a wave out from the sprite's alpha outline on each
detected kick. Select **Media → Sprite → Render as → Nested**, then open a
transparent PNG. Use the same Open button to swap artwork while the scene
runs. **Bloom reach** and **Travel time** shape the wave. **Artwork visibility**
can hide the original image while keeping its outline. Studio uses a built-in
outline when no Nested media is active. Switch off **Demo outline** to clear it.
The topographic field carries a second pulse across the frame. Both scenes'
**Backdrop** controls set the strength of the surrounding animation.

Nested feeds MilkDrop's previous-frame buffer before the preset warps it.
Expect different results across presets. A preset with little feedback may
show very little of the sprite. On Studio scenes, the sprite can colour
Shrouded's cell simulation, refract inside Glass Garden, thread through
Signal Loom or texture the walls of Vector Tunnel. Imported GLSL uses its
active `syn_Media` or `iChannel0` sampler when available, or receives the
sprite in `syn_FinalPass` if it reads previous frames. The Media panel shows
when Nested is unavailable. Existing feedback can continue to evolve after
switching to Overlay or clearing the sprite. Reload the scene to clear it.

**Sprite motion** offers Still, Drift, Orbit, Spin, Pendulum, Bounce and Beat
Pulse. Motion transforms the image or video before either render route.
Beat Pulse uses the beat signal and bass level. The motion selection is
independent of the active scene and of video playback speed.

**Media → Nested sprite** has Injection, Beat push, Angle and Position.
Injection trims how much sprite enters the scene. At zero, a MilkDrop trail
can keep moving with the feedback already in memory. Beat push expands the
incoming sprite with the music. Angle and Position move its point of entry.
These controls apply to both MilkDrop and GLSL scenes that support Nested.

**Scene → Feedback** on MilkDrop has Trail length, Zoom, Spin and Warp.
The centre of each control uses the preset's own values. The controls adjust
the running feedback mesh and decay. Presets with custom shaders can respond
differently depending on how they sample and fade the previous frame.

NestDrop v2 uses Overlay and Nested on its sprite buttons and can link either
mode to a preset. Its separate FX 00–99 bank defines sprite animations and
blend modes through `Milk2_img.ini`. Studio's named motions are original
implementations of those kinds of effects. [NestDrop v2 manual](https://nestimmersion.ca/assets/nestdrop_user_manual_V2.pdf#page=43)

## Live master audio from Alloy

1. In Alloy 0.55.75 or later, enable **View → Share master audio with Visual Studio**.
2. In Studio, choose **Alloy live** in the audio source menu below the preview.
3. Play the mix in Alloy. Studio shows **Receiving Alloy master audio** and the
   meters and scenes react to the live master. Continue to control the mix in Alloy.

Alloy 0.55.76 also sends the detected BPM of the active track and the current
mix tempo. The BPM field shows the detected value when available. The footer
shows both values so a stretched track's source tempo stays distinguishable
from its live mix clock. During an overlap, Studio shows the source tempo
from the clip with the greatest fade and lane gain.

The feed is local to this computer. It taps the master before the monitor mute,
and Studio uses it for visuals without opening a second audio output. Turn off
sharing from the same Alloy menu item. Use **Open audio…** for rendered audio
files. Open `.mixworks` arrangements in Alloy and use the live feed.

## Alloy FLkey 49

Open **MIDI**, choose the primary **FLkey MIDI** port and click **Connect**.
Use the Alloy Custom modes already installed on the controller. Studio ships
the verified Alloy mapping JSON and reads the same messages through Windows
MIDI. It opens the port only on request and sends no messages to the controller.
Windows or the device driver may reserve a MIDI port for one client at a time.
Use **MIDI Input On/Off** in the Studio header to release or reopen the
selected port. To control Studio alone, first uncheck the FLkey input in
Alloy's **Tools → Audio / MIDI settings**, then enable it in Studio. For Alloy alone,
turn Studio's MIDI input off and enable the device in Alloy.

| Physical controls | Studio action |
| --- | --- |
| Pots 1–8 | Eight scene control values in descriptor order. Vector components take consecutive slots. |
| Top pads 1–8 | Select available pot banks. |
| Bottom pads 1–8 | First eight scene switches or triggers. |
| Faders 1–5 | Brightness, gamma, contrast, hue, saturation. |
| Faders 6–9 | Sprite amount, media scale, video speed, Master. |
| Fader buttons 1–9 | Reset the corresponding fader's setting. |
| Pitch / modulation wheels | The two scene parameters shown beneath the on-screen wheels. Pitch returns to the range midpoint. |
| Play / Stop / Home | Play, stop and rewind, restart. |
| Undo / Redo | Undo or redo the active control panel. |
| Record / Snap | Save a PNG frame or randomise the Scene panel. |

For MilkDrop, **Bank 1** maps the first five pots to Trail length, Zoom,
Spin, Warp and Beat sensitivity. **Bank 2** maps them to Nested Injection,
Beat push, Angle, Position X and Position Y. The bank buttons also work
on screen. Select **Nested** to see the music drive Beat push in the preview.

Click the assignment beneath either wheel to choose a scene parameter or a
single component of a vector. **Assign both wheels** beneath an XY pad pairs
them with X and Y. Studio starts with the scene's first XY pad when available,
and remembers separate wheel assignments as you switch scenes during a run.
Shrouded starts with **Cam Look X** on Pitch and **Cam Look Y** on Mod. Move
**Tunnel** separately to draw the simulation towards the centre.

Shrouded is a fresh Studio scene inspired by the Synesthesia reference. It
exposes the eight reference controls alongside Cam Look and a five-look
Palette selector. To try **Media Color**, open an image or video in **Media**
and select **Nested**. Sprite brightness feeds one of the cell populations.
**BPM Oscillate** uses Studio's beat wave, with the detected pulse as a
fallback for file playback.

## Authoring

### Standalone scene viewer

Run [Launch-Scene-Viewer.cmd](Launch-Scene-Viewer.cmd) to scan the `scenes`
folder. Choose from the list or use **Back**, **Forward** or the arrow keys.
The ends wrap around. Use **Reload scenes** after adding folders. Choose an
image with **Choose Nested sprite…** to feed it into compatible shaders.

The retained Lit and Lib alternatives are grouped with their **Laser Grid Horizon** layouts in the viewer list. Use **Back** and **Forward** to compare the versions with
the same sprite and desktop audio connection.

Use **Mark** or **Ctrl+M** on a scene to add it to the removal list. The marks
are saved per library and survive a restart. **Remove marked** moves the marked
scenes into a dated recovery folder and shows its location. From the shared
library, the same action updates the catalogue used by the current Alloy
launcher. Close and reopen **+ GLSL** to see the updated list. The active
release keeps the shader files for mixes that already reference them. The
recovery folder includes the previous catalogues.

The viewer uses the Windows default playback device through WASAPI loopback.
Use **Reconnect audio** after changing speakers or headphones. To install the
viewer-only dependency from the repository root, run:

```powershell
.\.local\visual-studio-env\Scripts\python.exe -m pip install --only-binary=:all: --target .local/scene-viewer-deps PyAudioWPatch==0.2.12.9
```

Use the [scene rulebook](../docs/STUDIO-SCENE-RULEBOOK.md) for the additional
Studio library. It records the family roster and the full-frame, Nested media
and visual comparison checks for new scenes.

For the 24 expanded families, edit the anchor shader and the family profiles
in `authoring/families.json`. Nested mechanisms live in
`authoring/nested-treatments.glsl`. Run
`py -3.10 scripts/build-studio-families.py` from the repository root to
regenerate the standalone siblings and the picker catalog. Fault Ledger
and the anchors are individually authored scenes.

Use OpenGL 3.3 desktop fragments. A plain `void main()` shader can declare
standard host uniforms such as `TIME` and `RENDERSIZE` and exposes active
scalar and vector uniforms as editable values. ShaderToy-style
`mainImage(out vec4, in vec2)` supports time, frame, resolution, mouse and
one media channel. The SSF-style `vec4 renderMain()` entry point provides
`_xy`, `_uv`, `_uvc`, `PI` and the uniform declarations automatically.
It accepts a `scene.json` beside `main.glsl` or a matching JSON sidecar beside
a single GLSL file. The bundled scenes are working examples.

Alloy Originals such as Inkstone use a separate pass graph and renderer.
Open those from **Visuals / scenes** in Alloy. Studio expects its own scene
descriptors or standalone GLSL fragments.

The renderer supports named intermediate passes, previous-frame feedback
via `syn_FinalPass`, local images declared with `IMAGES`, and user media via
`syn_Media` and `syn_MediaType`. Passes are double-buffered. Local audio
uniforms cover overall, bass, mid, mid-high and high levels and hits, plus an
on-beat pulse. In Alloy live mode, `syn_BPM` uses the detected track tempo
when available and falls back to the mix tempo. `syn_BPMSin` follows the
mix grid's beat phase. `alloy_DetectedBPM` and `alloy_MixBPM` expose both
values separately. Studio computes the level and hit signals locally. A scene using Synesthesia's
JavaScript, helper library, audio texture uniforms, cubemaps or automatic
transitions needs a port. The `.milk` backend uses projectM 4 and its preset
and texture support. Studio's patched runtime exposes the fixed Feedback
controls described above.

Studio's scene, control and pass model follows the published
[Synesthesia Shader Format](https://app.synesthesia.live/docs/ssf/ssf.html) and
[JSON configuration](https://app.synesthesia.live/docs/ssf/json.html).
Its overall panel layout follows the supplied control references.

## Local setup

The local workspace already has an environment and projectM runtime. To
recreate the Python environment from the repository root in PowerShell:

```powershell
py -3.10 -m venv .local/visual-studio-env
& .local/visual-studio-env/Scripts/python.exe -m pip install -r visual-studio/requirements.txt
```

Python needs Tk, and the graphics driver needs OpenGL 3.3. For MilkDrop place
`projectM-4.dll`, `glew32.dll` and their notices from the local projectM
runtime into `.local/visual-studio-runtime`. `scripts/fetch-projectm.ps1`
builds the runtime with Nested and live Feedback controls. Alternatively, give
`studio.py --projectm <directory>` the path to that runtime. For presets
with external textures, Studio checks the preset directory and nearby
`Textures` folders.
Open the full Visual Studio from Alloy's four visual lanes using **Open Visual Studio**
or **View > Open Visual Studio**. Alloy sends its master audio on local loopback
by default. Empty lanes put Studio in Live mode, with scene, meta and media controls.
Once the mix has visual clips, **Control Lane** selects Background, MilkDrop,
GLSL or Sprite. The controls follow that lane's clips as the playhead moves.
**Sprite Route** sends the sprite to MilkDrop feedback, a compatible GLSL
scene, or the final Overlay. A missing or incompatible destination uses
Overlay for that frame. The audio timeline's **Follow playhead** button also
keeps the Visual Studio timeline marker in view.

In the clip picker, **Follow playhead** exposes the controls for the
current visual. The clip dropdown lets you focus another layer. Clicking a clip
in Alloy also focuses it in Studio. Use **MIDI** to connect your controller.
The mapped pots, pads and wheels follow the focused clip's controls. Moving a
control holds its value across compatible clips, including after reopening.
**Reset** returns that parameter to the current timeline value. Shared GLSL
Motion and grading controls carry across scenes that expose them. A
scene-specific control carries within its family when the control matches.

Continuous controls have an **Auto** toggle. Open **Automation** to set the
minimum, maximum and cycle length in bars. The default is a full 64-bar cycle.
The same panel sets a bass or overall energy response and its maximum depth.
On the Alloy lane, right-click a clip and choose **Draw automation**. Ctrl-click
sets a point and Shift-click removes an interior point. Auto takes precedence
over drawn points while enabled. The audio response is added to the current
value, within the control's bounds. A manual hold takes precedence until Reset.

The four **Swap** buttons pick a different available item for the named lane
at the playhead. A swap keeps the clip's timing and compatible controls. In a
gap, it places the item at the playhead. Use Alloy's **+ MilkDrop** to link more
presets. Changes use Alloy's Undo and save with the mix. Closing Studio leaves
Alloy running.
**Fullscreen output** moves the picture
to the other monitor at its display resolution, or uses the current monitor if
there is only one. Studio stays available for scene and timeline controls. Click
**Return preview**, or press F11 or Escape, to bring the picture back. The
**FPS counter** checkbox shows the measured frame rate on the fullscreen output.

**Original / MilkDrop** runs from the original GLSL scene at 0 to MilkDrop at 1.
Intermediate settings retain the nested treatment as well as blending the images.
In standalone mode it uses bundled Prism Current; connected mode uses the preset
under the playhead in the MilkDrop lane. With no active preset, the original remains.

The auto arranger deals across shuffled scene families, without repeating a scene
until the available library is exhausted. A fresh arrangement gets a new order.
Backgrounds follow sustained shifts in energy, MilkDrop follows sustained changes
in groove, GLSL follows section cues and track handovers, and sprites follow
marked hits or strong bass returns. A lane can hold its starting element to the
end of the mix. Manual control holds remain active on compatible clips until Reset.
Match Mix chooses **Dissolve**, **Bass Handoff** or **Downbeat Cut** from the
audio join. Right-click the coloured strip at a visual join to change the
preset. Drag its edges to change the duration or drag the body to move it.
These gestures leave the music timing in place.

Sprite Bounce now has one bounce per audible beat. Drift, Orbit, Spin and Pendulum
use musical periods too. Connected playback uses the absolute native beat clock,
including seeks and tempo changes; its effective tempo follows the heard track,
including any deliberate mix tempo adjustment. Independent Studio uses its selected
audio clock. Video playback speed remains a separate control.

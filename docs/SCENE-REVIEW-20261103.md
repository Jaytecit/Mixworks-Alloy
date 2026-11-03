# Installed scene review — 3 November 2026

The scenes library now defaults to an image overview in the former live-preview
area. Click an image or its title to inspect it larger. Retain checkboxes in both
views save the development shortlist immediately, independently of arrangement
Save. Search and Retained only narrow the catalogue; unchecked scenes stay
installed. Live mix remains available in the view selector.

## Current review session

- Isolated root: `.local/rvw1`; candidate: `bundle/Mixworks.exe` within it.
- **Shortlist: `.local/rvw1/data/scene-retention-review.json`.** Read this before
  proceeding with Jay's scene choices. Do not initialise another root and lose
  sight of his selections. Keys and titles are both saved with schema 1.
- Empty session saved as `review-checkpoint`. No real mix or production settings
  were modified. The review window is deliberately left open for Jay.
- To reopen a stopped review, use the development bridge `start` on this root
  with its frozen `bundle/Mixworks.exe`, then open Visuals / scenes. Confirm the
  old process has exited first. Selection persistence does not require reopening
  the empty arrangement.

## Images and scope

127 JPEG reference images at 640×360 cover the complete installed catalogue
(100 base entries plus 27 existing variants). The runtime was frozen from
`out/releases/0.55.67-4258046694c2/original-visualiser-runtime`. All rendered serially
from the same 2.5-second excerpt of authorised music, with separate background
and cutout artwork. Captures use actual scene output, not generated concept art.
The three newer isolated direction proofs are not part of this installed set.

`scripts/render_scene_previews.py` creates a fresh output directory and records
runtime/scene/music hashes. Its output manifest and preview images are bundled
in `visuals/original/previews`. Seven inspected contact sheets, the exact runtime
snapshot and before/after source checkpoints are retained in `.local/scene-review`.
Stills demonstrate appearance at one instant; they do not establish movement,
musical sensitivity or the quality of other artwork/effect combinations.

## Verification

- Successful Release build: one worker, below-normal priority; build log retained.
- 127 unique, nonempty preview files match catalogue keys; all contact sheets
  visually inspected. Native gallery displayed the matching images and titles.
- Native Retain toggle wrote the expected key/title. Retained only showed the
  single selected scene; clicking its image opened the enlarged view.
- Saved and closed the isolated app. The first immediate relaunch timed out
  while the previous app was shutting down; its status request remains retained.
  Confirmed no owned process remained before launching again. Successful restart
  showed the same checked scene. Removed that verification choice through UI;
  empty shortlist confirmed before user handoff.
- Live mix mode started the renderer and showed the normal empty-timeline prompt.
  Gallery/image views avoid continuous rendering. This task did not retest music,
  audio output, fullscreen output, transitions or the full regression suite.

This temporary selection mechanism does not delete or hide unretained scenes
outside the optional filter. Atomic replacement protects the previous shortlist
on write failure; unreadable review documents disable editing and report it.

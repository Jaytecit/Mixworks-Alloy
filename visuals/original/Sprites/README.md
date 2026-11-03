# Your still-image library

Add PNG, JPEG, WebP, BMP or TIFF still images here. On visual-player startup,
Alloy scans this folder (up to 512 files, no subfolders). Images are read only.
Actual transparency separates cutout objects from opaque backgrounds. PNG files
with an opaque alpha channel remain backgrounds. The separate Backgrounds folder
is preferred for backgrounds; identical content across both folders appears only
once. Cutouts preserve alpha and proportions in sprite grids, particle fields,
foreground compositions or decals; backgrounds feed textures and environments.
The show advances every 32 project beats, blending over the last four beats;
scene automation varies rotation, zoom and how the image enters the material.
A specifically selected project image takes precedence over this pool.

Images must be at most 16 megapixels; GPU copies are reduced to 2048 pixels.
Unreadable images are reported and skipped. Reopen the visual browser to refresh
the pool after adding files. `media-library.json` can name up to eight folders.
User media is not covered by the application licence or committed to Git.

# Beats in 4D

Shader author: Mårten Rånge. The source shader declares CC0 1.0 Universal.
See `licenses/CC0-1.0.txt` in the Alloy package.

Alloy adapts the CC0-labelled `main.glsl` from the installed
`release/beats_in_4d.synScene` directory. The source hash and before-source
checkpoint are retained privately in `.local/visual-release-06`.

The third-party rational `tanh_approx` function marked “License: Unknown” is
excluded in full; GLSL 330's built-in `tanh` is used instead. No source JavaScript,
JSON descriptor, thumbnail, texture, extracted SSF helper or native media shader
is included. The scene has no required image asset.

Alloy supplies its own normalized control descriptor, PCM envelopes and bass
clock, image fitting and alpha-aware cutout integration. The single-pass shader
retains the source's four-dimensional folding and accumulated light structure.
It is an adaptation, not an unchanged Synesthesia scene or an assertion of
identical native audio behaviour. Its attribution and CC0 status are independent
of Alloy's licence for original material; no author endorsement is implied.

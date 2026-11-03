# Transition filter sweeps

Current guide for 0.55.64. High to low/Low to high presets and depth controls generate ordinary editable Filter nodes inside the selected frame. Whole-transition presets replace their affected frame shapes; independent Filter application changes that parameter. No hidden generator is layered under a newly applied sweep. Exterior controls, source clocks, neighbours and locks remain protected. See [preset nodes](PRESET-NODES.md).

The existing two-pole filter uses a logarithmic20Hz to min(20kHz,45 percent of sample rate) domain and a continuous dry blend near neutral. Depth scales travel; cutoff is not brick-wall removal. Gain/EQ, source and lane/manual Filter ownership remain distinct and explicitly named. Original-track controls and VST ownership are shared across split pieces; source geometry/fades remain local.

Choose the Filter footer/Editing parameter to add/move/bend ordinary nodes or enter Off/LP/HP frequency values. Empty curves add only the requested node. Native exponential spans keep sweeps sparse while retaining necessary wet entrances/exits and real boundaries. Existing authored/dense curves are not automatically simplified. Edits use one Undo; playing/export/rehearsal consume the same canonical controls under their documented owners.

Arrange A/B/C is master-output matched-RMS rehearsal; Record/Perform has independent private transition preparation and explicit Apply/Launch. [Mode contract](ARRANGE-RECORD-PERFORM-PLAN.md) defines routing/capture. Noise, resonance/effect-region generators and complete compensation/isolation remain future work.

[Historical filter implementation and reference observations (archived)](MAINTENANCE-20261103.md) preserve0.40 DSP measurements and competitor provenance. Those measurements and old descriptor/session formats do not qualify the latest complete musical/hardware workflow. Current analysis/session identities and remaining acceptance are in the [request register](REQUEST-STATUS-20261024.md).

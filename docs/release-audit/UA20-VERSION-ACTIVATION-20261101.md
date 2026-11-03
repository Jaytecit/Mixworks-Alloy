# Version0.55.66 and A/B/C activation — 1 November 2026

Jay requested: "Activate and update, including the v number in the app UI".
The version changed0.55.65 to0.55.66 in CMake. All Source inputs and remaining
CMake bytes exactly match ua19's archived109-check candidate. Both Workstation
branding and executable metadata use the generated project version.

One serial single-worker below-normal build succeeded. Fresh bounded
`--waveform-alignment-only` through `verification.run_checks` passes37/0 on
SHA256 `708a26477c5d31e65f6161cf9f7d17716c241a0fc85e3fa2443e53de262cd409`.
No stress, infrastructure or retention errors. Standard retention removed the
owned3,840,104-byte waveform fixture. The actual1250x800 Workstation capture was
inspected and visibly reads V.0.55.66; Windows ProductVersion and FileVersion
also read0.55.66. Six additional waveform captures are retained with passing
native pixel checks. ua19's109/0 is explicitly prior-candidate evidence, not a
fresh109-check result on this version-only executable. No broader rerun.

Activated local bundle:
`out/releases/0.55.66-708a26477c5d/Mixworks.exe`.
All35 payload files were hash-verified, dependencies match pinned assets and the
visualiser runtime matches the previously staged payload. Canonical model/runtime
files use hardlinks. Receipt and scoped summary explicitly state that complete
regressions were not run. This is the established user-authorised local-preview
activation path, not public release clearance or a weakened full release gate.

The desktop shortcut still targets repository `Mixworks.cmd`, which calls
`out/Launch.cmd`; that launcher now selects the exact versioned executable.
Previous launcher bytes and bundle are preserved. Source archives, source
manifests, exact executable, dependency hashes, native report, UI capture,
Windows metadata and activation receipt survive independently of Git under
`.local/ua20/`. The staged bundle carries its scoped verification and receipt.

Before/after production inventories were empty. No app was launched or restarted;
no rescan, analysis reset, source or session cleanup, or user-data modification.
No full regression, physical gesture/device, novice usability or real-track
listening acceptance. The A/B/C behaviour and its remaining limits are recorded
in [ua19](UA19-SIMPLE-ABC-GRID-20261101.md).

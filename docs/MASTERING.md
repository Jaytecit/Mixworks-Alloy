# Master output and spectrum

The timeline has its own stereo **PEAK** panel on the right, between the waveforms and inspector. It follows timeline height and window resizing. Independent L/R rails span -48 to 0 dBFS, with numeric readings and lines holding peaks for 1.2 seconds. A red **CLIP / reset** button reports full-scale peaks; click it to reset held peaks and clipping history. The master gain knob sits below the rails and returns to the inspector row when the timeline is very short.

These live peaks accumulate actual device-callback samples after master processing and before monitor mute. Brief transients between display frames are retained. Offline rendering, export and preparation of rehearsal alternatives cannot drive this display. Steady copper heat follows control values and meter levels, becoming paler toward maximum; disabled controls retain their disabled appearance.

Enable **View → Molten sparks / forge playhead** for occasional sparks near meter peaks and a molten seam where the playing cursor crosses overlapping audible tracks. **View → Forge motion** offers **Follow Windows** (default), **Animated**, and **Steady**. Animated explicitly permits the effect without changing Windows settings. The effect is off by default; both choices persist as workspace preferences outside sessions and Undo. Waveform data, track timing and audio processing stay unchanged. Source-grid and rehearsal preview do not trigger the overlap effect.

Click the timeline hexagon, or choose View → Master output / Spectrum. The lane-count selector stays beside the hexagon. Closing this window leaves the mix open. Escape closes it; ordinary mix shortcuts remain available outside text editing.

The panel controls the summed stereo mix: low shelf at 120 Hz, mid band at 1 kHz, high shelf at 8 kHz, stereo width, the existing master gain and optional soft saturation. Playback, rehearsal rendering and WAV export use the same master stage. Defaults bypass tone, width and saturation changes exactly. Reset tone / width restores those defaults while keeping master gain and its automation.

Width at 100% preserves the stereo image; 0% folds it to mono and values above 100% expand the side component. Soft saturation deliberately changes tone and bounds output. Its drive increases coloration. Master gain follows the existing Read/Touch/Write automation workflow. EQ, width and saturation are saved manual settings with Undo; they do not currently have automation lanes.

IN measures the summed bus before master processing. OUT measures the processed signal before monitor mute. CLIP latches when output exceeds full scale; Reset clip clears the observation. Source-grid or pre-rendered rehearsal monitoring has no separate bus-input measurement, so IN is unavailable there. Preparing A/B/C alternatives restores the live meters and clipping history after its temporary renders.

Mute monitor uses the same persistent workspace preference as the main master checkbox. It silences device output while playback, meters and the spectrum continue. It does not change saved gain, automation or exported audio.

The spectrum shows post-master stereo mean spectral power in dBFS per bin. Independent left/right transforms preserve anti-phase material. A 4096-sample Hann window sets its frequency and time resolution. This is signal analysis, not perceived-loudness measurement or matching. Audio callbacks only copy into a bounded preallocated queue; the UI computes the FFT. Closing/hiding the panel or exporting suspends capture and clears old history. A rate change or queue overflow discards discontinuous history rather than presenting it as one coherent window.

Master settings entered session version 18; versions 1–17 open with bypass defaults. The current session format is 24; keep normal session backups when changing app versions. Recovery captures master settings with the rest of the session. Export includes the configured bounded effect-tail window; see [export controls and limits](EXPORTING.md).

Exact candidate, regression and deployment status live in HANDOVER.md. Native controls, session/WAV round trips and numerical DSP are tested separately from musical listening and physical device checks.

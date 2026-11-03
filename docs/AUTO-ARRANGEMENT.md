# Auto Arrange — energy, genre and blend choices

Auto Arrange finds a feasible progression using a bounded heuristic, not an exhaustive search or a guarantee of the globally best order. The [research targets](research/DJ-ARRANGEMENT-TARGETS-20261023.md) distinguish published guidance, implementation decisions and future techniques.

With an existing mix, the **Auto Arrange** dialog opens with **Extend current arrangement** selected. Enter additional songs or minutes and choose **Extend mix**. The existing arrangement stays in place, already-used sources are excluded, and one Undo restores the previous mix. Use **Create new arrangement** in the Action selector to start over. The first-song selector applies only to a new arrangement.

**Visuals → Create scenes with transition blends** generates one editable scene
per song, fading across each final prepared music transition. It replaces the
whole visual lane, including when extending a mix. **Keep visual lane** leaves
visual blocks intact and is the default when any already exist. Scene choices
use independent randomness and do not change music selection. For an existing
arrangement, **Match mix** in the visual library creates the same kind of lane
without rearranging music. Later music edits require matching again. See
[Visual scenes](VISUAL-SCENES.md) for placement, playback and current scope.

Crowded or nested music handovers no longer stop visual generation. The visual
planner follows transition order rather than raw clip starts, shortens competing
fades and uses cuts where needed. Music placement, timing and audio treatment
remain unchanged. Exceptionally short coincident placements may share scenes.

## Energy direction

| Direction | Preference |
| --- | --- |
| Unrestricted | Nearby BPM and harmonic compatibility, without an energy trajectory. |
| Keep pace | Stay near the starting musical activity and tempo; discourage substantial activity drops. |
| Build energy | Gradually approach higher available activity and tempo, allowing small rises, plateaus and necessary reversals. |
| Wind down | Gradually approach lower available activity and tempo, allowing plateaus and necessary reversals. |

An explicit first song stays fixed. Otherwise, Unrestricted chooses randomly; the other directions choose a start with room for the progression in the largest BPM/key-connected group. Count mode follows position within the feasible library count; duration mode follows elapsed mix time. One-step onward preference discourages immediate dead ends.

The current key compatibility,8% adjacent BPM bound, supported beat/downbeat evidence, source availability, valid placement, no repeats, whole-track targeting and three-hour maximum remain. Energy/genre preferences never veto an otherwise feasible track. A short result reports a compatibility, placement, duration or library limit.

## Musical activity analysis

Supplementary **musical-energy revision1** measures rhythm, rhythmic bass and transient dynamics. This is an explainable activity/drive estimate, not a trained perceptual-energy, genre, vocal or danceability classifier.

- 20ms frames form eight-second source windows. Supported bounds:0.25s–180min, one/two channels,2–384kHz.
- Positive multiband envelope changes and an80ms refractory interval measure attacks. This is not a claimed spectral-flux implementation.
- Bass evidence uses a35Hz high-pass/180Hz low-pass plus attack activity; high-band evidence starts at1kHz, adjusted below Nyquist at low sample rates. Channel powers are averaged, preserving opposite-polarity stereo evidence.
- Local crest/punch and active-frame dynamic range modulate rhythmic drive. Loud mastering, stationary bass, DC or wide dynamics alone cannot establish high activity.
- The initial score weights rhythm/bass/dynamics60%/25%/15%; density saturates at6 attacks/s and bass attacks at3/s. These engineering weights need listening calibration.
- Within-recording normalization makes practical constant gain changes largely irrelevant. A low absolute silence floor remains. Silent/invalid/cancelled evidence supplies no invented activity; measured silence is complete so it does not trigger endless re-analysis.

The arranger uses central80% mean activity for its overall target and two halves of the actual source-mapped handover as a small additional cost. Recording RMS is no longer its intensity score or unknown-profile fallback. BPM contributes separately. Completion reports unavailable evidence and departures. Build/Wind down plateau reporting uses0.025 score tolerance; Keep pace reports changes over0.1. These report outcomes, never admit/reject tracks.

New imports obtain profiles while PCM is available. Existing tracks are upgraded on the Auto Arrange worker when an energy direction or **DJ set (varied)** needs them. Progress says **Analysing rhythm, bass and dynamics**. Cancellation preserves the mix; completed compatible cache results can survive a later cancellation, but partial profiles are never published. Ordinary warm catalogue startup does not decode every old source merely to add features.

Profiles are bound to the enclosing record's source identity and independently versioned. Missing/malformed/future supplementary data does not invalidate valid grids. An opaque newer supplement survives a same-source manual correction. Current detector analysis revision11 and session format41 are unchanged by supplementary activity analysis; no analysis/correction/arrangement/cache reset is required. The activity feature originally landed with session34/portable preset3; those are historical identities.

## Preferred genre

Choose **Any genre**, an existing normalized library label, or type a genre—even one absent from the library. A small bounded preference favours matching tags after feasibility and progression. Other genres and unlabelled tracks remain usable and are counted. The first song stays fixed.

Normalization handles case/punctuation and explicit Hip-Hop/hiphop, D&B/dnb/drum and bass and R&B aliases. It is not broad genre-family inference or substring matching: trip-hop does not become hip-hop. Original metadata stays intact; no genre is guessed. A tag does not establish a safe vocal/drop/phrase region.

## Blend styles

| Style | Actual treatment |
| --- | --- |
| Balanced | Section-aware bass handover; gentle filters where recording-level changes justify them. Plain fade with unknown level evidence. |
| Smooth | Linear full-band volume crossfade. |
| Bold | Bass handover, stronger filters at smaller recording-level changes, plus mid/high fades. Plain fade with unknown evidence. |
| Use my settings | Selected insertion preset, overlap and section setting; alignment-only works without overlap measurements. |
| Constant power | Equal-power envelope, no automatic bass/filter. It does not normalise levels or guarantee constant perceived loudness/peaks. |
| Hard cuts only | Midpoint cut with a short click-prevention ramp, no automatic bass/filter. |
| DJ set (varied) | Supported phrase fades, rhythmic bass handovers and restrained filters, using actual local musical activity and recent choices. |

DJ set falls back to a plain fade with missing activity. Rhythmic bass on both sources favours a bass handover. Dense changing activity can justify a filter at depth0.3, followed by two joins without another automatic filter handover. A substantial incoming lift may prefer eight bars, but actual placement/section feasibility wins; only an actually short window is reported as Quick phrase blend. Safe treatments may repeat. Completion reports counts; verification evidence retains reasons.

All generated treatments are ordinary editable automation nodes. No new delay/loop/stem DSP or vocal/drop classifier is implied. RMS change flags remain activity suggestions; a bar grid alone does not prove phrasing. Genre-specific technique palettes, Wave/Journey curves, advanced routing and the remaining [technique catalogue](research/DJ-ARRANGEMENT-TARGETS-20261023.md#technique-catalogue) are targets, not shipped capabilities.

Controlled audio tests cover gain invariance, equal-RMS rhythm differences, bass/drone distinction, dynamics, silence/DC/nonfinite data, stereo/rates, local activity, ranking and a rendered prepared join. Cache tests cover warm access, upgrades, corrections and future versions. Representative real sources can be measured read-only with isolated caches. These do not establish listening acceptance, every-genre semantic accuracy, physical MIDI/ASIO or audience response. Human-ranked references are the next calibration layer.


## 0.55.57 continuation and node repairs

Action offers **Create new arrangement** or **Extend current arrangement**. Extension counts additional songs or additional minutes, starts from the existing final song, and excludes songs already used. Key and ±8% BPM matching still apply. Existing source clocks, earlier joins, mixer/master automation and document identity stay intact; only the last song receives the new outgoing handover. A shorter new overlap is used when needed to keep its previous incoming transition intact. A locked, unavailable or changing arrangement refuses safely. There is one Undo. The complete mix retains the180-minute limit.

Automatic changed-BPM targets retain their64-beat minimum ramp spacing. Tracks may still be placed when a new target cannot fit; they follow the established map. The reported premature “need more room” failure was also hiding a source-time rounding problem while creating a fade. Gain sampling now uses the captured source clock at the clip boundary, and generated guard timestamps remain distinct after saving. A failed candidate now reports its actual preparation error.

Newly generated automatic curves omit neutral controls, inaudible exterior guards and redundant flat/straight padding. Straight fades keep two endpoints; genuine curved envelopes retain the points needed for their measured accuracy. Existing manually edited clips are not silently simplified when extending.

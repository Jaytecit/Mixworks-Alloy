# Auto Arrange — energy, genre and blend choices

Auto Arrange finds a feasible progression using a bounded heuristic, not an exhaustive search or a guarantee of the globally best order. The [research targets](research/DJ-ARRANGEMENT-TARGETS-20261023.md) distinguish published guidance, implementation decisions and future techniques.

With an existing mix, the **Auto Arrange** dialog opens with **Extend current arrangement** selected. Enter additional songs or minutes and choose **Extend mix**. The existing arrangement stays in place, already-used sources are excluded, and one Undo restores the previous mix. Use **Create new arrangement** in the Action selector to start over. The first-song selector applies only to a new arrangement.

**Visuals → Create automatic visual show** replaces the visual lanes with an
editable schedule. GLSL scenes use the full Backgrounds image pool as shader
inputs. MilkDrop presets continuously use the Sprites image pool inside their
feedback. Choose **GLSL + MilkDrop** for all four lanes. Consecutive uses of
the same background form a single clip. **Keep visual lane** retains the
existing visual schedule and is selected by default when visual clips exist.
Detected musical cues guide each lane. A visual can hold for the whole mix when
its lane has no cue. [Visual scenes](VISUAL-SCENES.md)

## Energy direction

| Direction | Preference |
| --- | --- |
| Unrestricted | Nearby BPM, harmonic compatibility and local handover quality, without an energy trajectory. |
| Keep pace | Stay near the starting musical activity and tempo; discourage substantial activity drops. |
| Build energy | Gradually approach higher available activity and tempo, allowing small rises, plateaus and necessary reversals. |
| Wind down | Gradually approach lower available activity and tempo, allowing plateaus and necessary reversals. |

An explicit first song stays fixed. Otherwise, Unrestricted chooses randomly; the other directions choose a start with room for the progression in the largest BPM/key-connected group. Count mode follows position within the feasible library count; duration mode follows elapsed mix time. One-step onward preference discourages immediate dead ends.

The current key compatibility,8% adjacent BPM bound, supported beat/downbeat evidence, source availability, valid placement, no repeats, whole-song count targeting and three-hour maximum remain. Energy/genre preferences never veto an otherwise feasible track. A short result reports a compatibility, placement, duration or library limit.

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

Profiles are bound to the enclosing record's source identity and independently
versioned. Valid grids remain usable with a missing, malformed or newer activity
profile. An opaque newer supplement survives a same-source manual correction.
Alloy 0.55.134 uses detector analysis revision 11 and session format 54.
Supplementary activity analysis preserves existing analysis and corrections.

## Preferred genre

Choose **Any genre**, an existing normalized library label, or type a genre—even one absent from the library. A small bounded preference favours matching tags after feasibility and progression. Other genres and unlabelled tracks remain usable and are counted. The first song stays fixed.

Normalization handles case/punctuation and explicit Hip-Hop/hiphop, D&B/dnb/drum and bass and R&B aliases. It is not broad genre-family inference or substring matching: trip-hop does not become hip-hop. Original metadata stays intact; no genre is guessed. A tag does not establish a safe vocal/drop/phrase region.

## Blend styles

| Style | Actual treatment |
| --- | --- |
| Balanced (default) | Uses section and level evidence to choose the overlap. Selects equal-power volume, coordinated volume/EQ for competing bass, or a restrained filter when the sources change in opposite directions. |
| Smooth | Linear full-band volume crossfade. |
| Bold | Uses the shared overlap search and bass check, with a stronger filter fade when the overlap has a measured level contrast. |
| Use my settings | Selected insertion preset, overlap and section setting; alignment-only works without overlap measurements. |
| Constant power | Equal-power volume curves across the full band. |
| Hard cuts only | Fast volume swap at a shared section change or near the middle of the frame on the grid. Uses a short click-prevention ramp. |
| DJ set (varied) | Uses the shared overlap search and bass check. Local musical activity and the previous two joins govern whether it adds a restrained filter fade. |

**Blend timeline tracks** blends the songs already on the timeline in their
current order. It shares the Balanced pair planner and measured bass pass with
library **Auto Arrange**, which also chooses the song order. Both commit one
Undo step. The timeline command removes owned automatic treatments, retains
source controls, and refuses a blend that would replace authored volume
inside its new frame. Authored EQ remains intact and prevents an automatic EQ
handover on that pair.

Balanced, Bold and DJ set use reviewed section markers and sustained changes in
retained waveform energy across up to 64 bars of the outgoing tail and incoming
head. Compatible tracks can overlap for up to 64 bars. The two section changes,
available source audio and earlier handovers constrain the length. Four and
eight bars remain fallbacks. The search ranks level continuity and section
evidence, then tests at most six candidates on the actual tempo map. A middle
song retains at least four beats between its two handovers.

The planner may trim up to eight source bars or 20% of a segment, whichever is
smaller, on segments at least sixteen bars long. An extension preserves the
existing tail. Re-blending a generated timeline keeps its current source spans.
The energy suggestions identify changes in level and are useful for auditioning
candidate handovers. They do not label vocals, melody or phrase structure.
Automatically planned joins use their IN/OUT boundaries as audible limits, so
file lead-in before a nonzero cue cannot play briefly at full level before the
fade. Departing audio stops at the planned OUT.

The audible bass pass measures short windows from the chosen sources. It ranks
interior bars by local level agreement and nearby section evidence.
When both sources have sufficient bass, a four-beat low-end exchange is
coordinated with the faders and subtractive mid/treble curves. Low-shelf cuts
are bounded to 18 dB: Alloy's EQ uses overlapping shelves and a peaking band,
not independent crossover bands. This avoids multiplying an independent bass
swap by two attenuated faders. Incoming bass rises once from −18 dB to neutral;
outgoing bass falls once to −18 dB and stays there. Each generated bass exchange
uses two editable analytical endpoints, with no dip-and-return or rows of flat
points. Each handover's volume shape uses three points and its upper-band shapes
use five; exterior controls and other joins can add points. Missing evidence keeps the ordinary volume
blend. Hard cuts remain an explicit choice.

The [two-track mixing audit](research/AUTO-ARRANGER-MIXING-AUDIT-20261112.md)
records the behaviour before this change and the listening criteria.

Auto Arrange applies attenuation-only segment gain using the middle 80% of each
song and its measured sample peaks. It aims for −18 dBFS RMS and caps measured
source peaks at −6 dBFS where the measurements are available. Extensions can
lower the RMS target to −24 dBFS to match a quieter tail and apply trims only to appended songs. These
targets leave headroom for the handover. Final levels also depend on source
dynamics, stretch, mixer gain and effects. Segment gain can be adjusted in the
clip inspector.

New joins are movement-locked. Consecutive locked pairs move as a group, and
**Unlock pair** allows an individual timing move. The transition curves remain
editable. Extending a mix preserves the earlier pairs' lock choices.

All generated treatments are ordinary editable automation nodes. No new delay/loop/stem DSP or vocal/drop classifier is implied. RMS change flags remain activity suggestions; a bar grid alone does not prove phrasing. Genre-specific technique palettes, Wave/Journey curves, advanced routing and the remaining [technique catalogue](research/DJ-ARRANGEMENT-TARGETS-20261023.md#technique-catalogue) are targets, not shipped capabilities.

Short blends and bass handovers own their generated points. Applying a named
preset to the pair replaces those points and preserves manual automation.

Controlled audio tests cover gain invariance, equal-RMS rhythm differences, bass/drone distinction, dynamics, silence/DC/nonfinite data, stereo/rates, local activity, ranking and a rendered prepared join. Cache tests cover warm access, upgrades, corrections and future versions. Representative real sources can be measured read-only with isolated caches. These do not establish listening acceptance, every-genre semantic accuracy, physical MIDI/ASIO or audience response. Human-ranked references are the next calibration layer.


## 0.55.57 continuation and node repairs

Action offers **Create new arrangement** or **Extend current arrangement**. Extension counts additional songs or additional minutes, starts from the existing final song, and excludes songs already used. Key and ±8% BPM matching still apply. Existing source clocks, earlier joins, mixer/master automation and document identity stay intact; only the last song receives the new outgoing handover. A shorter new overlap is used when needed to keep its previous incoming transition intact. A locked, unavailable or changing arrangement refuses safely. There is one Undo. The complete mix retains the180-minute limit.

Automatic changed-BPM targets retain their64-beat minimum ramp spacing. Tracks may still be placed when a new target cannot fit; they follow the established map. The reported premature “need more room” failure was also hiding a source-time rounding problem while creating a fade. Gain sampling now uses the captured source clock at the clip boundary, and generated guard timestamps remain distinct after saving. A failed candidate now reports its actual preparation error.

Newly generated automatic curves omit neutral controls, inaudible exterior guards and redundant flat/straight padding. Straight fades keep two endpoints; genuine curved envelopes retain the points needed for their measured accuracy. Existing manually edited clips are not silently simplified when extending.

> Historical evidence / design record. Its task lists, candidate identities and pending labels describe the dated checkpoint. Outstanding work is consolidated in [the roadmap](../../FINISHING-PLAN.md); this record does not schedule work or authorise old reset/scan/activation instructions.

# DJ arrangement research and targets — 23 October 2026

Requested scope: musical energy from rhythm, bass and dynamics; more transition techniques and suitability/avoidance rules; variety over a set; an optional genre preference that yields to better musical choices or missing tags. This document separates published guidance, product decisions and unimplemented targets. Sources accessed 23 October 2026; no source code, models, audio or training material is imported from these sites.

## Evidence and its limits

| Primary source | Relevant guidance | Implication for Mixworks |
| --- | --- | --- |
| [Native Instruments: five transition techniques](https://www.native-instruments.com/blogs/blog/dj-transitions) | Phrase timing, key and tempo support a blend. Fades, cuts, phrase blends, bass swaps and filter handovers serve different material. | Choose timing and treatment together. Clear competing bass parts. A cut exposes timing; effects cannot establish a missing phrase. |
| [Pioneer DJ: mixing across genres](https://blog.pioneerdj.com/djtips/we-uncover-the-mixing-techniques-behind-every-major-genre/) | House often follows groove and phrase changes; techno favours continuity; tech house favours distinct payoffs; song-heavy material raises overlap risks. These are conventions with exceptions. | Genre should suggest a palette and track preference, never override actual evidence or become an eligibility filter. Do not assume a tag establishes vocal content or a drop. |
| [Roland: crafting a set](https://articles.roland.com/crafting-a-great-dj-set/) | Energy differs from BPM/key. Ramps, waves and journeys provide different shapes. Bridge tracks and small prepared combinations help flow. Constant maximum intensity loses impact. | Distinguish track intensity, set trajectory and transition drama. Include restrained joins, recovery space and onward-route planning. Offline arrangement cannot read a crowd. |
| [librosa: onset strength](https://librosa.org/doc/0.11.0/generated/librosa.onset.onset_strength.html) | Positive spectral changes provide onset evidence. | Detect activity and attacks rather than sorting by recording RMS. A first local implementation may use multiband envelope changes; it must not claim to be the cited spectral-flux algorithm. |
| [Essentia: beat-band measurements](https://essentia.upf.edu/reference/std_BeatsLoudness.html) | Relative band energy can describe spectral content around beats. | Measure bass proportion and bass attacks independently of overall gain. Bass quantity alone must not give a sustained drone a high drive score. |
| [Essentia: dynamic complexity](https://essentia.upf.edu/reference/std_DynamicComplexity.html) | Variation about a recording's level describes dynamics; boundary silence distorts a simple global measure. | Keep dynamics relative, examine local activity, and separate punch from loud mastering. Wide dynamic range alone does not establish dance energy. |
| [Essentia: danceability](https://essentia.upf.edu/reference/std_Danceability.html) | Danceability has its own statistical estimator and assumptions. | Do not rename a weighted feature score as a validated danceability or perceptual model. Human ranking/listening remains an acceptance layer. |

The numerical rules below are engineering proposals, not thresholds established by these sources. Beats, bars and phrases are separate: the current product targets constant-tempo 4/4, but a bar boundary alone does not prove a musical phrase or semantic section.

## Alternatives

1. Continue RMS/BPM ranking: cheap and immediately available, but confuses mastering with intensity. Rejected as the principal energy signal.
2. Add local, explainable, gain-independent features and conservative technique rules: recommended first increment. Works with existing PCM, requires no extra model/runtime/licence, and can be tested with controlled audio.
3. Add trained vocal, semantic structure, genre and perceptual-energy models: potentially richer, but needs separately reviewed models/licences/runtime cost and a labelled evaluation set. A later target rather than an implied capability of this increment.

## Musical-energy contract

Store a separately versioned, source-identity-bound supplementary profile. Do not revise the existing BPM/downbeat detector or invalidate analysis11, reviewed grids, corrections, arrangements or PCM caches. New imports obtain the profile while PCM is already available. Existing tracks can be upgraded on the Auto Arrange worker without rerunning valid rhythm detection. Do not force a decode of every valid catalogue entry on ordinary startup.

Use local activity windows covering the source, with descriptors for onset density, bass proportion and bass attacks, transient crest/punch and active-frame dynamic range. Average channel powers so opposite-polarity stereo cannot erase the evidence. Normalize changes and ratios against the recording itself; a constant gain change should preserve the musical descriptors. Silence, corrupt samples, unsupported/future profiles and cancellation must remain explicit unknown/incomplete states. Preserve a source-time profile so a calm intro and a busy body need not share one estimate.

The initial score is an explainable activity/drive estimate, not a probability. Rhythm should dominate; bass contributes in combination with activity; dynamics modulate transient drive instead of rewarding loud compression or every wide-range recording. Recording RMS may still inform level-risk reporting/treatment, but must not be the fallback intensity score. Unknown musical features receive neutral handling plus BPM/key/placement evidence and a clear report.

Initial acceptance targets:

- Gain-scaled copies across practical levels keep their musical score and descriptors within a small numerical tolerance.
- At equal RMS and BPM, authored busy percussion ranks above sparse percussion; adding rhythmic bass increases bass activity, while a stationary bass drone does not mimic repeated kicks.
- A quieter driving fixture can outrank a louder sustained pad. Silence/DC/NaN/antiphase stereo/channel changes and 8/44.1/48kHz sources have defined outcomes.
- Time-local quiet/busy changes survive serialization and can affect the actual mapped handover, not just whole-song order.
- Cache/profile upgrades preserve exact manual BPM/key/downbeat, old automatic grid facts and source identity. Missing/malformed/future supplementary data does not poison otherwise reusable beat analysis.
- Cancellation never publishes a partial profile, replaces the mix or erases an earlier complete profile. Warm complete profiles require no new audio decode for catalogue access.

## Technique catalogue

These suitability and avoidance rules are product design inferences from the guidance and the current engine. Named semantic regions require separate evidence; RMS change flags are only activity-change suggestions.

| Technique | Useful conditions / intended role | Avoid / prerequisites | Implementation target |
| --- | --- | --- | --- |
| Phrase volume blend | Compatible tempo/key, space for a clean overlap; restrained connective join. | Dense simultaneous lead/lyric material or a long overlap without confidence. | Existing linear fade; contextual choice and duration. |
| Bass swap / EQ handover | Both tracks carry bass; preserve a single dominant low end. | Two full-strength bass parts; a handoff away from the chosen shared change. | Existing band automation; contextual choice. |
| Long progressive EQ blend | Stable repetitive material and supported long musical window. | Short radio intros, busy hooks, uncertain section timing. | Existing bands; target longer-window evidence before general automatic use. |
| Quick phrase blend | Short clean window or a deliberate lift; less layering than a long blend. | Cutting words/hooks without vocal or authored-region evidence. | Existing fade with shorter feasible phrase preference. |
| Filter-assisted handover | Clear space during a dense handover; occasional punctuation. | Repeated large sweeps, missing evidence or using a sweep to mask poor timing. | Existing filter/bass controls; restrained variety and cooldown. |
| Downbeat cut | Reliable shared musical moment, a deliberately sharp handover. | Uncertain anchor, exposing a timing error, automatic vocal interruption. | Existing click-safe cut; opt-in explicit or conservative contextual use. |
| Constant-power crossfade | Explicit user choice, suitable level relationship and material. | Assuming it normalises levels or prevents correlated-signal overshoot. | Existing explicit style; no unconditional automatic selection. |
| Drop/impact swap | Authored or confidently detected incoming impact aligned to a suitable outgoing moment. | Inferring a drop from any loud region; double breakdowns and hook collisions. | Target: semantic/activity-role evidence and placement around those anchors. |
| Echo-out / delay bridge | Resolve a phrase and carry a tail across a change or reset. | Feedback accumulation, clipped/unbounded tails, wrong clock domain, arbitrary tempo jumps. | Target: owned local effect region, bounded feedback/tail and offline/Undo/save acceptance. |
| Loop extension / loop exit | Extend a sparse intro/outro or maintain groove through a short transition window. | Looping an unintended vocal, broken loop continuity, silently changing source clocks. | Target: phrase-safe loop selection and explicit arrangement edits, not live hotcue reuse. |
| Vocal-safe swap / call-and-response | One intelligible focal vocal, or deliberate sequential hooks. | Guessing vocals from mid-band energy. | Target: reviewed vocal evidence, stems or authored vocal-safe source regions. |
| Stem/percussion bridge | Verified aligned percussion can bridge harmony or different styles. | Misaligned separation, leakage, unknown stem ownership, unnecessary layered bass. | Target: separation/phase/clock/ownership validation first. |
| Half/double-time and tempo reset | Deliberate relationship between rhythms and a clean transition moment. | Treating arbitrary tempo gaps as beatmatched; violating current fixed-4/4 scope. | Target: explicit alternate pulse relationship and transition policy; retain current 8% guard meanwhile. |
| Teases, backspins, stop effects, double drops | Deliberate performance accents with the right source material. | Repeating a trick, unverified pitch/time behavior, accidental climax collision. | Later explicit performance/effect targets; never randomly enable as variety. |

## Rules and priority

1. Protect source integrity, playback readiness, supported timing, locks, cancellation, duration and editable ownership.
2. Require a feasible musical placement and the current key/BPM rules. Prefer reliable boundaries and avoid simultaneous heavy bass; do not weaken these for a genre match or to meet a variety quota.
3. Follow the requested musical-energy trajectory with appropriate room to breathe. A gradual Build is different from making every join dramatic.
4. Prefer a genre match where it does not worsen the above. Unknown tags are usable. If all matching tracks are exhausted or unsuitable, choose another usable track and disclose the fallback.
5. Within similarly suitable choices, discourage repeated identical treatment and excessive filter/cut drama. Choose the safest repeated technique rather than forcing a different unsafe one.

Proposed first variety option: a separate **DJ set (varied)** blend choice. Preserve the existing six explicit choices. Select from techniques already supported by ordinary editable nodes, using actual handover activity/bass evidence and a short history of previous treatments. Keep a reason/usage trace in verification evidence and concise counts in the completion report. No new delay/stem DSP is implied.

## Genre preference

Offer **Any genre** plus normalized labels present in the library. Preserve the original tags. Match case/punctuation consistently and recognize only a small explicit alias list; use token boundaries rather than substring accidents such as treating trip-hop as hip-hop. A tag is catalogue evidence, not an audio classification. Do not invent genre labels for unlabelled music.

The first increment treats genre as a small, bounded ranking cost. It never removes candidates, changes the requested first song, bypasses key/BPM/placement, forces re-analysis or truncates a feasible mix. Unknown-genre and other-genre choices remain available and are counted. Genre-specific transition palettes are a later calibrated layer; the same tag does not prove that a particular overlap is safe.

## Set-level targets beyond this increment

Add explicit Wave and Journey curves separately from Build/Keep pace/Wind down, with recovery space and a planned late peak. Add theme/artist/hook recurrence checks when trustworthy metadata exists. Plan several joins ahead to avoid stranded route branches. Represent authored protected hooks and safe intro/outro regions. Let users pin tracks or anchor moments and explain each relaxation. Evaluate genre palettes against actual sets, not fixed stereotypes. Do not claim crowd reading, taste or universal musical success from offline features.

## Evaluation and delivery order

1. Supplementary energy extraction, cache safety, controlled-audio tests and ranking integration.
2. Soft genre choice and all missing/contradictory-tag fallbacks.
3. Conservative varied selection among supported techniques, exact native UI/Undo/save/render tests and usage evidence.
4. Human-ranked reference tracks/joins and listening assessment before calling the score perceptually calibrated.
5. Add the remaining catalogue techniques one at a time after their stated prerequisites; measure them against baseline joins and retain examples of rejection/fallback.

The complete native gate is required for analysis/cache/ordering changes; use verification.run_checks retention. Preserve original music, all existing analysis/corrections/arrangements, protected DownbeatReferences-v1 and dated reference baselines. No automatic production restart or detector-development reset applies to this independently versioned supplementary analysis.

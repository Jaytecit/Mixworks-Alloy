# Auto arranger: musical transitions and feasible development

12 November 2026. Research and implementation recommendations; no new application build or musical acceptance claim.

**Recommendation.** Give the arranger a bounded search over source passages, overlap lengths and coordinated treatments. Its next useful capability is to find a musically suitable handover and maintain the groove through it. Alloy already has enough playback and automation machinery for substantially better ordinary blends. More elaborate effects can follow once the planner can choose when to use them.

The priority remains constant-tempo 4/4. “Invisible” means the listener follows a continuous musical development without an unintended change of level, bass weight, rhythm or focal part. Interesting transitions can vary the entry, layering, duration and handover moment while remaining subtle. A dramatic effect is an optional artistic choice.

**Evidence reviewed.** Current checkout, relevant Git checkpoints, dated handovers, previous transition research, current guides, primary DJ teaching and academic research, and the running 0.55.113 application. Jay explicitly made the open arrangement disposable and then saved it as `Sessidafn.mixworks`. A copy and reproducible metadata audit are retained under `.local/arranger-research-1112`. The app remains open. This research performed no audio decode, detector rerun, native test suite, build or listening evaluation. The source and saved-state findings below are distinguished from musical hypotheses and proposed implementation.

**How we reached the current behaviour.**

| Checkpoint | Relevant change | What it established |
| --- | --- | --- |
| 18–20 October | Selected-pair editor, independent Volume/Bass/Mid/Treble modes, strengths, gaps, swap positions, locks and filter sweeps followed the transition-editor audit. | A reasonably expressive manual control surface. Manual Bass swap and a full volume blend are distinct treatments. See the [dated audit](DJSTUDIO-TRANSITION-AUDIT.md) and [current controls](../TRANSITION-PRESETS.md). |
| 20 October, Git `824d4e8` | Auto styles used local RMS changes for bass/filter decisions. Energy-direction admission also used RMS. | Measurement-driven rules, but recording level was an inadequate proxy for musical drive. |
| 23–24 October, present in `28cad13` | Supplementary rhythm/bass/dynamics profiles, soft energy/genre preferences, DJ set variety, Extend, source-clock repairs and node simplification. | Better selection preferences and editable output. Semantic phrasing, vocals, loops and effects were already identified as future work in the [research targets](DJ-ARRANGEMENT-TARGETS-20261023.md). |
| 5 November local checkpoints | Fixed catalogue-only planning stopping after one song and generated automation being lost at commit. Added equal-power blending, attenuation-only song trims and movement locks. | Arrangement completion and curve ownership improved. The handover explicitly left real-music listening open. |
| By the 0.55.84/.88 checkpoints | Balanced, Bold and DJ set selected a full-band volume swap; nominal ramp approximately 4 ms. Some EQ/filter movement surrounded the switch. Tests were updated to expect this treatment. | Long visible overlaps could contain almost no audible blending. Git `60b355f` preserves this behaviour; the [earlier 12 November audit](AUTO-ARRANGER-MIXING-AUDIT-20261112.md) explains it. This identifies the recorded behaviour, not the original motivation for changing it. |
| 12 November, 0.55.106 | Restored short equal-power blends; added 8/4-bar preference, preceding-join room checks, local balance comparison, bounded bass measurement and replaceable generated-node ownership. | 188 focused Auto Arrange checks and 17 comparison checks passed. One real pair had three native renders; the record did not claim listening preference or broad musical coverage. |
| Current 0.55.113 | Subsequent timeline and grid work retained the repair. | Current source and the saved mix show actual fades. Their narrow selection and timing policy remain limiting. |

Historical technical passes demonstrate the checked behaviours. They do not establish that the automatic choices sound like a skilled DJ.

**What can be selected today.**

| Auto Arrange choice | Current implementation | Important limit |
| --- | --- | --- |
| Balanced | Equal-power full-band blend, aiming for 8 bars; 4 bars where room or measured balance supports it. Adds bass handover if both short sampled passages qualify. | Does not search semantic phrase cues. |
| Smooth | Linear full-band crossfade, using section placement. | “Smooth” names the curve; it does not guarantee smooth musical development. |
| Bold | Same short blend and bass check as Balanced; optional filter at 0.26 depth for measured level contrast. | Not a separate drop/impact technique. |
| Use my settings | Insertion preset, length and section preference. Insertion presets are Align IN/OUT, Bass swap and Crossfade. | Does not automatically choose among the user's saved named pair presets. |
| Constant power | Equal-power full-band crossfade, using section placement. | Equal power neither matches local loudness nor guarantees the processed sum stays constant. |
| Hard cuts only | Grid/section-positioned volume swap with click-prevention ramp. | A shared bar or energy flag does not establish a suitable musical resolution/drop. |
| DJ set (varied) | Same short blend/bass treatment; local activity may add a shallow filter, with a two-join filter cooldown. | It is a small rule variation, not a broad palette of DJ techniques. |

The manual editor additionally supports separate band fades/swaps, a one-beat Bass fade handover, adjustable crossing positions/gaps/depths, independent filter sweeps, editable curves, named/portable presets and A/B/C rehearsal. These are usable implementation building blocks. See [Auto Arrange](../AUTO-ARRANGEMENT.md), [presets](../TRANSITION-PRESETS.md) and [filters](../TRANSITION-FILTERS.md).

There are **two planning entry points**. Library Auto Arrange selects songs and calls `applyAutoArrangementBass` after planning. Auto Arrange timeline retains chronological order, tries section placement with a 128-beat preference, then uses the Balanced volume treatment. It does not call that bass pass and does not share the library planner's 8/4-bar search and preceding-join room policy. Changing one path alone will leave inconsistent results. This is visible in `prepareOrderedTimeline`, `planAutoArrangement` and the UI call sites.

**What the open arrangement demonstrates.**

The saved session contains 25 songs, 24 joins and approximately 91 minutes 58 seconds of arranged time:

- 18 joins are labelled Short blend; six are Bass handover.
- Every source offset is zero. All 24 transition frames end at the outgoing source's played end; there is no alternative internal exit selection in this example.
- Frame lengths range from 6.69 to 7.81 bars, median 7.37. These are frame spans calculated from saved source coordinates and source BPM, not measured audible-overlap lengths. Fractional spans alone do not prove incorrect phrasing: source pre-roll, file ends and alignment can explain them.
- Volume automation exists on all 25 clips and bass automation on 11. No mid, treble or filter automation is present. This is a clear example of the limited variety, not proof that filters were needed.
- All 25 relevant analysis records match the saved source digest, frame count and sample rate. All store G major and have key-confidence values below the current 0.55 review threshold; none has a manual key override. Confidence is not a probability, and this does not prove the keys are wrong. It does mean the equal-key chain is not independently confirmed harmonic compatibility.
- Overnight (Edit) and Triumvirate have no accepted downbeat origin and use tentative downbeat guesses. Neither has a manual downbeat. Their audible bar phases have not been checked in this research.

The saved example includes many radio/edit versions. Their names cannot establish where vocals or clean intros occur. They are useful test material precisely because assuming a long, uncomplicated DJ intro would be unsafe.

The observed view showed Are You Ready into It's A New Dream, labelled Bass handover. Retained state identifies that as join 9, with a 7.42-bar frame. It should be one of the comparison cases. No specific audible defect is assigned to that pair without listening.

**Why the choices remain mechanical.**

1. **Order and musical join quality are insufficiently coupled.** Candidate ranking primarily uses tempo, key and optional activity/genre. With Unrestricted energy there is no transition-quality cost in the final candidate score. The short-window balance comparison can replace an 8-bar placement with 4 bars, but it does not reject a poor partner on musical-clash evidence. Bass is checked after the chosen order and placements.
2. **The main repaired styles bypass section placement.** Balanced, Bold and DJ set set the section flag false. Their bass handover is on the project bar nearest the frame midpoint. The current policy thus cannot intentionally choose an earlier sparse passage or move the handover to an evidenced musical change.
3. **Existing sections are coarse evidence.** `EnergySections` derives suggestions from waveform RMS and a source bar grid, with authored boundaries also supported. It does not recognise verse, chorus, sung lines, a hook or a resolved phrase. An 8/16/32-bar interval is a duration hypothesis; its phrase offset still needs evidence.
4. **Confidence is not carried into technique choice.** `Media::grid()` can promote `downbeatGuess` into a usable bar grid. `Media::key()` can return a best guess, and eligibility does not require `keyConfirmed()`. Preserve the distinction between reviewed, accepted automatic and tentative facts. Do not silently treat them alike for long tonal overlaps or exposed cuts.
5. **Volume and EQ need joint design.** A symmetric equal-power fade can lower the outgoing bass before the incoming bass becomes dominant. Adding an EQ handover can reduce the combined bass further. That is a plausible control interaction to measure, not a diagnosed PCM defect in this session. Source RMS/headroom trims cannot by themselves prove continuous level or weight after processing.

Primary source locations: [planner and treatment](../../Source/AutoArrangement.cpp), [UI preparation](../../Source/AutoArrangementUI.cpp), [section evidence](../../Source/EnergySections.cpp), [band envelopes](../../Source/TransitionBands.cpp), [confidence accessors](../../Source/Model.h), [source/project mapping](../../Source/Model.cpp).

**What skilled DJ practice suggests.**

DJ teaching places cue selection, matched timing and musical phrases ahead of the fader move. Fades, single-phrase beatmixes, bass swaps and filter handovers have different uses. The incoming part should become established without obscuring the outgoing focal part. [Native Instruments' transition guide](https://www.native-instruments.com/blogs/blog/dj-transitions).

Genre informs likely technique choices: long groove layering, distinct phrase handovers, short hook-led transitions and deliberate cuts serve different music. The source's genre descriptions are tendencies, not rules for every recording. For Alloy, tags should remain a preference while the actual passages determine eligibility. [Pioneer DJ's genre comparison](https://blog.pioneerdj.com/djtips/we-uncover-the-mixing-techniques-behind-every-major-genre/).

Hotcues, loops, teasing and effects can deliberately set up an incoming impact. Echo-outs can carry an outgoing ending across a change; their timing and the next entry still matter. These expressive moves should be occasional, evidence-led options. [Crossfader's worked performance](https://wearecrossfader.co.uk/blog/pro-dj-methods-explained/) and [echo/cue example](https://wearecrossfader.co.uk/blog/how-to-mix-any-genre-in-three-easy-steps/).

Across the set, energy can rise, settle and recover. Tempo/key alone cannot describe this trajectory. Existing energy policy is useful, but no variety quota should force a weak transition. [Roland's set-planning guidance](https://articles.roland.com/crafting-a-great-dj-set/).

**A realistic technique palette for Alloy.** The build assessments below are engineering inferences from the inspected engine, not assurances of automatic musical accuracy.

| Technique | When it serves the music | Feasible implementation and boundary |
| --- | --- | --- |
| Short phrase handover | A brief clean entry/exit, or dense material that should overlap only briefly. | Existing faders and timing. Compare 4/8-bar candidates around suitable cues; permit asymmetric fades. Short is a fallback, not a guarantee against cutting a vocal. |
| Progressive groove blend | Stable compatible passages with space for layering. | Existing volume/EQ curves can support 16/32 bars. Need better boundary evidence and local overlap scoring. Exclude uncertain long melodic/vocal overlaps. |
| Staged bass/EQ handover | Both passages contain rhythm and low end. | Introduce selected upper content, retain outgoing bass weight, transfer bass at the chosen musical moment, then remove remaining outgoing content. Coordinate faders and EQ; measure the combined result. Avoid universal midpoint, fixed-depth and fixed-duration rules. |
| Percussion-first or percussion-last blend | A verified sparse/drum-led passage can preserve the groove without competing lead parts. | First use clean passages in the original audio and gentle EQ. With ready, validated stems, existing stem-volume automation can isolate percussion more effectively. EQ alone is not drum separation. |
| Restrained filter handover | A specific conflicting frequency region benefits from clearance. | Existing filter automation; choose direction/depth according to both passages. Retain a cooldown and compare with an unfiltered alternative. A loudness change alone is inadequate justification. |
| Breakdown-to-entry or resolution-to-impact | The outgoing phrase resolves as the incoming groove/impact arrives. | Timing, trims and fades already exist. Use reviewed source cues first. Automatic structural/impact detection needs its own validation; do not classify every energy rise as a drop. |
| Vocal-safe handover | One foreground vocal finishes before another begins. | Authored vocal-safe intervals are reliable input. A validated detector or prepared vocal stems can extend coverage. Existing stem volume can help; mid EQ cannot reliably remove a vocal. Phrase completion still requires evidence. |
| Loop-assisted bridge | A verified clean instrumental loop extends a short intro/outro. | Clip loops and repeat edits already exist. Add automatic loop suitability, seam checks, duration/phase ownership, neighbouring-join protection and explicit editable loop placement. Do not equate existing playback loops with safe autonomous selection. |
| Echo-out / reverb tail | A deliberate resolution or tempo/style reset benefits from a bounded tail. | Generic plugin hosting and tail infrastructure exist; there is no reliable built-in automatic echo recipe. Add an owned processor/region with input/send gating, feedback/decay limits, pre/post-fader rules and exact render/save/reopen behaviour. Moderate engine work. |
| Stem bridge / sequential hooks | Verified separated parts create musical space unavailable in the full mix. | Aligned local stems plus independent volume/pan already exist. Add automatic stem selection and transition ownership, with leakage listening checks. Independent stem EQ/filter/VST remains unfinished. Separation quality is not guaranteed by successful recombination. |
| Cuts, half/double-time changes, double drops, teases, brake/backspin | Explicit expressive moments. | Cuts exist; the others need suitable source cues and, depending on technique, new playback/effect policy. Keep out of the first fluid-mixing milestone. Preserve the current tempo bound until a separate tempo-transition contract is demonstrated. |

The [stem guide](../STEM-SEPARATION.md), [timeline editing guide](../TIMELINE-EDITING.md) and [roadmap](../../FINISHING-PLAN.md) distinguish implemented infrastructure from unfinished musical use.

One possible **16-bar progressive handover** illustrates what existing controls could already express:

| Passage | Intended audible change |
| --- | --- |
| Opening four bars | Incoming percussion/texture becomes audible while outgoing groove and lead remain intelligible. Incoming bass is reduced only if needed. |
| Following four bars | Incoming identity develops; outgoing conflicting mid/high content is reduced where warranted. |
| Supported boundary, possibly near bar 9 | Bass ownership changes with duration and fader levels chosen for these passages. The handover may be early or late. |
| Remaining bars | Outgoing layers resolve and disappear; the incoming track takes the lead at stable level. |

This is a candidate family, not a universal preset. Some pairs need a short handover, some a different entry, and some a different partner. The final curve cannot be specified from bar count alone.

**Implementation order and observable milestones.**

**First: a better join between the same two songs.** Factor a shared pair planner used by both Auto Arrange entry points. Represent each candidate with outgoing/incoming source intervals, entry and exit cues, overlap, principal handover point, technique, ordinary editable controls, evidence and rejection reasons. Compare a bounded set of 4/8/16-bar possibilities; permit 32 bars only with suitable evidence. Use current source sections/hotcues as candidates, respecting authored intent; automatic hotcues are suggestions, not reviewed labels. Allow earlier exits and later entries under an explicit track-preservation policy so the user understands skipped material.

Start with short fade, progressive EQ/bass handover and selective filter assistance. Score silence/quiet tails, local level change, sustained bass overload or loss, spectral crowding, timing confidence, protected focal regions where known, and room for the middle song to lead. Unknown vocals must remain unknown. Generate volume and EQ together, then measure a few finalist renders through the actual engine. Existing masters/plugins/tempo preparation must be represented; a raw-source sum is only a preliminary screen. Missing evidence should trigger a conservative candidate or review flag, with a reason.

Success means the same pair offers meaningfully different usable passages/treatments, the chosen one improves the matched listening comparison, and its curves, neighbouring joins, Undo and save/reopen remain correct. Begin with joins 1, 9 and 16 from the retained session, then add reviewed counterexamples; those join numbers select evidence cases, not proven failures.

**Second: choose songs using the quality of their possible joins.** Apply join-quality costs even with Unrestricted energy. Use existing metadata to shortlist partners, then evaluate actual windows only for finalists. Keep a small bounded lookahead so a locally attractive choice does not strand the next song or force two handovers together. Variety should break ties among similarly suitable candidates: different durations, entries and EQ sequencing count, without forced filter/cut usage. Report the selected technique, actual audible span, reason and uncertainty. Reuse the current completion report and rehearsal infrastructure rather than exposing a wall of technical scores.

Add a separately versioned, source-bound supplementary profile with finer local level/bass/attack/timbre measurements and candidate structural boundaries. Preserve current analysis and manual reviews. Reuse decoded audio when available, analyse on demand in a cancellable worker, and avoid a forced whole-library rescan. Reviewed annotations can provide semantic truth while automatic labels are still being evaluated.

**Third: expand only with demonstrated prerequisites.** Add safe loop bridges, then a bounded native echo-out, then optional stem-aware transitions and validated vocal/structure inference. These are buildable extensions. Each needs its own audible cases and clock/ownership tests. Public model availability is not evidence that an integration will reliably understand Jay's collection.

Published work supports cue/structure search as a realistic direction. An EDM switch-point study used feature changes and novelty analysis; its reported dataset result is not an Alloy accuracy estimate. A later study identified energy, harmonic and timbral changes as useful switch-point evidence. [2020 cue-point paper](https://arxiv.org/abs/2007.08411), [2024 interpretability study](https://www.mdpi.com/2624-6120/5/4/36).

A complete rule-based Drum and Bass system demonstrates bounded genre-specific automation, but its vocal detector missed clashes and its authors reported remaining musical limitations. This supports incremental, evaluated heuristics rather than a claim of universal automatic phrasing. Research using learned EQ/fader controls also demonstrates that richer transitions can use ordinary processing; it does not require Alloy to adopt a generative model. [Integrated system and evaluation](https://link.springer.com/article/10.1186/s13636-018-0134-8), [learned transition controls](https://arxiv.org/abs/2110.06525).

All-In-One is a possible later structure-analysis experiment: it predicts segment labels/boundaries alongside beats/downbeats. Its current documentation requires PyTorch and a NATTEN source build on Windows. The repository's MIT label does not establish clearance for every dependency/model in an Alloy package. Benchmark its source-time alignment, labels, cost and deployment prerequisites on a small frozen set before adoption; do not replace the trusted grid merely because it also outputs beats. [Maintainer documentation](https://github.com/mir-aidj/all-in-one).

The requested skills.sh check surfaced general sound-engineering and DSP skills, not a demonstrated Alloy/DJ-arrangement method. No skill package was installed. The approach above therefore uses project evidence and primary research rather than generic game-audio instructions. [Directory](https://skills.sh), [general audio skill](https://www.skills.sh/curiositech/some_claude_skills/sound-engineer).

**How to establish that it is better.**

Use a small frozen set first: sparse instrumental intros/outros; two strong bass parts; vocal/hook overlap; an incoming breakdown; a fading/quiet outgoing tail; short radio edits; a short middle song; tentative timing/key; and one pair where the current simple blend is already good. Reuse cases where these conditions coincide. Include separate holdout pairs before widening automatic use.

For each case compare the current candidate, an improved treatment at fixed cues, and improved cues plus treatment. That separates the value of cue selection from curve sophistication. Audition context before and after each join, with randomized identities and one constant gain adjustment per comparison excerpt. Do not dynamically normalize away a dip. Judge groove continuity, bass weight, focal-part clarity, phrase resolution, unwanted level changes and whether the transition serves the set. Explicitly record worse/tied cases.

Technical evidence should check the rendered audible overlap, timing, peaks and local band/level trajectories; protect manual nodes and both neighbours; exercise Undo/Redo and source-backed save/reopen; and cover both planner entry points. A low RMS discrepancy cannot establish an unobtrusive transition. Use focused serial checks through `verification.run_checks`, stress off, at useful checkpoints. No broad suite or new build was needed for this research. For later verified builds, retain the standing patch-version, complete local bundle and launcher rules.

The next preview should demonstrate a convincing pair and a short chain with clear musical improvement. Better scoring counters, larger technique menus or synthetic test totals alone are insufficient acceptance.

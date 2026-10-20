#include "Engine.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
namespace
{
constexpr double sampleRate = 22050;
constexpr double pi = juce::MathConstants<double>::pi;
juce::AudioBuffer<float> layeredRhythm(double bpm, double origin, double seconds, bool hats, bool offbeatBass)
{
    juce::AudioBuffer<float> audio(2, (int) std::round(seconds * sampleRate)); audio.clear();
    const double period = 60 / bpm;
    juce::Random random(17563);
    auto add = [&](double start, double duration, bool bass, float gain)
    {
        for (int i = 0; i < (int) (duration * sampleRate); ++i)
        {
            const int at = (int) std::round(start * sampleRate) + i; if (at < 0 || at >= audio.getNumSamples()) continue;
            const double t = i / sampleRate;
            const float value = gain * (float) (bass ? std::sin(2 * pi * (75 * t - 150 * t * t)) * std::exp(-t * 35)
                : (random.nextFloat() * 2 - 1) * std::exp(-t * 100));
            for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, at, value);
        }
    };
    for (int n = 0; origin + n * period < seconds; ++n)
    {
        const double at = origin + n * period;
        add(at, 0.12, true, n % 4 == 0 ? 0.65f : 0.45f);
        // Syncopated, broadband accents have more total spectral bins than the kick.
        if (hats) { add(at + period * 0.375, 0.06, false, 0.8f); add(at + period * 0.75, 0.06, false, 0.55f); }
        if (offbeatBass)
            for (int i = 0; i < (int) (period * 0.4 * sampleRate); ++i)
            {
                const int index = (int) std::round((at + period * 0.5) * sampleRate) + i;
                if (index >= audio.getNumSamples()) break;
                const double t = i / sampleRate, envelope = std::min(1.0, t / 0.025) * std::exp(-t * 12);
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, index, (float) (0.3 * envelope * std::sin(2 * pi * 110 * t)));
            }
    }
    return audio;
}
// Same drum construction as the preserved 40-second red fixture. Events are
// construction truth; no commercial labels or reserved recordings are used.
juce::AudioBuffer<float> timingControl(bool tempoChange, bool phaseEdit, bool missingAlternates)
{
    constexpr double rate = 44100;
    juce::AudioBuffer<float> audio(2, (int) (40 * rate)); audio.clear();
    double time = .5; int ordinal = 0;
    while (time < 39.5)
    {
        if (phaseEdit && ordinal == 40) time += .125;
        if (!(missingAlternates && ordinal >= 40 && ordinal % 2 != 0))
        {
            const int start = (int) std::nearbyint(time * rate);
            for (int i = 0; i < (int) (.12 * rate) && start + i < audio.getNumSamples(); ++i)
            {
                const double t = i / rate;
                const double kick = .45 * std::sin(2 * pi * (60 * t - 35 * t * t)) * std::exp(-35 * t);
                const double attack = .18 * std::sin(2 * pi * (ordinal % 4 == 0 ? 1600 : 2500) * t) * std::exp(-150 * t);
                const float value = (float) (std::nearbyint((kick + attack) * 32767) / 32768);
                for (int channel = 0; channel < 2; ++channel) audio.addSample(channel, start + i, value);
            }
        }
        time += tempoChange && ordinal >= 40 ? .4 : .5; ++ordinal;
    }
    return audio;
}
juce::AudioBuffer<float> constantFourFour(float kickGain, float snareGain, float hatGain, float bassGain, int phase, double seconds = 64)
{
    juce::AudioBuffer<float> audio(2, (int) (seconds * sampleRate)); audio.clear();
    juce::Random random(1832);
    auto add = [&](double at, double seconds, auto sample)
    {
        for (int i = 0; i < (int) std::round(seconds * sampleRate); ++i)
        {
            const int frame = (int) std::round(at * sampleRate) + i;
            if (frame < 0 || frame >= audio.getNumSamples()) continue;
            const float value = (float) sample(i / sampleRate);
            for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, frame, value);
        }
    };
    for (int n = 0; .213 + n * .48 < seconds - .2; ++n)
    {
        const double at = .213 + n * .48;
        add(at, .12, [&](double t) { return kickGain * std::sin(2 * pi * (75 * t - 150 * t * t)) * std::exp(-35 * t); });
        if (((n - phase) % 2 + 2) % 2 == 1)
        {
            double previous = 0;
            add(at, .16, [&](double t) { const double noise = 2 * random.nextFloat() - 1, value = noise - previous;
                previous = noise; return snareGain * value * std::exp(-45 * t); });
        }
        add(at + .24, .06, [&](double t) { return hatGain * std::sin(2 * pi * 7100 * t) * std::exp(-80 * t); });
        add(at + .24, .192, [&](double t) { return bassGain * std::min(1., t / .025) * std::exp(-12 * t) * std::sin(2 * pi * 110 * t); });
        if (n >= phase && (n - phase) % 16 == 0)
            add(at, .7, [&](double t) { return .18 * std::min(1., t / .01) * std::exp(-6 * t)
                * (std::sin(2 * pi * 130.8128 * t) + std::sin(2 * pi * 155.5635 * t) + std::sin(2 * pi * 195.9977 * t)); });
    }
    audio.applyGain(.94f / std::max(.94f, audio.getMagnitude(0, audio.getNumSamples())));
    return audio;
}
}
void runConstantFourFourTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    for (const auto settings : {std::pair<bool, bool>{true, false}, {true, true}, {false, true}})
    {
        const double bpm = 123.4567, origin = 0.213;
        const auto analysis = analyseMusic(layeredRhythm(bpm, origin, 150, settings.first, settings.second), sampleRate);
        const double phase = analysis.bpm > 0 ? std::abs(std::remainder(analysis.beatOrigin - origin, 60 / bpm)) : 1;
        const double drift = analysis.bpm > 0 ? std::abs(60 / analysis.bpm - 60 / bpm) * 300 : 1;
        check(analysis.bpm > 0 && std::abs(analysis.bpm - bpm) < 0.015 && phase < 0.012 && drift < 0.012 && !analysis.variableTempo,
            "layered rhythm hats=" + juce::String((int) settings.first) + " bass=" + juce::String((int) settings.second)
            + ": " + juce::String(analysis.bpm, 4) + " BPM, phase " + juce::String(phase * 1000, 2) + " ms, drift " + juce::String(drift * 1000, 2) + " ms");
    }
    struct Case { const char* name; float kick, snare, hat, bass; int phase; };
    for (const auto& setting : {Case{"standard 4/4", .5f, .35f, .2f, .25f, 0},
                               Case{"strong offbeat bass", .2f, .35f, .15f, 1.2f, 0},
                               Case{"strong backbeats", .12f, .7f, .4f, .25f, 0},
                               Case{"two-beat pickup", .5f, .35f, .2f, .25f, 2}})
    {
        const auto analysis = analyseMusic(constantFourFour(setting.kick, setting.snare, setting.hat, setting.bass, setting.phase), sampleRate);
        check(std::abs(analysis.bpm - 125) < .02 && !analysis.variableTempo,
              juce::String(setting.name) + " retains a steady 125 BPM grid");
        const double firstError = std::abs(std::remainder(analysis.beatOrigin - .213, .48));
        const double lastError = analysis.bpm > 0 ? std::abs(std::remainder(analysis.beatOrigin + 128 * 60 / analysis.bpm - (.213 + 128 * .48), .48)) : 1;
        check(firstError < .025 && lastError < .025,
              juce::String(setting.name) + " follows scored beats throughout: " + juce::String(firstError * 1000, 2)
              + "/" + juce::String(lastError * 1000, 2) + " ms; neural correction " + juce::String(analysis.neuralPhaseCorrectionMs, 2) + " ms");
        // The bass-heavy loop produces competing half-bar predictions; its
        // quiet chord alone does not require a confident bar interpretation.
        // Availability and correctness are distinct: an asserted bar must fit.
        const bool ambiguous = setting.bass > 1;
        check((ambiguous && analysis.downbeatOrigin < 0 && analysis.downbeatConfidence == 0)
              || (analysis.downbeatOrigin >= 0 && std::abs(std::remainder(analysis.downbeatOrigin - (.213 + setting.phase * .48), 1.92)) < .025),
              juce::String(setting.name) + (ambiguous ? " retains a correct bar phase or explicit ambiguity" : " identifies the scored 4/4 bar phase using any reliable downbeat anchor"));
    }
    {
        auto audio = constantFourFour(.2f, .35f, .15f, 1.2f, 0, 96);
        // A clear recurring beat-one accent appears only after 60 seconds.
        // The tempo and meter stay fixed; the opening need not identify bar one.
        for (int n = 128; .213 + n * .48 < 95.8; n += 4)
            for (int i = 0; i < (int) (.12 * sampleRate); ++i)
            {
                const int at = (int) std::round((.213 + n * .48) * sampleRate) + i;
                const double t = i / sampleRate;
                const float value = (float) (.8 * std::sin(2 * pi * (75 * t - 150 * t * t)) * std::exp(-35 * t));
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, at, value);
            }
        const auto analysis = analyseMusic(audio, sampleRate);
        check(!analysis.variableTempo && std::abs(analysis.bpm - 125) < .02
              && std::abs(std::remainder(analysis.beatOrigin - .213, .48)) < .025,
              "a clear late 4/4 passage retains the same constant tempo and pulse as the bass-heavy opening");
        check(analysis.downbeatOrigin >= 0 && analysis.downbeatConfidence > 0
              && std::abs(std::remainder(analysis.downbeatOrigin - .213, 1.92)) < .025,
              "reliable downbeat evidence after 60 seconds establishes the track's 4/4 bar phase");
    }
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1, 80 * 8000); media->samples.clear();
    media->analysis.bpm = 120; media->analysis.beatOrigin = .213; media->analysis.tempoConfidence = 1;
    media->analysis.downbeatOrigin = 60.213; media->analysis.downbeatConfidence = 1;
    check(media->grid().hasBar() && std::abs(media->grid().beatAt(.213) + 120) < 1.0e-8
          && std::abs(media->grid().timeAtBeat(-120) - .213) < 1.0e-8,
          "a reliable downbeat 60 seconds into a steady source anchors its earlier beat/bar grid");
    for (double offset : {0., 3.77})
    {
        Project project; Clip clip; clip.media = media; clip.offset = offset; clip.length = 80 - offset;
        clip.pendingPlacement = PendingPlacement::append; project.clips = {clip};
        const auto prepared = prepareTempoMap(project, false, 16);
        bool aligned = prepared.error.isEmpty() && prepared.matched == 1;
        for (double anchor : {.213, 20.213, 60.213, 76.213}) if (anchor >= offset)
        {
            const auto& placed = prepared.project.clips[0];
            const double at = placed.start + placed.localTimeForSource(anchor - placed.offset);
            aligned &= std::abs(std::remainder(prepared.project.tempo.beatsAt(at), 4)) < 1.0e-7;
        }
        check(aligned, "late source downbeat maps earlier and later bars onto project bar lines, source trim " + juce::String(offset, 2));
    }
    media->analysis.downbeatOrigin = -1; media->analysis.downbeatConfidence = 0;
    Project pulseOnly; Clip clip; clip.media = media; clip.length = 80; clip.pendingPlacement = PendingPlacement::append; pulseOnly.clips = {clip};
    const auto prepared = prepareTempoMap(pulseOnly, false, 16);
    check(prepared.error.isEmpty() && prepared.matched == 1 && prepared.project.clips[0].firstDownbeatTime() < 0,
          "reliable pulse-only alignment remains available without inventing beat one");
    juce::ignoreUnused(report);
}
void runRhythmReliabilityTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    runConstantFourFourTests(check, report);
    // The distributed physical windows cover [0,20), [50,70), ... here.
    // Changes in [22,48) must not inherit the surrounding steady grid merely
    // because the original physical sampling did not examine that passage.
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        auto audio = layeredRhythm(120, .213, 420, false, false);
        audio.clear((int) (22 * sampleRate), (int) (26 * sampleRate));
        const double interval = scenario == 0 ? .4 : scenario == 1 ? .5 : 1.;
        for (int n = 0; 22.213 + n * interval < 47.9; ++n)
        {
            const double at = 22.213 + n * interval + (scenario == 1 ? .125 : 0);
            for (int i = 0; i < (int) (.12 * sampleRate); ++i)
            {
                const int frame = (int) std::round(at * sampleRate) + i;
                const double t = i / sampleRate;
                const float value = (n % 4 == 0 ? .65f : .45f)
                    * (float) (std::sin(2 * pi * (75 * t - 150 * t * t)) * std::exp(-35 * t));
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, frame, value);
            }
        }
        RhythmDiagnostics evidence;
        const auto analysis = analyseMusic(audio, sampleRate, {}, &evidence);
        const bool expectedReview = scenario != 2;
        const juce::String name = scenario == 0 ? "150 BPM passage" : scenario == 1 ? "125ms phase edit" : "missing alternate beats";
        check(analysis.variableTempo == expectedReview && std::abs(analysis.bpm - 120) < .02,
              "unsampled " + name + " is classified without replacing the surrounding 120 BPM estimate: BPM "
              + juce::String(analysis.bpm, 4) + ", review " + juce::String((int) analysis.variableTempo));
        check(std::all_of(evidence.localFits.begin(), evidence.localFits.end(), [](const auto& fit)
              { return fit.examinedPhysicalSeconds <= fit.sourceEnd - fit.sourceStart + .001; }),
              "targeted " + name + " never counts physical window coverage twice");
        if (expectedReview)
        {
            check(analysis.downbeatOrigin < 0,
                  "unsampled " + name + " cannot publish a confident fixed-grid bar");
            // Either passage may seed the provisional global phase. Require
            // measurement of the changed passage and an independent conflict,
            // without prescribing which passage contradicts the chosen seed.
            check(std::any_of(evidence.localFits.begin(), evidence.localFits.end(), [scenario](const auto& fit)
                  { return fit.sourceStart == 20 && fit.examinedPhysicalSeconds >= 19.99 && fit.hasPhysicalFit
                      && std::abs(fit.physicalBpm - (scenario == 0 ? 150 : 120)) < .2; })
                  && std::any_of(evidence.localFits.begin(), evidence.localFits.end(), [](const auto& fit) { return fit.conflict; }),
                  "unsampled " + name + " receives independent physical timing corroboration");
            auto media = std::make_shared<Media>(); media->sampleRate = sampleRate;
            media->samples = std::move(audio); media->analysis = analysis;
            Project project; Clip clip; clip.media = media; clip.length = media->duration(); project.clips = {clip};
            const auto prepared = prepareTempoMap(project, false, 0);
            check(prepared.error.isEmpty() && prepared.skipped == 1 && !prepared.project.clips[0].beatmatched
                  && prepared.project.clips[0].length == 420,
                  "unsampled " + name + " leaves automatic playback unsynced and preserves its source span");
        }
    }
    for (int scenario = 0; scenario < 4; ++scenario)
    {
        const bool expectedReview = scenario == 1 || scenario == 2;
        const juce::String name = scenario == 0 ? "steady percussion" : scenario == 1 ? "120-to-150 tempo step" : scenario == 2 ? "125ms source phase edit" : "missing alternate attacks";
        auto audio = timingControl(scenario == 1, scenario == 2, scenario == 3); RhythmDiagnostics evidence;
        const auto analysis = analyseMusic(audio, 44100, {}, &evidence);
        const bool independentConflict = std::any_of(evidence.localFits.begin(), evidence.localFits.end(), [](const auto& fit) { return fit.conflict; });
        check(analysis.variableTempo == expectedReview, name + " retains the correct fixed-grid review state: BPM " + juce::String(analysis.bpm, 4));
        check(independentConflict == expectedReview && !evidence.physicalTransients.empty(), name + " derives local conflict from independent physical timing, not missing predicted beats");
        if (expectedReview) check(analysis.downbeatOrigin < 0 && analysis.downbeatConfidence == 0, name + " cannot publish a confident all-source bar phase");
        if (scenario == 1)
        {
            check(std::any_of(evidence.localFits.begin(), evidence.localFits.end(), [](const auto& fit)
                  { return fit.conflict && fit.sourceStart >= 20 && fit.hasPhysicalFit && std::abs(fit.physicalBpm - 150) < .2; }),
                  "the tempo-step conflict retains a source-owned physical 150 BPM local fit");
            auto media = std::make_shared<Media>(); media->sampleRate = 44100; media->samples = std::move(audio); media->analysis = analysis;
            Project project; Clip clip; clip.media = media; clip.length = 40; project.clips = {clip};
            const auto prepared = prepareTempoMap(project, false, 0);
            check(prepared.error.isEmpty() && prepared.skipped == 1 && !prepared.project.clips[0].beatmatched && prepared.project.clips[0].length == 40,
                  "automatic preparation leaves the corroborated changing source unsynced with its original audio span");
            project.tempo.setPoint(0, 120); project.followTrackTempo = true;
            project.clips[0].beatmatched = true; project.clips[0].tempoSourceBpm = 120;
            const auto restored = prepareSavedClips(project);
            check(restored.isEmpty() && project.clips[0].beatmatched && project.clips[0].tempoSourceBpm == 120 && project.clips[0].length == 40,
                  "saved playback geometry retains its captured clock when refreshed source facts require review");
        }
    }
    {
        RhythmDiagnostics evidence; evidence.localFits.push_back({}); evidence.physicalTransients.push_back({1, 1, 1});
        auto cancelled = std::make_shared<std::atomic<bool>>(true); juce::AudioBuffer<float> audio(2, 44100);
        const auto analysis = analyseMusic(audio, 44100, cancelled, &evidence);
        check(analysis.rhythmIssue.containsIgnoreCase("cancel") && evidence.localFits.empty() && evidence.physicalTransients.empty(),
              "cancelled analysis clears stale or partial physical diagnostics");
    }
    {
        auto audio = timingControl(false, false, false); audio.clear(0, 20 * 44100); RhythmDiagnostics evidence;
        analyseMusic(audio, 44100, {}, &evidence);
        check(!evidence.localFits.empty() && evidence.localFits[0].examinedPhysicalSeconds == 20
              && evidence.localFits[0].physicalEvents == 0 && !evidence.localFits[0].hasPhysicalFit && !evidence.localFits[0].globalPhysicalSupport,
              "an examined silent interval retains unknown physical timing support rather than a fabricated zero score");
    }
    auto correctedMedia = std::make_shared<Media>(); correctedMedia->sampleRate = sampleRate;
    correctedMedia->samples = layeredRhythm(120, 0.213, 24, false, false); correctedMedia->manualBpm = 120;
    Clip correctedClip; correctedClip.media = correctedMedia; correctedClip.length = 24; correctedClip.beatmatched = true;
    Project correctedProject; correctedProject.clips = {correctedClip};
    auto beforeCorrection = prepareTempoMap(correctedProject, false, 0);
    correctedMedia->manualBpm = 128;
    auto afterCorrection = prepareTempoMap(beforeCorrection.project, false, 0);
    check(afterCorrection.error.isEmpty() && std::abs(afterCorrection.project.clips[0].length - 25.6) < 1.0e-7
        && std::abs(afterCorrection.project.clips[0].sourceElapsed(afterCorrection.project.clips[0].length) - 24) < 1.0e-7
        && afterCorrection.project.clips[0].prepared != nullptr,
        "correcting a source BPM rebuilds its mapping and pitch-preserved PCM even when the mix tempo map is unchanged");
    auto pickup = layeredRhythm(120, 0.213, 40, false, false);
    // Remove the repeated bar accent, then add one loud pickup only.
    for (int n = 0; 0.213 + n * 0.5 < 39.8; n += 4)
        for (int ch = 0; ch < 2; ++ch) pickup.applyGain(ch, (int) std::round((0.213 + n * 0.5) * sampleRate), (int) (0.12 * sampleRate), 0.45f / 0.65f);
    for (int ch = 0; ch < 2; ++ch) pickup.applyGain(ch, (int) std::round(0.713 * sampleRate), (int) (0.12 * sampleRate), 8.0f);
    check(analyseMusic(pickup, sampleRate).downbeatOrigin < 0, "one loud pickup cannot establish a recurring first downbeat");
    // Equal four-on-the-floor kicks, with a bright phrase entry every four bars.
    // The first phrase cue is deliberately later than the first downbeat, and
    // two quiet pickup beats may precede bar one. Ground truth comes from this
    // score, independently of the detector's feature thresholds.
    auto phraseRhythm = [&](int phase, double intro, int entries, bool misleadingPickup, bool conflictingAccent)
    {
        auto audio = layeredRhythm(120, intro, 84, false, false);
        if (!conflictingAccent)
            for (int n = 0; intro + n * 0.5 < 84; n += 4)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const int at = (int) std::round((intro + n * 0.5) * sampleRate);
                    audio.applyGain(ch, at, std::min((int) (0.12 * sampleRate), audio.getNumSamples() - at), 0.45f / 0.65f);
                }
        juce::Random random(9131);
        auto entry = [&](double at, float gain)
        {
            for (int i = 0; i < (int) (0.35 * sampleRate); ++i)
            {
                const int index = (int) std::round(at * sampleRate) + i;
                if (index >= audio.getNumSamples()) break;
                const double t = i / sampleRate;
                const float value = gain * (float) ((std::sin(2 * pi * 880 * t) + std::sin(2 * pi * 1320 * t)
                    + random.nextFloat() - 0.5) * std::exp(-t * 18));
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, index, value);
            }
        };
        for (int n = 0; n < entries; ++n) entry(intro + (phase + 16 * (n + 1)) * 0.5, 0.65f);
        if (misleadingPickup) entry(intro + ((phase + 1) % 4) * 0.5, 2.5f);
        return audio;
    };
    for (int phase = 0; phase < 4; ++phase)
    {
        const double intro = phase == 2 ? 9.213 : 0.213, expected = intro + phase * 0.5;
        auto audio = phraseRhythm(phase, intro, 8, true, false);
        if (phase == 3) { audio.applyGain(0.08f); audio.applyGain(1, 0, audio.getNumSamples(), -1.0f); }
        const auto analysis = analyseMusic(audio, sampleRate);
        check(std::abs(analysis.downbeatOrigin - expected) < 0.025 && analysis.downbeatConfidence > 0
              && std::abs(analysis.downbeatExaminedSeconds - 84) < 0.001,
              "phrase changes identify phase " + juce::String(phase) + " at its first audible downbeat despite an isolated louder pickup: "
              + juce::String(analysis.downbeatOrigin, 4) + " s, expected " + juce::String(expected, 4));
    }
    check(analyseMusic(phraseRhythm(2, 0.213, 2, false, false), sampleRate).downbeatOrigin < 0,
          "only two isolated phrase accents remain insufficient to establish bar one");
    check(analyseMusic(phraseRhythm(2, 0.213, 8, false, true), sampleRate).downbeatOrigin < 0,
          "conflicting recurring bass accents and phrase changes retain an unknown downbeat");
    auto quietIntro = phraseRhythm(2, 0.213, 8, false, false);
    for (int ch = 0; ch < 2; ++ch) quietIntro.applyGain(ch, 0, (int) ((0.213 + 8) * sampleRate), 0.04f);
    const auto quietAnalysis = analyseMusic(quietIntro, sampleRate);
    check(std::abs(quietAnalysis.downbeatOrigin - 1.213) < 0.025,
          "quiet opening downbeat is retained before the later full-level drums: " + juce::String(quietAnalysis.downbeatOrigin, 4));
    // A regular treble fill occurs more often than the real phrase entry. It
    // changes the hat pattern, while the scored section enters on another beat.
    for (int phase = 0; phase < 4; ++phase)
    {
        auto audio = phraseRhythm(phase, 0.213, 8, false, false);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const int beat = (int) std::floor((i / sampleRate - 0.213) / 0.5) - phase;
            const float level = ((beat % 32 + 32) % 32) < 16 ? 1.0f : 0.2f;
            for (int ch = 0; ch < 2; ++ch) audio.setSample(ch, i, audio.getSample(ch, i) * level);
        }
        juce::Random random(4102);
        for (int n = (phase + 1) % 4 + 8; 0.213 + n * 0.5 < 83.5; n += 8)
            for (int i = 0; i < (int) (0.18 * sampleRate); ++i)
            {
                const int at = (int) std::round((0.213 + n * 0.5) * sampleRate) + i;
                const double t = i / sampleRate;
                const float v = (float) (0.4 * std::exp(-t * 12) * (std::sin(2 * pi * 3100 * t) + 0.2 * (random.nextFloat() - 0.5)));
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, at, v);
            }
        const auto detected = analyseMusic(audio, sampleRate);
        check(std::abs(detected.downbeatOrigin - (0.213 + phase * 0.5)) < 0.025,
              "recurring treble fills do not outweigh the scored phrase entries for phase " + juce::String(phase)
              + ": " + juce::String(detected.downbeatOrigin, 4) + ", BPM " + juce::String(detected.bpm, 4));
    }
    // Score-grounded counterexample: a drumless breakdown repeats a slow
    // melodic gesture halfway through the bar more often than the real phrase
    // entries occur. Spectral change alone confuses this with bar one.
    for (int phase = 0; phase < 4; ++phase)
    {
        const double origin = 0.213, expected = origin + phase * 0.5;
        auto audio = phraseRhythm(phase, origin, 9, false, false);
        const double begin = expected + 24, end = expected + 56;
        audio.clear((int) std::round(begin * sampleRate), (int) std::round((end - begin) * sampleRate));
        for (double at = begin + 1; at < end; at += 4)
            for (int i = 0; i < (int) (0.9 * sampleRate); ++i)
            {
                const int index = (int) std::round(at * sampleRate) + i;
                const double t = i / sampleRate;
                const float value = (float) (0.9 * std::min(1.0, t / 0.2) * std::exp(-t * 4)
                    * (std::sin(2 * pi * 880 * t) + std::sin(2 * pi * 1320 * t)));
                for (int ch = 0; ch < 2; ++ch) audio.addSample(ch, index, value);
            }
        if (phase == 3) audio.applyGain(0.08f);
        const auto analysis = analyseMusic(audio, sampleRate);
        check(std::abs(analysis.downbeatOrigin - expected) < 0.025,
              "drumless melodic repetitions do not displace phase " + juce::String(phase)
              + " first downbeat: " + juce::String(analysis.downbeatOrigin, 4) + " s, expected " + juce::String(expected, 4));
    }
    // Test the actual two-source mixer output, not only matching numeric BPMs.
    for (double target : {96.0, 125.0, 150.0})
    {
        Project project; project.masterDb = 0;
        for (int track = 0; track < 2; ++track)
        {
            auto media = std::make_shared<Media>(); media->sampleRate = sampleRate;
            media->manualBpm = track == 0 ? 120 : 128; media->manualBeatOrigin = 0.213;
            media->samples = layeredRhythm(media->manualBpm, media->manualBeatOrigin, 48, false, false);
            Clip clip; clip.media = media; clip.track = track; clip.length = media->duration(); clip.beatmatched = true;
            project.clips.push_back(clip);
        }
        auto prepared = prepareBeatmatch(project, target, true, 32);
        check(prepared.error.isEmpty(), "prepare two-source " + juce::String(target) + " BPM: " + prepared.error);
        if (prepared.error.isNotEmpty()) continue;
        double worst = 0, separation = 0; int compared = 0;
        Engine engine; engine.sampleRate = sampleRate; engine.project = prepared.project; juce::MidiBuffer midi;
        juce::AudioBuffer<float> pcm(2, 512);
        const double begin = engine.project.clips[1].start + engine.project.clips[1].firstBeatTime();
        for (double at = begin + 60 / target; at < engine.project.clips[0].end() - 60 / target; at += 60 / target)
        {
            double found[2] {};
            for (int track = 0; track < 2; ++track)
            {
                engine.project.tracks[0].mute = track != 0; engine.project.tracks[1].mute = track != 1;
                // Peak attack slope in a known kick; compare with its unprocessed source slope.
                double best = 0, previous = 0;
                const int frames = (int) (0.12 * sampleRate);
                for (int base = 0; base < frames; base += 512)
                {
                    const int count = std::min(512, frames - base); engine.render(pcm, count, at - 0.06 + base / sampleRate, midi);
                    for (int i = 0; i < count; ++i)
                    {
                        const double value = pcm.getSample(0, i), rise = std::abs(value - previous); previous = value;
                        if (rise > best) { best = rise; found[track] = at - 0.06 + (base + i) / sampleRate; }
                    }
                }
                worst = std::max(worst, std::abs(found[track] - at));
            }
            separation = std::max(separation, std::abs(found[0] - found[1])); ++compared;
        }
        check(compared > 8 && worst < 0.020 && separation < 0.020,
              "audible two-source attacks at " + juce::String(target) + " BPM: grid " + juce::String(worst * 1000, 2)
              + " ms, separation " + juce::String(separation * 1000, 2) + " ms");
    }
    Project drops;
    for (int track = 0; track < 2; ++track)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = sampleRate;
        media->manualBpm = track == 0 ? 120 : 128; media->manualBeatOrigin = 0.213;
        media->samples = layeredRhythm(media->manualBpm, 0.213, 40, false, false);
        Clip clip; clip.media = media; clip.track = track; clip.length = 40;
        clip.start = track == 0 ? 0 : 24.125; clip.pendingPlacement = PendingPlacement::drop;
        drops.clips.push_back(clip);
    }
    auto matched = prepareTempoMap(drops, false, 32);
    const auto& map = matched.project.tempo;
    check(matched.error.isEmpty() && matched.matched == 2 && map.points.size() == 2 && map.bpmAt(0) == 120
          && map.points[1].bpm == 128 && map.points[1].clipId == matched.project.clips[1].id
          && std::abs(map.points[1].time - (matched.project.clips[1].start + matched.project.clips[1].transitionTime(true))) < 1.0e-7,
          "dropped tracks expose one tempo target each, reaching the incoming native tempo at IN");
    for (const auto& clip : matched.project.clips)
        check(clip.beatmatched && clip.pendingPlacement == PendingPlacement::none
              && std::abs(std::remainder(map.beatsAt(clip.start + clip.firstDownbeatTime()), 4)) < 1.0e-7,
              "dropped source " + juce::String(clip.track) + " downbeat aligns to the integrated changing-tempo grid");
    if (map.points.size() == 2)
    {
        auto edit = matched.project; edit.tempo.movePoint(0, 0, 122);
        edit.tempo.movePoint(1, map.points[1].time + 3, 132); const auto wanted = edit.tempo.points;
        auto adjusted = prepareTempoMap(edit, false, 32);
        check(adjusted.error.isEmpty() && adjusted.project.tempo.points == wanted
              && adjusted.project.clips[1].prepared != matched.project.clips[1].prepared
              && std::abs(std::remainder(adjusted.project.tempo.beatsAt(adjusted.project.clips[1].start + adjusted.project.clips[1].firstDownbeatTime()), 4)) < 1.0e-7,
              "editing the starting and incoming targets rebuilds PCM while retaining beat alignment and the user's tempo map");
    }
    for (auto& clip : drops.clips) clip.pendingPlacement = PendingPlacement::append;
    const auto cut = prepareTempoMap(drops, false, 0);
    const double cutGap = cut.project.clips[1].start - cut.project.clips[0].end();
    check(cut.error.isEmpty() && cutGap >= -1.0e-7 && cutGap < 60 / drops.clips[0].media->bpm()
          && cut.project.clips[0].fadeOut == 0 && cut.project.clips[1].fadeIn == 0,
          "an explicit Cut aligns to the next pulse without introducing a whole-bar gap");
    juce::ignoreUnused(report);
}
}

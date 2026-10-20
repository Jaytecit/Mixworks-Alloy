#include "RhythmDecoder.h"
#include "AuftaktFeatures.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runRhythmDecoderTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    runAuftaktFeatureTests(check);
    auto observations = [](int meter, bool change, std::vector<double>& truth)
    {
        RhythmActivations data; data.beatLogits.assign(3000, -9); data.downbeatLogits.assign(3000, -9);
        int ordinal = 0;
        for (int frame = 25; frame < 2950; frame += change && ordinal >= 60 ? 20 : 25, ++ordinal)
        {
            truth.push_back(frame / 50.0); data.beatLogits[(size_t) frame] = 9;
            if (ordinal % meter == 0) data.downbeatLogits[(size_t) frame] = 9;
        }
        return data;
    };
    for (auto meter : {3, 4}) for (bool change : {false, true})
    {
        std::vector<double> truth; const auto data = observations(meter, change, truth); const auto decoded = decodeRhythm(data);
        size_t matches = 0, correctBars = 0; double worst = 0;
        for (size_t i = 0; i < truth.size(); ++i)
        {
            auto found = std::min_element(decoded.beats.begin(), decoded.beats.end(), [&](const auto& a, const auto& b) { return std::abs(a.seconds - truth[i]) < std::abs(b.seconds - truth[i]); });
            if (found == decoded.beats.end()) continue;
            const double error = std::abs(found->seconds - truth[i]); worst = std::max(worst, error);
            if (error < 0.021) { ++matches; if (found->meter == meter && found->position == (int) (i % (size_t) meter) + 1) ++correctBars; }
        }
        const auto label = juce::String(meter) + "/4 " + (change ? "tempo step" : "steady");
        check(decoded.error.isEmpty() && matches == truth.size() && worst < 0.021, label + " retains every observed beat within one model frame");
        check(correctBars == truth.size(), label + " recovers meter and beat position");
        check(decoded.beats.size() == truth.size(), label + " adds no unsupported opening or tail beats");
    }
    RhythmActivations fast; fast.beatLogits.assign(2000, -9); fast.downbeatLogits.assign(2000, -9);
    int fastCount = 0;
    for (int beat = 0; beat < 152; ++beat)
    {
        const auto frame = (size_t) std::llround(25 + beat * 12.5); fast.beatLogits[frame] = 9;
        if (beat % 4 == 0) fast.downbeatLogits[frame] = 9;
        ++fastCount;
    }
    const auto fastResult = decodeRhythm(fast);
    check(fastResult.error.isEmpty() && (int) fastResult.beats.size() == fastCount
          && std::all_of(fastResult.beats.begin(), fastResult.beats.end(), [](const auto& b) { return b.beatActivation > 0.99f; }),
          "240 BPM boundary retains alternating 12/13-frame intervals without losing or inventing beats; observed "
          + juce::String((int) fastResult.beats.size()) + ", supported "
          + juce::String((int) std::count_if(fastResult.beats.begin(), fastResult.beats.end(), [](const auto& b) { return b.beatActivation > 0.99f; })));
    std::vector<double> truth; auto data = observations(4, false, truth);
    // A quiet missing pulse must not restart a bar or shift subsequent events.
    data.beatLogits[1025] = -9;
    const auto bridged = decodeRhythm(data);
    check(bridged.error.isEmpty() && std::any_of(bridged.beats.begin(), bridged.beats.end(), [](const auto& b) { return std::abs(b.seconds - 20.5) < 0.001 && b.position == 1; }), "state sequence bridges a missing beat without moving beat one");
    RhythmActivations silent; silent.beatLogits.assign(1000, -9); silent.downbeatLogits.assign(1000, -9);
    check(decodeRhythm(silent).beats.empty() && decodeRhythm(silent).error.isEmpty(), "silence remains without a grid");
    auto invalid = data; invalid.downbeatLogits.pop_back();
    check(decodeRhythm(invalid).error.isNotEmpty(), "unequal observation channels rejected");
    invalid = data; invalid.beatLogits[2] = std::numeric_limits<float>::quiet_NaN();
    check(decodeRhythm(invalid).error.isNotEmpty() && decodeRhythm(invalid).beats.empty(), "non-finite observations return no partial grid");
    RhythmDecoderSettings settings; settings.meters = {4, 4};
    check(decodeRhythm(data, settings).error.isNotEmpty(), "duplicate meter hypotheses rejected");
    settings = {}; settings.meters = {0}; check(decodeRhythm(data, settings).error.isNotEmpty(), "invalid meter rejected");
    settings = {}; settings.maximumTraceBytes = 8;
    check(decodeRhythm(data, settings).error.isNotEmpty(), "trace memory budget checked before allocation");
    invalid = data; invalid.framesPerSecond = 1.0e100;
    check(decodeRhythm(invalid).error.isNotEmpty(), "unrepresentable state dimensions rejected before conversion");
    settings = {}; settings.minimumBpm = 241;
    check(decodeRhythm(data, settings).error.isNotEmpty(), "inverted tempo bounds rejected");
    settings = {}; settings.tempoChangeCost = std::numeric_limits<double>::infinity();
    check(decodeRhythm(data, settings).error.isNotEmpty(), "non-finite transition costs rejected");
    check(decodeRhythm({}).beats.empty(), "empty observations remain empty");
    RhythmActivations classes; classes.framesPerSecond = 100;
    classes.classProbabilities.assign(2200, {0.00001f, 0.00001f, 0.99998f});
    for (size_t i = 0; i < 40; ++i) classes.classProbabilities[50 + i * 50] = i % 4 == 0
        ? std::array<float, 3>{0.00001f, 0.99998f, 0.00001f} : std::array<float, 3>{0.99998f, 0.00001f, 0.00001f};
    const auto categorical = decodeRhythm(classes);
    check(categorical.error.isEmpty() && categorical.beats.size() == 40
          && categorical.beats.front().position == 1 && categorical.beats.front().meter == 4,
          "categorical ordinary beat/downbeat/neither observations retain all beats and bar phase");
    RhythmDecoderSettings recovered; recovered.recoveredAuftaktTopology = true;
    // Fifty recovered logarithmic proposals include 49/51 frames, not 50.
    // A PCM frontend spreads an attack across adjacent frames. Keep the separate
    // independent decoder's exact single-frame pulse tests above unchanged.
    for (size_t i = 0; i < 40; ++i) for (int shift : {-1, 1})
        classes.classProbabilities[50 + i * 50 + shift] = i % 4 == 0
            ? std::array<float, 3>{0.00001f, 0.89998f, 0.10001f} : std::array<float, 3>{0.89998f, 0.00001f, 0.10001f};
    classes.signalPresent = true;
    const auto recoveredResult = decodeRhythm(classes, recovered);
    check(recoveredResult.error.isEmpty()
          && std::all_of(recoveredResult.beats.begin(), recoveredResult.beats.end(), [](const auto& event) { return event.meter == 4; })
          && std::count_if(recoveredResult.beats.begin(), recoveredResult.beats.end(), [](const auto& event)
             { return event.seconds >= .479 && event.seconds <= 20.021
                   && std::abs(std::remainder(event.seconds - .5, .5)) <= .021; }) == 40,
          "recovered tempo proposals and transition factors retain constructed pulses within two frontend hops; events "
          + juce::String((int) recoveredResult.beats.size()) + ", error " + recoveredResult.error
          + ", first meter " + juce::String(recoveredResult.beats.empty() ? 0 : recoveredResult.beats.front().meter));
    classes.signalPresent = false;
    check(decodeRhythm(classes, recovered).beats.empty(), "known empty PCM feature support suppresses model boundary hallucinations");
    classes.signalPresent.reset();
    RhythmActivations quietEdges; quietEdges.framesPerSecond = 100; quietEdges.signalPresent = true;
    quietEdges.classProbabilities.assign(400, {0.1f, 0.1f, 0.8f});
    const auto fullDuration = decodeRhythm(quietEdges, recovered);
    check(fullDuration.error.isEmpty() && !fullDuration.beats.empty()
          && fullDuration.beats.front().seconds < .5 && fullDuration.beats.back().seconds > 3.5,
          "recovered decoding retains low-probability quiet boundaries when the producer measured a signal");
    RhythmActivations impossible; impossible.framesPerSecond = 100;
    impossible.classProbabilities.assign(3, {0, 1, 0});
    check(decodeRhythm(impossible, recovered).error.isNotEmpty(),
          "exact zero categorical probabilities remain impossible instead of being clipped into a fabricated path");
    classes.classProbabilities[0] = {1, 1, 1};
    check(decodeRhythm(classes).error.isNotEmpty(), "unnormalized categorical observations rejected");
    classes.classProbabilities[0] = {0, 0, 1}; classes.beatLogits = {0}; classes.downbeatLogits = {0};
    check(decodeRhythm(classes).error.isNotEmpty(), "mixed categorical and independent-logit representations rejected");
    juce::var summary(new juce::DynamicObject()); summary.getDynamicObject()->setProperty("scope", "synthetic activations; no vendor equivalence or real-music accuracy claim");
    summary.getDynamicObject()->setProperty("decoder", "alloy-state-sequence-research-1");
    check(report.getSiblingFile("research-scope.json").replaceWithText(juce::JSON::toString(summary)), "research scope retained with native results");
}
}

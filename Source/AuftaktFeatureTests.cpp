#include "AuftaktNetwork.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runAuftaktFeatureTests(const std::function<void(bool, const juce::String&)>& check)
{
    juce::AudioBuffer<float> pcm(2, 4410); pcm.clear();
    const auto silent = extractAuftaktFeatures(pcm, 44100);
    check(silent.error.isEmpty() && !silent.signalPresent && silent.frames() == 10 && silent.values.size() == 3200
          && std::all_of(silent.values.begin(), silent.values.end(), [](float v) { return v == 0; }),
          "recovered frontend silence has ten source-centred rows and no spectral activity");
    pcm.setSample(0, 0, 1); pcm.setSample(1, 0, 1);
    const auto impulse = extractAuftaktFeatures(pcm, 44100);
    // First band is the recovered single-bin unit weight. DFT magnitude for an
    // impulse is exactly its window coefficient, independent of FFT algorithm.
    const float coefficient = 0.5f * (1 - std::cos(513.0f * 6.2831855f / 1025.0f));
    check(impulse.error.isEmpty() && impulse.signalPresent && std::abs(impulse.values[0] - std::log(1 + coefficient) * 0.4342944920063019f) < 1e-6,
          "recovered first filter/window/compression matches analytic impulse DFT");
    check(impulse.seconds(9) == 0.09 && impulse.values[22] == 0 && impulse.values[136 + 92] == 0,
          "source timestamps and initial temporal-difference history are retained");
    pcm.setSample(1, 0, -1);
    const auto cancelled = extractAuftaktFeatures(pcm, 44100);
    check(cancelled.error.isEmpty() && cancelled.values == silent.values,
          "recovered arithmetic stereo mean preserves antiphase cancellation");
    pcm.clear();
    for (int i = 0; i < pcm.getNumSamples(); ++i)
    { const float value = 0.2f * std::sin((float) i * 0.071f); pcm.setSample(0, i, value); pcm.setSample(1, i, value); }
    const auto tone = extractAuftaktFeatures(pcm, 44100);
    bool differences = tone.error.isEmpty();
    int column = 0;
    for (auto group : {std::pair<int, int>{22, 1}, {46, 1}, {92, 2}})
    {
        for (size_t f = 0; f < tone.frames(); ++f) for (int b = 0; b < group.first; ++b)
        {
            const auto row = f * 320 + column + b;
            const auto expected = f >= (size_t) group.second ? std::max(0.0f, tone.values[row] - tone.values[row - group.second * 320]) : 0;
            differences = differences && tone.values[row + group.first] == expected;
        }
        column += group.first * 2;
    }
    check(differences, "recovered 10/10/20 ms positive differences preserve resolution-specific history");
    const auto resampled = extractAuftaktFeatures(pcm, 48000);
    check(resampled.error.isEmpty() && resampled.frames() == 10, "48 kHz frontend conversion retains source duration");
    check(extractAuftaktFeatures(pcm, 44100, 100).error.isNotEmpty(), "expanded frontend budget checked before allocation");
    pcm.setSample(0, 99, std::numeric_limits<float>::quiet_NaN());
    const auto invalid = extractAuftaktFeatures(pcm, 44100);
    check(invalid.error.isNotEmpty() && invalid.values.empty(), "non-finite PCM returns no partial feature matrix");
    check(extractAuftaktFeatures(pcm, std::numeric_limits<double>::infinity()).error.isNotEmpty(), "non-finite frontend rate rejected");
    check(extractAuftaktFeatures({}, 44100).error.isNotEmpty(), "empty frontend PCM rejected");
    auto malformed = silent; malformed.values.pop_back();
    const juce::File missing = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("alloy-absent-model", ".json");
    check(evaluateAuftaktNetwork(malformed, missing).error.isNotEmpty(), "network rejects partial feature rows before model loading");
    check(evaluateAuftaktNetwork(silent, missing).error.isNotEmpty(), "recovered model absence returns explicit error without substitution");
    check(evaluateAuftaktNetwork(silent, missing, 1).error.isNotEmpty(), "network intermediate expansion bound checked before model loading");
}
}

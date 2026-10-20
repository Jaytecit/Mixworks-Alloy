#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <optional>

namespace mixworks
{
struct RhythmActivations
{
    double framesPerSecond = 50;
    std::vector<float> beatLogits, downbeatLogits;
    // Alternative observation representation, mutually exclusive with logits:
    // recovered AUFTAKT classes [ordinary beat, downbeat, neither].
    std::vector<std::array<float, 3>> classProbabilities;
    std::optional<bool> signalPresent; // Producer-owned silence observation; absent means unmeasured.
};
struct RhythmDecoderSettings
{
    double minimumBpm = 60, maximumBpm = 240, tempoChangeCost = 20;
    std::vector<int> meters {3, 4};
    size_t maximumTraceBytes = 128 * 1024 * 1024;
    double nonbeatObservationDivisor = 39; // Recovered observation model: 1/(40-1).
    bool recoveredAuftaktTopology = false; // 50 log-spaced tempo proposals; recovered transition factors.
};
struct DecodedBeat
{
    double seconds = 0;
    int position = 0, meter = 0; // One-based position; retain meter on every event.
    float beatActivation = 0, downbeatActivation = 0; // Model activations, not accuracy probabilities.
};
struct DecodedRhythm
{
    std::vector<DecodedBeat> beats;
    std::vector<std::pair<int, double>> meterScores;
    juce::String error;
};
// Experimental max-product/backtrace decoder. The default Beat This route retains
// its independent topology. Categorical AUFTAKT mode uses the recovered state and
// transition construction; finite-file admission/tie/float equivalence is unverified.
DecodedRhythm decodeRhythm(const RhythmActivations&, const RhythmDecoderSettings& = {});
void runRhythmDecoderTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
int runRhythmResearch(const juce::StringArray&);
}

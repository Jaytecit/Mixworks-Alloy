#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace mixworks
{
// Reconstructed PCM frontend. This contains no vendor classifier or model weights.
// Rows retain the source timebase; consumers must not infer bar identity from them.
struct AuftaktFeatures
{
    static constexpr int sampleRate = 44100, hop = 441, dimensions = 320;
    std::vector<float> values; // Frame-major: each resolution's levels, then positive differences.
    bool signalPresent = false; // Measured spectral activity; quiet edges do not delimit decoding.
    juce::String error;
    size_t frames() const { return values.size() / dimensions; }
    double seconds(size_t frame) const { return frame * (double) hop / sampleRate; }
};
AuftaktFeatures extractAuftaktFeatures(const juce::AudioBuffer<float>&, double sampleRate,
                                     size_t maximumBytes = 256 * 1024 * 1024);
void runAuftaktFeatureTests(const std::function<void(bool, const juce::String&)>&);
}

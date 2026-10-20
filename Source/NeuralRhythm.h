#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "RhythmDecoder.h"
#include "PcmView.h"
#include <atomic>

namespace mixworks
{
struct NeuralRhythm
{
    std::vector<double> beats, downbeats;
    std::vector<float> beatConfidence, downbeatConfidence;
    juce::String error;
};
// Owns the pinned local model, signal conversion, and model output validation.
// No source paths, metadata, caches or comparison labels enter this boundary.
NeuralRhythm analyseNeuralRhythm(const PcmView&, double rate, RhythmActivations* research = nullptr, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
}

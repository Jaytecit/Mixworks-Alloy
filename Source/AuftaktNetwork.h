#pragma once
#include "AuftaktFeatures.h"
#include <array>

namespace mixworks
{
struct AuftaktObservations
{
    // Recovered class order: ordinary beat, downbeat, neither.
    std::vector<std::array<float, 3>> probabilities;
    double packagedTestMaximumError = -1;
    juce::String error;
};
// Explicit, pinned local model carrier. No implicit download, adjacent-model
// substitution, analysis cache access, or dependency on the Beat This model.
AuftaktObservations evaluateAuftaktNetwork(const AuftaktFeatures&, const juce::File& model,
                                         size_t maximumBytes = 256 * 1024 * 1024);
}

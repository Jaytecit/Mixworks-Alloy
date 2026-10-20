#pragma once
#include "Model.h"

namespace mixworks
{
constexpr double autoArrangementMaxSeconds = 3 * 60 * 60;
enum class AutoTransitionStyle { balanced, smooth, bold, current, constantPower, hardCut };
enum class AutoEnergyPolicy { unrestricted, maintain, build, windDown };
struct AutoArrangementOptions
{
    juce::String startingId; // Empty selects a random eligible source.
    int trackCount = 10;
    std::optional<double> targetSeconds;
    TransitionPreset preset = TransitionPreset::crossfade;
    double overlapBeats = 128;
    bool sectionTransitions = true;
    AutoTransitionStyle transitions = AutoTransitionStyle::balanced;
    AutoEnergyPolicy energy = AutoEnergyPolicy::unrestricted;
};
struct AutoArrangementResult
{
    Project project;
    juce::String error, summary, stopReason;
    int eligibleTracks = 0;
};
bool autoArrangementEligible(const Media&);
AutoArrangementResult planAutoArrangement(const std::vector<std::shared_ptr<Media>>&, const AutoArrangementOptions&,
                                         juce::Random&, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
}

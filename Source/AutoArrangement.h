#pragma once
#include "Model.h"

namespace mixworks
{
struct MatchResult;
MatchResult prepareOrderedTimeline(Project, int64_t existingResidentBytes = 0);
constexpr double autoArrangementMaxSeconds = 3 * 60 * 60;
enum class AutoTransitionStyle { balanced, smooth, bold, current, constantPower, hardCut, varied };
enum class AutoEnergyPolicy { unrestricted, maintain, build, windDown };
struct AutoArrangementOptions
{
    juce::String startingId; // Empty: random when unrestricted, direction-aware otherwise.
    int trackCount = 10;
    int laneCount = mixworks::trackCount;
    std::optional<double> targetSeconds;
    TransitionPreset preset = TransitionPreset::crossfade;
    double overlapBeats = 128;
    bool sectionTransitions = true;
    AutoTransitionStyle transitions = AutoTransitionStyle::balanced;
    AutoEnergyPolicy energy = AutoEnergyPolicy::unrestricted;
    juce::String preferredGenre; // Soft preference; empty accepts any tag.
    bool extendExisting = false; // Count/duration describe additional material.
    bool createVisualScenes = false; // Explicit replacement, after the final music timeline is prepared.
    VisualSceneSource visualSceneSource = VisualSceneSource::originals;
};
struct AutoArrangementResult
{
    Project project;
    juce::String error, summary, stopReason;
    int eligibleTracks = 0;
    juce::StringArray decisions;
    juce::String completionText(AutoTransitionStyle) const;
};
// Metadata-only timeline command. Retains order and authored source controls;
// shares pair placement with library Auto Arrange. Audio refinement follows load.
AutoArrangementResult planTimelineBlends(Project, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
juce::String normalizedAutoGenre(const juce::String&);
bool autoArrangementEligible(const Media&);
AutoArrangementResult planAutoArrangement(const std::vector<std::shared_ptr<Media>>&, const AutoArrangementOptions&,
                                         juce::Random&, const std::shared_ptr<std::atomic<bool>>& cancelled = {}, const Project* existing = nullptr);
void applyAutoArrangementBass(AutoArrangementResult&, AutoTransitionStyle, size_t firstNewClip,
                              juce::AudioFormatManager&, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
}

#pragma once
#include "Model.h"

namespace mixworks
{
constexpr int64_t maxPreparedBytes = 1024LL * 1024 * 1024; // Resident PCM, including the next allocation; also the single-preparation cap.
struct StretchResult { std::shared_ptr<const PreparedAudio> audio; juce::String error; };
StretchResult stretchAudio(const Media&, double rate, int64_t availableBytes);
struct MatchResult { Project project; juce::String error; int matched = 0, skipped = 0; };
MatchResult prepareBeatmatch(Project project, double bpm, bool align, double overlapBeats, int64_t existingResidentBytes = 0);
MatchResult prepareTempoMap(Project project, bool align, double overlapBeats, int64_t existingResidentBytes = 0, size_t alignFrom = 0, std::optional<double> livePosition = {}, TransitionPreset preset = TransitionPreset::bassSwap, bool sectionTransitions = false);
// Same placement/tempo/transition owner, without decoding or preparing playback.
MatchResult planTempoMap(Project, double overlapBeats, TransitionPreset, bool sectionTransitions, double maxSectionBeats = 256);
juce::String prepareSavedClips(Project&);
}

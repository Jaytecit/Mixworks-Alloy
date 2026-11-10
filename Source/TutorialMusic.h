#pragma once
#include "AnalysisCache.h"

namespace mixworks
{
struct TutorialMusicResult
{
    juce::Array<juce::File> files;
    juce::String error;
};
// Original, locally synthesised examples. Never adopts or overwrites unknown
// files. Cancellation only removes files created inside this invocation's stage.
TutorialMusicResult createTutorialMusic(const juce::File& destination,
                                       const std::shared_ptr<std::atomic<bool>>& cancelled);
}

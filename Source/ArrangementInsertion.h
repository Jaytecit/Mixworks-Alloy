#pragma once
#include "Model.h"

namespace mixworks
{
struct ArrangementInsertion
{
    Project project;
    juce::String error, inserted, outgoing, incoming;
    double suffixStart = 0, displacement = 0, sourceEnd = 0;
    int moved = 0;
};
// Pure arrangement planning. Hydrate the new source first; no existing source,
// prepared payload, runtime effect or catalogue object is modified here.
ArrangementInsertion planArrangementInsertion(const Project&, std::shared_ptr<Media>, const juce::String& anchor,
    bool after, double overlapBeats, TransitionPreset);
juce::String prepareArrangementInsertion(ArrangementInsertion&, int64_t existingResidentBytes = 0);
}

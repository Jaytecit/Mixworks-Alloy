#pragma once
#include "AnalysisCache.h"

namespace mixworks
{
// Review evidence is separate from replaceable detector/cache records. Each
// immutable observation records the user's choice, not successful application.
juce::File downbeatReferenceRoot();
juce::Result writeDownbeatReference(const juce::File& audio, const TrackAnalysisRecord&, double previousManualAnchor);
}

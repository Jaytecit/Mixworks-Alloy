#pragma once
#include "Model.h"

namespace mixworks
{
// Source seconds, derived only from retained waveform RMS and the source bar grid.
// These are energy-change suggestions, not semantic or independently verified phrases.
struct EnergySection { double sourceSeconds = 0, contrastDb = 0; };
std::vector<EnergySection> detectEnergySections(const Media&);
struct SectionTransitionChoice { double overlapBeats = 32; int matchedSections = 0; };
std::optional<SectionTransitionChoice> chooseSectionTransition(const Clip& outgoing, const Clip& incoming, double maxBeats = 256);
// Mean-square energy over an exact source interval; incomplete/malformed displays stay unknown.
std::optional<double> sourceEnergy(const Media&, double begin, double end);
void paintEnergySections(juce::Graphics&, const Clip&, juce::Rectangle<float>, double pixelsPerSecond);
}

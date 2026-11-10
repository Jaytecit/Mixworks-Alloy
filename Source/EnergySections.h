#pragma once
#include "Model.h"

namespace mixworks
{
// Source seconds, derived only from retained waveform RMS and the source bar grid.
// These are energy-change suggestions, not semantic or independently verified phrases.
struct EnergySection { double sourceSeconds = 0, contrastDb = 0; juce::String id; bool manual = false; juce::String label; };
std::vector<EnergySection> detectEnergySections(const Media&);
std::vector<EnergySection> clipEnergySections(const Clip&);
struct SectionTransitionChoice
{
    double overlapBeats = 32; int matchedSections = 0;
    std::optional<std::array<double, 2>> boundaries; // Outgoing/incoming source seconds, not file edges.
};
std::optional<SectionTransitionChoice> chooseSectionTransition(const Clip& outgoing, const Clip& incoming, double maxBeats = 256);
std::optional<juce::Range<double>> sectionTransitionWindow(const Clip&, const Clip&, const TempoMap&);
double sectionSwapPosition(const Project&, const Project::TransitionLink&); // Beat-relative; .5 only without a shared change.
double alignedSectionCut(const Project&, const Project::TransitionLink&);
// Mean-square energy over an exact source interval; incomplete/malformed displays stay unknown.
std::optional<double> sourceEnergy(const Media&, double begin, double end);
void paintEnergySections(juce::Graphics&, const Clip&, juce::Rectangle<float>, double pixelsPerSecond);
}

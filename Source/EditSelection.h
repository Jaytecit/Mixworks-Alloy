#pragma once
#include "Model.h"

namespace mixworks
{
// Editor state only. Never part of Project, musical history or session JSON.
struct EditSelection
{
    juce::String primary;
    std::vector<juce::String> clips;
    std::optional<juce::Range<double>> range;
    bool contains(const juce::String& id) const
    { return std::find(clips.begin(), clips.end(), id) != clips.end(); }
    bool multiple() const { return clips.size() > 1 || range.has_value(); }
    void clear() { primary.clear(); clips.clear(); range.reset(); }
};
struct EditFragment
{
    std::vector<Clip> clips;
    std::vector<Project::TransitionLink> links;
    double origin = 0;
    int lane = 0;
};
}

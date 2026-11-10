#pragma once
#include "Model.h"

namespace mixworks
{
// Procedural descriptions are compiled once, outside rendering. The resulting
// automation is edited/played/saved by the ordinary curve owner.
constexpr unsigned presetNodeBands = 0x2f; // Volume, Bass, Mid, Treble, Filter; Pan is not a generated treatment.
juce::Result materialisePresetNodes(Project&, const juce::String& incoming, unsigned targets = presetNodeBands);
juce::Result editablePresetNodes(const Project&, const AutomationAddress&, AutomationCurve&);
juce::Result materialiseEditablePresetNodes(Project&, const AutomationAddress&);
juce::Result capturePresetNodes(const Project&, const juce::String& incoming, TransitionStyle::Nodes&);
juce::Result applyPresetNodes(Project&, const juce::String& incoming, const TransitionStyle::Nodes&, unsigned targets = 0x3f);
juce::Result applyOwnedPresetNodes(Project&, const juce::String& incoming, const TransitionStyle::Nodes&, const juce::String& name);
juce::Result placeTransitionBandNodes(Project&, const juce::String& incoming, int band, const TransitionBandSettings&);
juce::Result placeTransitionFilterNodes(Project&, const juce::String& incoming, const TransitionFilterSettings&);
juce::var presetNodesToJson(const TransitionStyle::Nodes&);
juce::Result readPresetNodes(const juce::var&, TransitionStyle::Nodes&);
}

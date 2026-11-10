#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixworks
{
juce::String wholeBpmText(double);
juce::String preciseBpmText(double);

// Hover help is a short action cue. Full reports belong in an explicit details view.
juce::String shortUiMessage(const juce::String&, int words = 35, int characters = 240);
void showUiDetails(juce::Component&, const juce::String& title, const juce::String& details);
}

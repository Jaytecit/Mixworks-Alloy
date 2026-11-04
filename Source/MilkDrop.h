#pragma once
#include "VisualScenes.h"

namespace mixworks
{
bool isMilkDropKey(const juce::String&);
bool milkDropRendererAvailable();
juce::File milkDropLibraryRoot();
std::vector<VisualSceneEntry> discoverMilkDropScenes(const juce::File& library);
juce::Result importMilkDropPreset(const juce::File& source, const juce::File& library, juce::String& key);
}

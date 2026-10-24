#pragma once
#include "Model.h"

namespace mixworks
{
void initialiseHotcues(Clip&);
const HotcueMarker* hotcueFor(const Clip&, int slot);
// The next visible loop occurrence at/after nearTime, or the first if none.
// No mutation, no preparation or implicit transport action.
std::optional<double> hotcueTimelineTime(const Clip&, int slot, double nearTime = -1);
juce::Colour hotcueColour(int slot);
}
